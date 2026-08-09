#include "runtime/Runtime.h"

#include "capability/ICapabilityQuery.h"
#include "diagnostic/DiagnosticLogger.h"
#include "diagnostic/FileDiagnosticSink.h"
#include "diagnostic/Stage0TelemetryCompat.h"
#include "hook/Stage0OreUIHooks.h"
#include "poc/Stage1NavigationPoc.h"

#include <utility>

namespace dearoreui::runtime {

Runtime::Runtime(RuntimeConfig config) : mConfig(std::move(config)) {}

bool Runtime::initialize() {
    if (mInitialized) return true;

    auto& logger = diagnostic::globalLogger();

    if (mConfig.enableFileDiagnostics) {
        logger.addSink(std::make_shared<diagnostic::FileDiagnosticSink>(
            mConfig.dataDirectory / "diagnostics" / "diagnostics.jsonl"
        ));
    }

    if (mConfig.enableStage0Compatibility) {
        diagnostic::initializeStage0FileSink(mConfig.dataDirectory);
        diagnostic::startStage0Session();
    }

    mRegistry = std::make_unique<registry::ModRegistry>();
    mApi      = std::make_unique<api::DearOreUIApi>(*mRegistry, mCapabilities, logger);

    logger.info("lifecycle", "load")
        .withField("stage", "2")
        .withField("target", "win-x64")
        .emit();

    mInitialized = true;
    return true;
}

bool Runtime::enable() {
    if (!mInitialized) return false;
    if (mEnabled) return true;

    auto& logger = diagnostic::globalLogger();
    logger.info("lifecycle", "enable").emit();

    bool result = true;
    if (mConfig.enableHooks) {
        result = hook::installStage0OreUIHooks();
    }
    logger.info("status", result ? "hooks_installed" : "hooks_unavailable")
        .emit();

    if (mApi != nullptr) {
        mApi->setReady(result);
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

    if (mRegistry != nullptr) {
        mRegistry->clear();
    }

    if (mConfig.enableHooks) {
        poc::stopStage1Navigation();
    }
    auto hooksRemoved = mConfig.enableHooks ? hook::uninstallStage0OreUIHooks() : true;

    logger.info("lifecycle", "disable").emit();
    logger.info("status", hooksRemoved ? "hooks_removed" : "hooks_remove_failed")
        .emit();

    if (mConfig.enableStage0Compatibility) {
        diagnostic::resetStage0Session();
    }

    logger.flush();

    mApi.reset();
    mRegistry.reset();
    mEnabled      = false;
    mInitialized  = false;
    return hooksRemoved;
}

diagnostic::DiagnosticLogger& Runtime::diagnostics() {
    return diagnostic::globalLogger();
}

capability::ICapabilityQuery& Runtime::capabilities() {
    return mCapabilities;
}

api::IDearOreUIApi* Runtime::api() {
    return mApi.get();
}

} // namespace dearoreui::runtime
