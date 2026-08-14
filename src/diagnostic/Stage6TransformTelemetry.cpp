#include <utility>

#include "diagnostic/Stage6TransformTelemetry.h"

#include "diagnostic/DiagnosticLogger.h"

namespace dearoreui::diagnostic {

namespace {

[[nodiscard]] std::string boolString(bool value) { return value ? "true" : "false"; }

} // namespace

void recordStage6ModRegistered(api::ModId id, std::string_view modNamespace, std::size_t dependencyCount) {
    globalLogger()
        .info("stage6", "mod_registered")
        .withMod(std::move(id))
        .withField("namespace", std::string{modNamespace})
        .withField("dependency_count", std::to_string(dependencyCount))
        .emit();
}

void recordStage6ModUnregistered(api::ModId id, std::size_t removedEntryCount) {
    globalLogger()
        .info("stage6", "mod_unregistered")
        .withMod(std::move(id))
        .withField("removed_entry_count", std::to_string(removedEntryCount))
        .emit();
}

void recordStage6PlanBuilt(
    api::ContextId contextId,
    std::size_t    operationCount,
    std::size_t    modOrderCount,
    std::size_t    problemCount,
    std::size_t    conflictCount
) {
    globalLogger()
        .info("stage6", "plan_built")
        .withContext(contextId)
        .withField("operation_count", std::to_string(operationCount))
        .withField("mod_order_count", std::to_string(modOrderCount))
        .withField("problem_count", std::to_string(problemCount))
        .withField("conflict_count", std::to_string(conflictCount))
        .emit();
}

void recordStage6ReportSubmitted(
    api::ContextId contextId,
    std::size_t    applied,
    std::size_t    skipped,
    std::size_t    blocked,
    bool           success
) {
    globalLogger()
        .info("stage6", "report_submitted")
        .withContext(contextId)
        .withField("applied", std::to_string(applied))
        .withField("skipped", std::to_string(skipped))
        .withField("blocked", std::to_string(blocked))
        .withField("success", boolString(success))
        .emit();
}

} // namespace dearoreui::diagnostic
