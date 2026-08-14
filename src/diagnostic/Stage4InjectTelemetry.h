#pragma once

#include "api/types/Id.h"
#include "api/types/Page.h"
#include "inject/InjectionReport.h"
#include "source/PageSourceSnapshot.h"

#include <string_view>

namespace dearoreui::diagnostic {

void recordStage4SnapshotCaptured(
    api::ContextId id, api::PageInfo const& info, source::PageSourceSnapshot const& snapshot
);

void recordStage4ResourceIndexBuilt(
    api::ContextId id, std::size_t locationCount
);

void recordStage4InjectSubmitted(
    api::ContextId id, inject::InjectionReport const& report
);

} // namespace dearoreui::diagnostic
