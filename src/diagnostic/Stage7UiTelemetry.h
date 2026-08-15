#pragma once

#include "api/manifest/UiManifest.h"
#include "api/types/Id.h"
#include "api/types/Page.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace dearoreui::diagnostic {

void recordStage7UiRegistered(
    api::ModId            modId,
    std::string_view      modNamespace,
    std::string_view      uiId,
    api::UiKind           kind
);

void recordStage7UiUnregistered(
    api::RegistrationHandle handle,
    std::string_view        modNamespace,
    std::string_view        uiId
);

void recordStage7UiPlanned(
    api::ContextId   contextId,
    api::PageScope   scope,
    std::size_t      mounted,
    std::size_t      skipped,
    std::size_t      blocked
);

void recordStage7UiMounted(
    api::ContextId   contextId,
    std::string_view modNamespace,
    std::string_view uiId,
    std::string_view containerId
);

void recordStage7UiFailed(
    api::ContextId   contextId,
    std::string_view modNamespace,
    std::string_view uiId,
    std::string_view reason
);

void recordStage7UiUnmounted(
    api::ContextId   contextId,
    std::string_view modNamespace,
    std::string_view uiId
);

// Stage 7.1: JS-side feedback reported through cohtml::View::BindCall.
// The report is a plain text string the bootstrap script sends back to prove
// that ExecuteScript actually ran inside the Coherent engine.
void recordStage7JsReport(std::string_view report);

} // namespace dearoreui::diagnostic
