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
        }
    }
    logger.info("stage8", "switch_loaded")
        .withField("disable_hooks", mConfig.enableHooks ? "0" : "1")
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
    mApi                = std::make_unique<api::DearOreUIApi>(
        *mRegistry, *mHostMethodRegistry, mCapabilities, logger
    );

    auto runtimeInfoFacet = std::make_shared<facet::RuntimeInfoFacet>(*mApi);
    auto facetRegisterResult = mFacetRegistry->registerProvider(runtimeInfoFacet);
    if (facetRegisterResult.isErr()) {
        logger.warning("facet", "builtin_registration_failed")
            .withError(facetRegisterResult.error().code)
            .withMessage(facetRegisterResult.error().message)
            .emit();
    }

    auto runtimeInfoMethod = std::make_shared<facet::FacetHostMethod>(
        runtimeInfoFacet, api::Permission::HostReadOnly
    );
    api::PermissionSet runtimePermissions{std::vector{api::Permission::HostReadOnly}};
    auto               facetResult = mHostMethodRegistry->registerMethod(
        api::ModId{"dearoreui"}, runtimePermissions, runtimeInfoMethod
    );
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
    mViewRegistry = std::make_unique<ipc::CoherentViewRegistry>();
    mHookAdapter = std::make_unique<hook::OreUIHookAdapter>(
        static_cast<hook::IPageHookCallback&>(*this),
        mCapabilities,
        *mViewRegistry,
        logger,
        mConfig.dataDirectory
    );

    auto sourceBase = mConfig.minecraftDirectory / "data" / "gui" / "dist" / "hbui";
    mSourceReader   = std::make_unique<source::FileSystemSourceReader>(std::move(sourceBase));

    mChangePlanner    = std::make_unique<transform::ChangePlanner>(*mRegistry);
    mPageTransformer  = std::make_unique<transform::PageTransformer>();

    mHostBridge     = std::make_unique<ipc::CoherentHostBridge>(
        *mViewRegistry, ipc::defaultCoherentExecutor
    );
    mHostDispatcher = std::make_unique<ipc::HostDispatcher>(
        *mHostMethodRegistry, *mPageManager, logger
    );
    // Stage 8: the native JS->C++ handler needs the dispatcher to route
    // engine.trigger("dearoreui_report", ...) payloads (handled via
    // handleJsPayload). The bridge registers that handler at view_initialize.
    mHostBridge->setHostDispatcher(*mHostDispatcher);
    // Stage 8: the JS->C++ channel moves off the cohtml engine bindings
    // entirely (BindCall/RegisterForEvent both crash the client on page
    // teardown). It now runs over the WebSocket loopback: a local server
    // routes JS frames through handleJsPayload into HostDispatcher. The
    // bridge stays C++->JS only (ExecuteScript + defer queue).
    std::string wsUrl;
    mWsServer = std::make_unique<ipc::LoopbackWsServer>();
    auto wsResult = mWsServer->start(*mHostDispatcher);
    if (wsResult.isOk()) {
        auto const& info = wsResult.value();
        wsUrl = "ws://127.0.0.1:" + std::to_string(info.port) + "/dearoreui?token=" + info.token;
        logger.info("ws", "url_ready").withField("url", wsUrl).emit();
    } else {
        logger.warning("ws", "start_failed")
            .withError(wsResult.error().code)
            .withMessage(wsResult.error().message)
            .emit();
    }
    mInjector       = std::make_unique<inject::RuntimeInjector>(logger, *mHostBridge, wsUrl);
    // Stage 8-A: native facet JS->C++ channel. The createFacetRegistry hook
    // hands every fresh IFacetRegistry to the bridge, which registers the
    // "dearoreui" facet (game-native dispatch, no engine bindings) and pushes
    // responses back through the C++->JS bridge.
    mOreUIFacetBridge = std::make_unique<ipc::OreUIFacetBridge>(
        *mHostDispatcher, *mHostBridge
    );
    mHookAdapter->setOnFacetRegistryCreated([this](void* registryPtr) {
        if (mOreUIFacetBridge != nullptr) {
            mOreUIFacetBridge->onFacetRegistryCreated(registryPtr);
        }
    });
    mUiPlanner      = std::make_unique<ui::UiPlanner>(*mRegistry);
    mMountHost      = std::make_unique<ui::NullMountHost>();
    mMountManager   = std::make_unique<ui::MountManager>(*mMountHost);

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
        .withField(
            "bridge_available",
            mHostBridge != nullptr && mHostBridge->isAvailable() ? "true" : "false"
        )
        .withField("view_registry_has_view", mViewRegistry != nullptr && mViewRegistry->hasActiveView() ? "true" : "false")
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

    // Stop the WS loopback FIRST: its threads reference the host dispatcher,
    // page manager and logger below, which are torn down next.
    if (mWsServer != nullptr) {
        mWsServer->stop();
        mWsServer.reset();
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
                .withField(
                    "bridge_available",
                    mHostBridge != nullptr && mHostBridge->isAvailable() ? "true" : "false"
                )
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
                    .withField(
                        "submitted",
                        uiInjectResult.value().hostBridgeAvailable ? "true" : "false"
                    )
                    .emit();
            }
        } else {
            logger.warning("stage7", "ui_mount_failed")
                .withContext(id)
                .withError(mountResult.error().code)
                .withMessage(mountResult.error().message)
                .emit();
        }
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

page::IPageContextManager* Runtime::pageManager() { return mPageManager.get(); }

ipc::HostDispatcher* Runtime::hostDispatcher() { return mHostDispatcher.get(); }

void Runtime::registerDemoOverlay() {
    if (mApi == nullptr || mRegistry == nullptr) {
        return;
    }
    auto& logger = diagnostic::globalLogger();
    logger.info("demo", "register_attempt")
        .withField("mod_id", "dearoreui")
        .withField("ui_id", "demo")
        .emit();

    api::ModManifest modManifest;
    modManifest.id           = api::ModId{"dearoreui"};
    modManifest.modNamespace = "dearoreui";
    modManifest.modVersion   = api::Version{0, 2, 0};
    auto modResult = mApi->registerMod(modManifest);
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
    uiManifest.fingerprint   = "demo-overlay-v1";

    // Stage 8: build the demo overlay from declarative components. The panel
    // + button tree is rendered by ComponentRenderer and flows through the
    // same register/plan/mount/inject pipeline as any Mod UI.
    component::ComponentSpec demoPanel;
    demoPanel.kind  = component::ComponentKind::Panel;
    demoPanel.label = "DearOreUI";

    component::ComponentSpec demoText;
    demoText.kind    = component::ComponentKind::Text;
    demoText.variant = "heading";
    demoText.label   = "DEMO";

    component::ComponentSpec demoButton;
    demoButton.kind    = component::ComponentKind::Button;
    demoButton.variant = "primary";
    demoButton.label   = "OK";

    demoPanel.children.push_back(demoText);
    demoPanel.children.push_back(demoButton);

    auto uiResult = mApi->registerComponent(api::ModId{"dearoreui"}, uiManifest, demoPanel);
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

} // namespace dearoreui::runtime
