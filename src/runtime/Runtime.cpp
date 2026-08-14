#include "runtime/Runtime.h"

#include "capability/ICapabilityQuery.h"
#include "diagnostic/DiagnosticLogger.h"
#include "diagnostic/FileDiagnosticSink.h"
#include "diagnostic/Stage0TelemetryCompat.h"
#include "diagnostic/Stage3PageLifecycleTelemetry.h"
#include "diagnostic/Stage4InjectTelemetry.h"
#include "hook/OreUIHookAdapter.h"
#include "inject/RuntimeInjector.h"
#include "ipc/NullHostBridge.h"
#include "poc/Stage1NavigationPoc.h"
#include "resource/ResourceIndex.h"
#include "source/FileSystemSourceReader.h"

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
    mApi                = std::make_unique<api::DearOreUIApi>(
        *mRegistry, *mHostMethodRegistry, mCapabilities, logger
    );

    logger.info("lifecycle", "load").withField("stage", "3").withField("target", "win-x64").emit();

    mInitialized = true;
    return true;
}

bool Runtime::enable() {
    if (!mInitialized) return false;
    if (mEnabled) return true;

    auto& logger = diagnostic::globalLogger();
    logger.info("lifecycle", "enable").emit();

    mPageManager = std::make_unique<page::PageContextManager>();
    mHookAdapter = std::make_unique<hook::OreUIHookAdapter>(
        static_cast<hook::IPageHookCallback&>(*this),
        mCapabilities,
        logger,
        mConfig.dataDirectory
    );

    auto sourceBase = mConfig.minecraftDirectory / "data" / "gui" / "dist" / "hbui";
    mSourceReader   = std::make_unique<source::FileSystemSourceReader>(std::move(sourceBase));

    mHostBridge     = std::make_unique<ipc::NullHostBridge>();
    mHostDispatcher = std::make_unique<ipc::HostDispatcher>(
        *mHostMethodRegistry, *mPageManager, logger
    );
    mInjector       = std::make_unique<inject::RuntimeInjector>(logger, *mHostBridge);

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
    mSourceReader.reset();

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

    auto index = std::make_unique<resource::ResourceIndex>();
    index->registerSnapshot(snapshot);

    // Stage 4 only indexes the original snapshot.
    // Mod-registered resources, scripts and stylesheets will be merged here in stage 6.

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

    logger.info("page", "ready")
        .withContext(id)
        .withPage(info.id)
        .withField("scope", std::to_string(static_cast<int>(info.scope)))
        .emit();
}

void Runtime::onPageDestroyed(api::ContextId id) {
    auto& logger = diagnostic::globalLogger();

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

} // namespace dearoreui::runtime
