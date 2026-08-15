#include "runtime/Runtime.h"

#include "capability/ICapabilityQuery.h"
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

#include <utility>

namespace dearoreui::runtime {

Runtime::Runtime(RuntimeConfig config) : mConfig(std::move(config)) {}

bool Runtime::initialize() {
    if (mInitialized) return true;

    auto& logger = diagnostic::globalLogger();

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

    mHostBridge     = std::make_unique<ipc::CoherentHostBridge>(*mViewRegistry);
    mHostDispatcher = std::make_unique<ipc::HostDispatcher>(
        *mHostMethodRegistry, *mPageManager, logger
    );
    mInjector       = std::make_unique<inject::RuntimeInjector>(logger, *mHostBridge);
    mUiPlanner      = std::make_unique<ui::UiPlanner>(*mRegistry);
    mMountHost      = std::make_unique<ui::NullMountHost>();
    mMountManager   = std::make_unique<ui::MountManager>(*mMountHost);

    bool result = true;
    if (mConfig.enableHooks) {
        result = mHookAdapter->install();
    }
    logger.info("status", result ? "hooks_installed" : "hooks_unavailable").emit();

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
        auto uiPlan        = mUiPlanner->plan(id, info.scope);
        auto mountResult   = mMountManager->mountPage(id, std::move(uiPlan));
        if (mountResult.isOk()) {
            auto uiInjectResult = mInjector->injectUi(id, mountResult.value());
            if (uiInjectResult.isErr()) {
                logger.warning("inject", "ui_bootstrap_failed")
                    .withContext(id)
                    .withError(uiInjectResult.error().code)
                    .withMessage(uiInjectResult.error().message)
                    .emit();
            }
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

    auto uiResult = mApi->registerOverlay(
        api::ModId{"dearoreui"},
        uiManifest,
        "<div id=\"dearoreui-demo-text\" style=\"padding:8px 12px;"
        "background:var(--colorsBackground,rgba(0,0,0,0.75));"
        "color:var(--colorsText,#ffffff);"
        "font-family:var(--fontsUi,sans-serif);"
        "font-size:14px;"
        "border:1px solid var(--colorsPrimary,#4caf50);\">"
        "DearOreUI Demo Overlay</div>"
    );
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
