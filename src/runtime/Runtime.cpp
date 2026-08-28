#include "runtime/Runtime.h"

#include "api/manifest/ManifestValidator.h"
#include "capability/ICapabilityQuery.h"
#include "component/ComponentRenderer.h"
#include "component/ComponentSpec.h"
#include "diagnostic/CrashProbe.h"
#include "diagnostic/DiagnosticLogger.h"
#include "diagnostic/FileDiagnosticSink.h"
#include "diagnostic/Stage0TelemetryCompat.h"
#include "diagnostic/Stage3PageLifecycleTelemetry.h"
#include "diagnostic/Stage4InjectTelemetry.h"
#include "diagnostic/Stage5IpcTelemetry.h"
#include "facet/FacetHostMethod.h"
#include "facet/RuntimeInfoFacet.h"
#include "hook/OreUIHookAdapter.h"
#include "inject/RuntimeInjector.h"
#include "ipc/CoherentHostBridge.h"
#include "poc/Stage1NavigationPoc.h"
#include "preview/UiAssetExporter.h"
#include "render/DomScriptSerializer.h"
#include "render/HtmlDomParser.h"
#include "resource/ResourceIndex.h"
#include "source/FileSystemSourceReader.h"
#include "ui/MountManager.h"
#include "ui/NullMountHost.h"
#include "ui/UiPlanner.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

