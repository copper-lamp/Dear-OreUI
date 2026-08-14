#pragma once

#include "api/types/Error.h"
#include "api/types/Id.h"

#include <cstddef>
#include <string_view>

namespace dearoreui::diagnostic {

void recordStage6ModRegistered(api::ModId id, std::string_view modNamespace, std::size_t dependencyCount);
void recordStage6ModUnregistered(api::ModId id, std::size_t removedEntryCount);

void recordStage6PlanBuilt(
    api::ContextId contextId,
    std::size_t    operationCount,
    std::size_t    modOrderCount,
    std::size_t    problemCount,
    std::size_t    conflictCount
);

void recordStage6ReportSubmitted(
    api::ContextId contextId,
    std::size_t    applied,
    std::size_t    skipped,
    std::size_t    blocked,
    bool           success
);

} // namespace dearoreui::diagnostic
