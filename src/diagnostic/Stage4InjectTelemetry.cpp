#include "diagnostic/Stage4InjectTelemetry.h"

#include "diagnostic/DiagnosticLogger.h"
#include "diagnostic/Stage0TelemetryCompat.h"

namespace dearoreui::diagnostic {

void recordStage4SnapshotCaptured(
    api::ContextId                    id,
    api::PageInfo const&              info,
    source::PageSourceSnapshot const& snapshot
) {
    static_cast<void>(info);

    auto& logger = globalLogger();
    logger.info("source", "snapshot_captured")
        .withContext(id)
        .withField("text_resources", std::to_string(snapshot.textResources.size()))
        .withField("binary_resources", std::to_string(snapshot.binaryResources.size()))
        .withField("partial", snapshot.partial ? "true" : "false")
        .withField("errors", std::to_string(snapshot.errors.size()))
        .emit();
}

void recordStage4ResourceIndexBuilt(api::ContextId id, std::size_t locationCount) {
    auto& logger = globalLogger();
    logger.info("resource", "index_built").withContext(id).withField("locations", std::to_string(locationCount)).emit();
}

void recordStage4InjectSubmitted(api::ContextId id, inject::InjectionReport const& report) {
    auto& logger = globalLogger();
    logger.info("inject", "submitted")
        .withContext(id)
        .withField("success", report.success ? "true" : "false")
        .withField("scripts", std::to_string(report.injectedScripts.size()))
        .withField("stylesheets", std::to_string(report.injectedStyleSheets.size()))
        .withField("errors", std::to_string(report.errors.size()))
        .emit();

    for (auto const& error : report.errors) {
        logger.warning("inject", "error").withContext(id).withError(error.code).withMessage(error.message).emit();
    }
}

} // namespace dearoreui::diagnostic