namespace dearoreui::runtime {

Runtime::Runtime(RuntimeConfig config) : mConfig(std::move(config)) {}

bool Runtime::initialize() {
    if (mInitialized) return true;

    auto& logger = diagnostic::globalLogger();

    // Optional test switch file <data>/stage8-switch.txt: disable_hooks=1
    // skips every hook install (mod load + crash probe only). Other Stage 8
    // isolation switches were removed after the crash investigation concluded
    // (engine bindings crash the client; JS->C++ moves to a WS loopback).
    auto switchPath = mConfig.dataDirectory / "stage8-switch.txt";
    if (std::filesystem::exists(switchPath)) {
        std::ifstream in(switchPath);
        std::string   line;
        while (std::getline(in, line)) {
            if (line.rfind("disable_hooks=1", 0) == 0) {
                mConfig.enableHooks = false;
            }
            if (line.rfind("demo_overlay=1", 0) == 0) {
                mConfig.enableDemoOverlay = true;
            }
            if (line.rfind("component_showcase=1", 0) == 0) {
                mConfig.enableComponentShowcase = true;
            }
        }
    }
    logger.info("stage8", "switch_loaded")
        .withField("disable_hooks", mConfig.enableHooks ? "0" : "1")
        .withField("demo_overlay", mConfig.enableDemoOverlay ? "1" : "0")
        .withField("component_showcase", mConfig.enableComponentShowcase ? "1" : "0")
        .emit();

    if (mConfig.enableFileDiagnostics) {
        logger.addSink(
            std::make_shared<diagnostic::FileDiagnosticSink>(
                mConfig.dataDirectory / "diagnostics" / "diagnostics.jsonl"
            )
        );
    }

    if (mConfig.enableStage0Compatibility) {
        diagnostic::initializeStage0FileSink(mConfig.dataDirectory);
        diagnostic::startStage0Session();
    }
    diagnostic::initializeStage3FileSink(mConfig.dataDirectory, diagnostic::currentStage0SessionId());
    // Stage 8 crash probe: records the faulting module+RVA on ANY unhandled
    // exception (including cohtml engine threads) to data/crash/crash-last.txt.
    // This is how the page-exit crash gets pinpointed instead of guessed at.
    diagnostic::installCrashProbe(mConfig.dataDirectory);

    mRegistry           = std::make_unique<registry::ModRegistry>();
    mHostMethodRegistry = std::make_unique<ipc::HostMethodRegistry>();
    mFacetRegistry      = std::make_unique<facet::FacetRegistry>();
    mApi                = std::make_unique<api::DearOreUIApi>(*mRegistry, *mHostMethodRegistry, mCapabilities, logger);

    auto runtimeInfoFacet    = std::make_shared<facet::RuntimeInfoFacet>(*mApi);
    auto facetRegisterResult = mFacetRegistry->registerProvider(runtimeInfoFacet);
    if (facetRegisterResult.isErr()) {
        logger.warning("facet", "builtin_registration_failed")
            .withError(facetRegisterResult.error().code)
            .withMessage(facetRegisterResult.error().message)
            .emit();
    }

    auto runtimeInfoMethod = std::make_shared<facet::FacetHostMethod>(runtimeInfoFacet, api::Permission::HostReadOnly);
    api::PermissionSet runtimePermissions{std::vector{api::Permission::HostReadOnly}};
    auto               facetResult =
        mHostMethodRegistry->registerMethod(api::ModId{"dearoreui"}, runtimePermissions, runtimeInfoMethod);
    if (facetResult.isErr()) {
        logger.warning("host", "builtin_facet_registration_failed")
            .withError(facetResult.error().code)
            .withMessage(facetResult.error().message)
            .emit();
    } else {
        logger.info("host", "builtin_facet_registered")
            .withField("handle", std::to_string(facetResult.value().value()))
            .withField("method", runtimeInfoMethod->name())
            .emit();
    }

    logger.info("lifecycle", "load").withField("stage", "5").withField("target", "win-x64").emit();

    mInitialized = true;
    return true;
}

bool Runtime::enable() {
    if (!mInitialized) return false;
    if (mEnabled) return true;

    auto& logger = diagnostic::globalLogger();
    logger.info("lifecycle", "enable").emit();

    mPageManager = std::make_unique<page::PageContextManager>();
    mApi->setPageManager(mPageManager.get());
    mViewRegistry = std::make_unique<ipc::CoherentViewRegistry>();
    mHookAdapter  = std::make_unique<hook::OreUIHookAdapter>(
        static_cast<hook::IPageHookCallback&>(*this),
        mCapabilities,
        *mViewRegistry,
        logger,
        mConfig.dataDirectory
    );

    auto sourceBase = mConfig.minecraftDirectory / "data" / "gui" / "dist" / "hbui";
    mSourceReader   = std::make_unique<source::FileSystemSourceReader>(std::move(sourceBase));

    mChangePlanner   = std::make_unique<transform::ChangePlanner>(*mRegistry);
    mPageTransformer = std::make_unique<transform::PageTransformer>();

    mHostBridge     = std::make_unique<ipc::CoherentHostBridge>(*mViewRegistry, ipc::defaultCoherentExecutor);
    mApi->setEventBridge(mHostBridge.get());
    mHostDispatcher = std::make_unique<ipc::HostDispatcher>(*mHostMethodRegistry, *mPageManager, logger);
    mHostDispatcher->setReportCallback([this](api::ContextId context, api::RequestId request, std::string_view method, std::chrono::milliseconds elapsed, std::size_t requestBytes, std::size_t responseBytes, api::ErrorCode result) {
        if (mApi != nullptr) mApi->recordHostCallReport(api::HostCallReportView{context, request, std::string{method}, elapsed, requestBytes, responseBytes, result});
    });
    // The HostDispatcher is shared by the native Facet JS->C++ bridge and the
    // optional loopback transport. The active page contract currently uses
    // the game's native facet:request protocol; dynamic BindCall/
    // RegisterForEvent is deliberately not used on this client.
    mHostBridge->setHostDispatcher(*mHostDispatcher);
    mInjector = std::make_unique<inject::RuntimeInjector>(logger, *mHostBridge);
    // Stage 8-A: native facet JS->C++ channel. The createFacetRegistry hook
    // hands every fresh IFacetRegistry to the bridge, which registers the
    // "dearoreui" facet (game-native dispatch, no engine bindings) and pushes
    // responses back through the C++->JS bridge.
    mOreUIFacetBridge = std::make_unique<ipc::OreUIFacetBridge>(*mHostDispatcher, *mHostBridge);
    mHookAdapter->setOnFacetRegistryCreated([this](void* registryPtr) {
        if (mOreUIFacetBridge != nullptr) {
            mOreUIFacetBridge->onFacetRegistryCreated(registryPtr);
        }
    });
    mUiPlanner    = std::make_unique<ui::UiPlanner>(*mRegistry);
    mMountHost    = std::make_unique<ui::NullMountHost>();
    mMountManager = std::make_unique<ui::MountManager>(*mMountHost);

    bool result = true;
    if (mConfig.enableHooks) {
        result = mHookAdapter->install();
    }
    logger.info("status", result ? "hooks_installed" : "hooks_unavailable").emit();

    // Stage 7.1 troubleshooting: summarize the real-display chain setup so a
    // broken gate is immediately visible in diagnostics.jsonl.
    auto hbuiRoot = mConfig.minecraftDirectory / "data" / "gui" / "dist" / "hbui";
    logger.info("stage7", "chain_setup")
        .withField("data_directory", mConfig.dataDirectory.string())
        .withField("minecraft_directory", mConfig.minecraftDirectory.string())
        .withField("hbui_root_exists", std::filesystem::exists(hbuiRoot) ? "true" : "false")
        .withField("hooks_installed", result ? "true" : "false")
        .withField("bridge_available", mHostBridge != nullptr && mHostBridge->isAvailable() ? "true" : "false")
        .withField(
            "view_registry_has_view",
            mViewRegistry != nullptr && mViewRegistry->hasActiveView() ? "true" : "false"
        )
        .withField("enable_demo_overlay", mConfig.enableDemoOverlay ? "true" : "false")
        .emit();

    if (mApi != nullptr) {
        mApi->setReady(result);
    }

    if (result) {
        logger.info("runtime", "ready").withField("page_lifecycle", "enabled").emit();
        if (mConfig.enableDemoOverlay) {
            registerDemoOverlay();
        }
        if (mConfig.enableComponentShowcase) {
            registerComponentShowcase();
        }
    }

    mEnabled = result;
    return result;
}

bool Runtime::disable() {
    if (!mInitialized) return true;

    auto& logger = diagnostic::globalLogger();

    diagnostic::uninstallCrashProbe();

    if (mApi != nullptr) {
        mApi->setReady(false);
    }

    bool hooksRemoved = true;
    if (mHookAdapter != nullptr) {
        if (mConfig.enableHooks) {
            poc::stopStage1Navigation();
        }
        hooksRemoved = mHookAdapter->uninstall();
        mHookAdapter.reset();
    }
    logger.info("status", hooksRemoved ? "hooks_removed" : "hooks_remove_failed").emit();

    mInjector.reset();
    mMountManager.reset();
    mMountHost.reset();
    mUiPlanner.reset();
    mSourceReader.reset();
    mChangePlanner.reset();
    mPageTransformer.reset();

    if (mPageManager != nullptr) {
        mPageManager->clear();
        mPageManager.reset();
    }

    // Stage 8-A: the facet bridge references dispatcher/bridge/registry below;
    // tear it down first.
    mOreUIFacetBridge.reset();

    if (mHostDispatcher != nullptr) {
        mHostDispatcher.reset();
    }
    if (mHostBridge != nullptr) {
        mHostBridge.reset();
    }
    if (mViewRegistry != nullptr) {
        mViewRegistry->clear();
        mViewRegistry.reset();
    }

    if (mRegistry != nullptr) {
        mRegistry->clear();
    }

    logger.info("lifecycle", "disable").emit();

    if (mConfig.enableStage0Compatibility) {
        diagnostic::resetStage0Session();
    }

    logger.flush();

    mApi.reset();
    mHostMethodRegistry.reset();
    mFacetRegistry.reset();
    mRegistry.reset();
    mEnabled     = false;
    mInitialized = false;
    return true;
}

api::ContextId Runtime::onPageCreated(std::string_view url, std::optional<api::RouterLocationSnapshot> location) {
    auto& logger = diagnostic::globalLogger();

    if (mPageManager == nullptr) {
        return api::ContextId{};
    }

    auto info      = page::PageContextManager::pageInfoFromUrl(url);
    info.location  = std::move(location);
    auto contextId = mPageManager->createContext(std::move(info));

    if (auto found = mPageManager->find(contextId); found) {
        if (mApi != nullptr) {
            mApi->notifyPage(api::PageEvent::Created, api::PageContextView{contextId, found->page});
        }
        diagnostic::recordStage3PageCreated(contextId, found->page, url);
        logger.info("page", "created")
            .withContext(contextId)
            .withPage(found->page.id)
            .withField("scope", std::to_string(static_cast<int>(found->page.scope)))
            .withField("url", std::string(url))
            .emit();

        runStage4Injection(contextId, found->page);
    }

    return contextId;
}

void Runtime::runStage4Injection(api::ContextId id, api::PageInfo const& info) {
    if (mSourceReader == nullptr || mInjector == nullptr) {
        return;
    }

    auto& logger = diagnostic::globalLogger();

    // Stage 7.1 troubleshooting: trace entry into the page injection chain.
    logger.info("stage7", "injection_entered")
        .withContext(id)
        .withField("scope", std::to_string(static_cast<int>(info.scope)))
        .withField("ui_entries", std::to_string(mRegistry != nullptr ? mRegistry->listUiEntries().size() : 0))
        .emit();

    auto snapshotResult = mSourceReader->capture(info);
    if (snapshotResult.isErr()) {
        logger.warning("source", "capture_failed")
            .withContext(id)
            .withError(snapshotResult.error().code)
            .withMessage(snapshotResult.error().message)
            .emit();
        return;
    }

    auto snapshot      = std::move(snapshotResult.value());
    snapshot.contextId = id;
    diagnostic::recordStage4SnapshotCaptured(id, info, snapshot);

    // Stage 6: build a change plan and materialize the final page resources.
    transform::ChangePlan plan;
    if (mChangePlanner != nullptr) {
        plan = mChangePlanner->plan(id, info.scope);
    }

    transform::TransformedPage transformed;
    if (mPageTransformer != nullptr) {
        transformed = mPageTransformer->transform(plan, snapshot);
        if (mApi != nullptr) {
            api::TransformReport report;
            report.context = id;
            report.scope = info.scope;
            report.preview = false;
            report.success = transformed.report.success;
            report.applicable = transformed.report.applied;
            report.blocked = transformed.report.blocked;
            for (auto const& operation : transformed.report.operations) {
                report.operations.push_back(api::TransformOperationInfo{operation.handle, operation.owner, operation.path, operation.fingerprint, operation.status == transform::ChangeOperationStatus::Applied, operation.status == transform::ChangeOperationStatus::Applied ? "applied" : "not applied"});
            }
            for (auto const& error : transformed.report.errors) report.errors.push_back(error);
            mApi->recordTransformReport(std::move(report));
        }
    }

    auto index = std::make_unique<resource::ResourceIndex>();
    index->registerSnapshot(snapshot);
    for (auto const& entry : transformed.scripts) {
        index->registerModScript(entry);
    }
    for (auto const& entry : transformed.styles) {
        index->registerModStyleSheet(entry);
    }
    for (auto const& entry : transformed.resources) {
        index->registerModResource(entry);
    }

    auto locations = index->listForPage(info.scope);
    diagnostic::recordStage4ResourceIndexBuilt(id, locations.size());

    auto injectResult = mInjector->inject(id, *index);
    if (mApi != nullptr && injectResult.isOk()) {
        auto const& r = injectResult.value();
        api::InjectionReportView view;
        view.context = r.contextId;
        view.success = r.success;
        view.bridgeAvailable = r.hostBridgeAvailable;
        view.scriptCount = r.injectedScripts.size();
        view.styleCount = r.injectedStyleSheets.size();
        view.uiCount = r.uiCount;
        for (auto const& error : r.errors) view.errors.push_back(error.code);
        mApi->recordInjectionReport(std::move(view));
    }
    if (injectResult.isErr()) {
        logger.warning("inject", "failed")
            .withContext(id)
            .withError(injectResult.error().code)
            .withMessage(injectResult.error().message)
            .emit();
        return;
    }

    diagnostic::recordStage4InjectSubmitted(id, injectResult.value());

    // Stage 7: plan and mount UI overlays for this page scope.
    if (mUiPlanner != nullptr && mMountManager != nullptr && mInjector != nullptr) {
        auto uiPlan      = mUiPlanner->plan(id, info.scope);
        auto mountResult = mMountManager->mountPage(id, std::move(uiPlan));
        if (mountResult.isOk()) {
            auto const& mountedPlan = mountResult.value();
            logger.info("stage7", "ui_plan_result")
                .withContext(id)
                .withField("mounted", std::to_string(mountedPlan.mounted))
                .withField("skipped", std::to_string(mountedPlan.skipped))
                .withField("blocked", std::to_string(mountedPlan.blocked))
                .withField("bridge_available", mHostBridge != nullptr && mHostBridge->isAvailable() ? "true" : "false")
                .emit();

            auto uiInjectResult = mInjector->injectUi(id, mountedPlan);
            if (uiInjectResult.isErr()) {
                logger.warning("inject", "ui_bootstrap_failed")
                    .withContext(id)
                    .withError(uiInjectResult.error().code)
                    .withMessage(uiInjectResult.error().message)
                    .emit();
            } else {
                logger.info("stage7", "ui_inject_result")
                    .withContext(id)
                    .withField("ui_count", std::to_string(uiInjectResult.value().uiCount))
                    .withField("submitted", uiInjectResult.value().hostBridgeAvailable ? "true" : "false")
                    .emit();

                // Offline-preview asset export: after a mount, capture the
                // freshly registered UIs (best-effort, de-duplicated; never
                // affects the inject result).
                exportUiAssetsForPreview();
            }
        } else {
            logger.warning("stage7", "ui_mount_failed")
                .withContext(id)
                .withError(mountResult.error().code)
                .withMessage(mountResult.error().message)
                .emit();
        }
    }

    if (mApi != nullptr) {
        mApi->notifyPage(api::PageEvent::Ready, api::PageContextView{id, info});
    }
    logger.info("page", "ready")
        .withContext(id)
        .withPage(info.id)
        .withField("scope", std::to_string(static_cast<int>(info.scope)))
        .withField("applied", std::to_string(transformed.report.applied))
        .withField("skipped", std::to_string(transformed.report.skipped))
        .withField("blocked", std::to_string(transformed.report.blocked))
        .emit();
}

void Runtime::onPageDestroyed(api::ContextId id) {
    auto& logger = diagnostic::globalLogger();

    if (mMountManager != nullptr) {
        static_cast<void>(mMountManager->unmountPage(id));
    }

    if (mHostDispatcher != nullptr) {
        mHostDispatcher->invalidateContext(id);
    }
    if (mHostBridge != nullptr) {
        mHostBridge->invalidateContext(id);
    }

    if (mPageManager == nullptr) return;

    auto context = mPageManager->find(id);
    bool removed = mPageManager->destroyContext(id);

    if (context) {
        if (mApi != nullptr) {
            mApi->clearRuntimeReports(id);
            mApi->notifyPage(api::PageEvent::Destroyed, api::PageContextView{id, context->page});
        }
        diagnostic::recordStage3PageDestroyed(id, context->page);
    }

    logger.info("page", "destroyed")
        .withContext(id)
        .withField("removed", removed ? "true" : "false")
        .withField("page_id", context ? context->page.id.value() : std::string{"unknown"})
        .emit();
}

diagnostic::DiagnosticLogger& Runtime::diagnostics() { return diagnostic::globalLogger(); }

capability::ICapabilityQuery& Runtime::capabilities() { return mCapabilities; }

api::IDearOreUIApi* Runtime::api() { return mApi.get(); }

// Frame service entry: called by the OreUIHookAdapter from the
// ClientInstance::update hook (client main loop, game thread). Drives frame
// subscribers only while the runtime is enabled and at least one page context
// exists, so periodic C++->JS pushes stay tied to a live page.
void Runtime::onClientFrame() {
    if (!mEnabled || mApi == nullptr || mPageManager == nullptr) {
        return;
    }
    if (mPageManager->activeContexts().empty()) {
        return;
    }
    mApi->frameTick();
}

page::IPageContextManager* Runtime::pageManager() { return mPageManager.get(); }

ipc::HostDispatcher* Runtime::hostDispatcher() { return mHostDispatcher.get(); }

void Runtime::registerDemoOverlay() {
    if (mApi == nullptr || mRegistry == nullptr) {
        return;
    }
    auto& logger = diagnostic::globalLogger();
    logger.info("demo", "register_attempt").withField("mod_id", "dearoreui").withField("ui_id", "demo").emit();

    api::ModManifest modManifest;
    modManifest.id           = api::ModId{"dearoreui"};
    modManifest.modNamespace = "dearoreui";
    modManifest.modVersion   = api::Version{0, 2, 0};
    auto modResult           = mApi->registerMod(modManifest);
    if (modResult.isErr()) {
        logger.warning("demo", "mod_registration_failed")
            .withError(modResult.error().code)
            .withMessage(modResult.error().message)
            .emit();
        return;
    }

    api::UiManifest uiManifest;
    uiManifest.modNamespace  = "dearoreui";
    uiManifest.id            = "demo";
    uiManifest.kind          = api::UiKind::Overlay;
    uiManifest.pageScopes    = {api::PageScope::Any};
    uiManifest.anchor        = api::UiAnchor::TopLeft;
    uiManifest.pointerEvents = false;
    uiManifest.fingerprint   = "demo-overlay-v2";

    // Stage 8-A cleanup: the demo overlay is now just a tiny corner badge —
    // it stays as the mount target for the round-6 event-context probe button,
    // but no longer renders the full-screen panel that blocked the view.
    component::ComponentSpec demoBadge;
    demoBadge.kind    = component::ComponentKind::Text;
    demoBadge.variant = "muted";
    demoBadge.label   = "DearOreUI 0.2.0";

    auto uiResult = mApi->registerComponent(api::ModId{"dearoreui"}, uiManifest, demoBadge);
    if (uiResult.isErr()) {
        logger.warning("demo", "overlay_registration_failed")
            .withError(uiResult.error().code)
            .withMessage(uiResult.error().message)
            .emit();
        return;
    }

    logger.info("demo", "overlay_registered")
        .withField("handle", std::to_string(uiResult.value().value()))
        .withField("container_id", api::makeUiContainerId("dearoreui", api::UiKind::Overlay, "demo"))
        .emit();
}

void Runtime::registerComponentShowcase() {
    if (mApi == nullptr || mRegistry == nullptr) {
        return;
    }
    auto& logger = diagnostic::globalLogger();
    logger.info("showcase", "register_attempt")
        .withField("mod_id", "dearoreui")
        .withField("ui_id", "component-showcase")
        .emit();

    api::ModManifest modManifest;
    modManifest.id           = api::ModId{"dearoreui"};
    modManifest.modNamespace = "dearoreui";
    modManifest.modVersion   = api::Version{0, 2, 0};
    auto modResult           = mApi->registerMod(modManifest);
    // The mod may already be registered by the demo overlay; that is fine —
    // both overlays share the same "dearoreui" mod. Only a non-already-registered
    // failure is fatal.
    if (modResult.isErr() && modResult.error().code != api::ErrorCode::AlreadyExists) {
        logger.warning("showcase", "mod_registration_failed")
            .withError(modResult.error().code)
            .withMessage(modResult.error().message)
            .emit();
        return;
    }

    api::UiManifest uiManifest;
    uiManifest.modNamespace = "dearoreui";
    uiManifest.id           = "component_showcase";
    uiManifest.kind         = api::UiKind::Overlay;
    uiManifest.pageScopes   = {api::PageScope::Any};
    uiManifest.anchor       = api::UiAnchor::TopLeft;
    // Block clicks: the showcase is a review surface, so pointer events must
    // NOT pass through to the vanilla UI underneath (no accidental clicks).
    uiManifest.pointerEvents = true;
    uiManifest.fingerprint   = "component-showcase-v1";

    // Build a full component-library showcase: a translucent (semi-transparent
    // black) scrollable panel container holding every vanilla atomic component
    // (stage 8.1) plus the stage 8.1.4 layout/composite/nav/interaction/data
    // components. The container blocks pointer events so clicks never reach
    // the vanilla UI underneath (no accidental clicks).
    component::ComponentSpec root;
    root.kind  = component::ComponentKind::Panel;
    root.style = "translucent";
    root.label = "DearOreUI 组件库展示 (Stage 8.1 + 8.1.4)";

    auto section = [](std::string title) {
        component::ComponentSpec s;
        s.kind    = component::ComponentKind::Text;
        s.variant = "subheading";
        s.label   = title;
        return s;
    };
    auto text = [](std::string label, std::string variant) {
        component::ComponentSpec t;
        t.kind    = component::ComponentKind::Text;
        t.variant = variant;
        t.label   = label;
        return t;
    };
    auto button =
        [](std::string label, std::string variant, std::string style = "normal", std::string state = "default") {
            component::ComponentSpec b;
            b.kind    = component::ComponentKind::Button;
            b.variant = variant;
            b.style   = style;
            b.state   = state;
            b.label   = label;
            return b;
        };

    // Text (representative type scale).
    root.children.push_back(section("Text"));
    root.children.push_back(text("Tiny 10px", "tiny"));
    root.children.push_back(text("Medium 16px", "medium"));
    root.children.push_back(text("Heading 32px", "heading"));

    // Button (variants). States are interactive now (M8.1.2 state machine), so
    // static state-variant buttons are omitted. The showcase is kept small:
    // cohtml ExecuteScript silently drops large scripts (verified ~47KB fails,
    // ~9KB works), so every component counts.
    root.children.push_back(section("Button"));
    root.children.push_back(button("Primary", "primary"));
    root.children.push_back(button("Secondary", "secondary"));
    root.children.push_back(button("Destructive", "destructive"));

    // Panel.
    root.children.push_back(section("Panel"));
    {
        component::ComponentSpec p;
        p.kind  = component::ComponentKind::Panel;
        p.style = "chest";
        p.label = "Chest Panel";
        root.children.push_back(p);
    }

    // Card / ListItem.
    root.children.push_back(section("Card / ListItem"));
    {
        component::ComponentSpec card;
        card.kind  = component::ComponentKind::Card;
        card.label = "Card base";
        root.children.push_back(card);
    }
    {
        component::ComponentSpec item;
        item.kind  = component::ComponentKind::ListItem;
        item.label = "List item base";
        root.children.push_back(item);
    }

    // Input / Divider.
    root.children.push_back(section("Input / Divider"));
    {
        component::ComponentSpec input;
        input.kind  = component::ComponentKind::Input;
        input.label = "Seed";
        root.children.push_back(input);
    }
    {
        component::ComponentSpec divider;
        divider.kind = component::ComponentKind::Divider;
        root.children.push_back(divider);
    }

    // TabBar.
    root.children.push_back(section("TabBar"));
    {
        component::ComponentSpec bar;
        bar.kind = component::ComponentKind::TabBar;
        component::ComponentSpec tabA;
        tabA.kind  = component::ComponentKind::Text;
        tabA.label = "Tab A";
        component::ComponentSpec tabB;
        tabB.kind  = component::ComponentKind::Text;
        tabB.label = "Tab B";
        bar.children.push_back(tabA);
        bar.children.push_back(tabB);
        root.children.push_back(bar);
    }

    // Tooltip / ContainerSlot / KeyIcon / Progress.
    root.children.push_back(section("Tooltip / Slot / Key / Progress"));
    {
        component::ComponentSpec tip;
        tip.kind  = component::ComponentKind::Tooltip;
        tip.label = "Tooltip text";
        root.children.push_back(tip);
    }
    {
        component::ComponentSpec slot;
        slot.kind  = component::ComponentKind::ContainerSlot;
        slot.label = "64";
        root.children.push_back(slot);
    }
    {
        component::ComponentSpec key;
        key.kind  = component::ComponentKind::KeyIcon;
        key.label = "A";
        root.children.push_back(key);
    }
    {
        component::ComponentSpec progress;
        progress.kind = component::ComponentKind::Progress;
        root.children.push_back(progress);
    }
    {
        component::ComponentSpec bubble;
        bubble.kind  = component::ComponentKind::Bubble;
        bubble.label = "Bubble";
        root.children.push_back(bubble);
    }
    {
        component::ComponentSpec filter;
        filter.kind  = component::ComponentKind::FilterBar;
        filter.label = "Filter";
        root.children.push_back(filter);
    }

    // Stage 8.1.4: layout components (stack / grid / section / spacer /
    // scrollView).
    root.children.push_back(section("Layout (Stack / Grid / Section / Scroll)"));
    {
        component::ComponentSpec scroll;
        scroll.kind = component::ComponentKind::ScrollView;
        component::ComponentSpec item;
        item.kind  = component::ComponentKind::ListItem;
        item.label = "Scroll item";
        scroll.children.push_back(item);
        root.children.push_back(scroll);
    }
    {
        component::ComponentSpec spacer;
        spacer.kind = component::ComponentKind::Spacer;
        root.children.push_back(spacer);
    }
    {
        component::ComponentSpec stack;
        stack.kind = component::ComponentKind::Stack;
        component::ComponentSpec a;
        a.kind  = component::ComponentKind::Button;
        a.label = "A";
        component::ComponentSpec b;
        b.kind  = component::ComponentKind::Button;
        b.label = "B";
        stack.children.push_back(a);
        stack.children.push_back(b);
        root.children.push_back(stack);
    }
    {
        component::ComponentSpec grid;
        grid.kind    = component::ComponentKind::Grid;
        grid.columns = 3;
        for (int i = 0; i < 3; ++i) {
            component::ComponentSpec slot;
            slot.kind = component::ComponentKind::ContainerSlot;
            grid.children.push_back(slot);
        }
        root.children.push_back(grid);
    }
    {
        component::ComponentSpec sec;
        sec.kind  = component::ComponentKind::Section;
        sec.label = "Section title";
        component::ComponentSpec tip;
        tip.kind  = component::ComponentKind::Tooltip;
        tip.label = "Section content";
        sec.children.push_back(tip);
        root.children.push_back(sec);
    }

    // Stage 8.1.4: composite components (modal / menu / dropdown / form /
    // navigationBar / toast / searchField / toggle / badge).
    root.children.push_back(section("Composite (Modal / Menu / Dropdown / Form)"));
    {
        component::ComponentSpec modal;
        modal.kind  = component::ComponentKind::Modal;
        modal.label = "Confirm";
        component::ComponentSpec ok;
        ok.kind    = component::ComponentKind::Button;
        ok.variant = "primary";
        ok.label   = "OK";
        modal.children.push_back(ok);
        root.children.push_back(modal);
    }
    {
        component::ComponentSpec menu;
        menu.kind = component::ComponentKind::Menu;
        for (int i = 0; i < 2; ++i) {
            component::ComponentSpec item;
            item.kind  = component::ComponentKind::ListItem;
            item.label = "Item " + std::to_string(i + 1);
            menu.children.push_back(item);
        }
        root.children.push_back(menu);
    }
    {
        component::ComponentSpec list;
        list.kind = component::ComponentKind::ScrollingList;
        for (int i = 0; i < 3; ++i) {
            component::ComponentSpec item;
            item.kind  = component::ComponentKind::ListItem;
            item.label = "List item " + std::to_string(i + 1);
            list.children.push_back(item);
        }
        root.children.push_back(list);
    }
    {
        component::ComponentSpec dropdown;
        dropdown.kind  = component::ComponentKind::Dropdown;
        dropdown.label = "Select";
        component::ComponentSpec opt;
        opt.kind  = component::ComponentKind::ListItem;
        opt.label = "Option 1";
        dropdown.children.push_back(opt);
        root.children.push_back(dropdown);
    }
    {
        component::ComponentSpec form;
        form.kind  = component::ComponentKind::Form;
        form.label = "Settings";
        component::ComponentSpec input;
        input.kind  = component::ComponentKind::Input;
        input.label = "Seed";
        form.children.push_back(input);
        root.children.push_back(form);
    }
    {
        component::ComponentSpec nav;
        nav.kind  = component::ComponentKind::NavigationBar;
        nav.label = "DearOreUI";
        root.children.push_back(nav);
    }
    {
        component::ComponentSpec toast;
        toast.kind  = component::ComponentKind::Toast;
        toast.label = "Saved!";
        root.children.push_back(toast);
    }
    {
        component::ComponentSpec search;
        search.kind  = component::ComponentKind::SearchField;
        search.label = "Search...";
        root.children.push_back(search);
    }
    {
        component::ComponentSpec toggle;
        toggle.kind  = component::ComponentKind::Toggle;
        toggle.state = "on";
        toggle.label = "Enable";
        root.children.push_back(toggle);
    }
    {
        component::ComponentSpec badge;
        badge.kind  = component::ComponentKind::Badge;
        badge.label = "3";
        root.children.push_back(badge);
    }

    // Stage 8.1.4: navigation / interaction / data (breadcrumb / pager /
    // slider / stepper / picker / icon / image).
    root.children.push_back(section("Nav / Interaction / Data"));
    {
        component::ComponentSpec crumb;
        crumb.kind = component::ComponentKind::Breadcrumb;
        component::ComponentSpec home;
        home.kind  = component::ComponentKind::Text;
        home.label = "Home";
        component::ComponentSpec worlds;
        worlds.kind  = component::ComponentKind::Text;
        worlds.label = "Worlds";
        crumb.children.push_back(home);
        crumb.children.push_back(worlds);
        root.children.push_back(crumb);
    }
    {
        component::ComponentSpec pager;
        pager.kind    = component::ComponentKind::Pager;
        pager.columns = 3;
        pager.value   = "1";
        root.children.push_back(pager);
    }
    {
        component::ComponentSpec area;
        area.kind  = component::ComponentKind::TextArea;
        area.label = "Notes";
        root.children.push_back(area);
    }
    {
        component::ComponentSpec slider;
        slider.kind  = component::ComponentKind::Slider;
        slider.value = "50";
        root.children.push_back(slider);
    }
    {
        component::ComponentSpec stepper;
        stepper.kind  = component::ComponentKind::Stepper;
        stepper.value = "3";
        root.children.push_back(stepper);
    }
    {
        component::ComponentSpec picker;
        picker.kind  = component::ComponentKind::Picker;
        picker.value = "A";
        component::ComponentSpec opt;
        opt.kind  = component::ComponentKind::ListItem;
        opt.label = "A";
        picker.children.push_back(opt);
        root.children.push_back(picker);
    }
    {
        component::ComponentSpec icon;
        icon.kind = component::ComponentKind::Icon;
        icon.icon = "checkmark";
        root.children.push_back(icon);
    }
    {
        component::ComponentSpec image;
        image.kind = component::ComponentKind::Image;
        image.src  = "/hbui/assets/Play-b8e5aadba97d31b3abd0.png";
        root.children.push_back(image);
    }

    auto uiResult = mApi->registerComponent(api::ModId{"dearoreui"}, uiManifest, root);
    if (uiResult.isErr()) {
        logger.warning("showcase", "overlay_registration_failed")
            .withError(uiResult.error().code)
            .withMessage(uiResult.error().message)
            .emit();
        return;
    }

    // TEMP diagnostic (stage 8.1): dump the generated htmlBody and the
    // serialized JS body array so the real-client rendering issue can be
    // inspected without a debugger.
    {
        auto          html  = component::renderComponentToHtml(root);
        auto          nodes = render::parseHtmlFragment(html);
        std::ofstream dumpHtml(mConfig.dataDirectory / "showcase-html.html");
        dumpHtml << html;
        std::ofstream dumpBody(mConfig.dataDirectory / "showcase-body.js");
        dumpBody << render::serializeDomForest(nodes);
        logger.info("showcase", "dump_written")
            .withField("html_length", std::to_string(html.size()))
            .withField("node_count", std::to_string(nodes.size()))
            .emit();
    }

    logger.info("showcase", "overlay_registered")
        .withField("handle", std::to_string(uiResult.value().value()))
        .withField("container_id", api::makeUiContainerId("dearoreui", api::UiKind::Overlay, "component_showcase"))
        .withField("pointer_events", "auto")
        .withField("click_blocked", "true")
        .emit();
}

void Runtime::exportUiAssetsForPreview() {
    if (mRegistry == nullptr) {
        return;
    }
    auto uiEntries = mRegistry->listUiEntries();
    if (uiEntries.size() == mLastExportedUiCount) {
        return; // no new UIs since last export — avoid rewriting on every page
    }

    preview::UiAssetExporter exporter(diagnostic::globalLogger());
    if (exporter.exportUiEntries(uiEntries, mConfig.dataDirectory / "preview")) {
        mLastExportedUiCount = uiEntries.size();
    }
}

} // namespace dearoreui::runtime
