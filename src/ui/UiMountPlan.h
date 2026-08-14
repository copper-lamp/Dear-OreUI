#pragma once

#include "api/types/Error.h"
#include "api/types/Id.h"
#include "ui/OverlaySpec.h"
#include "ui/UiManifest.h"

#include <string>
#include <vector>

namespace dearoreui::ui {

enum class UiMountDecision {
    Mount,
    Skip,
    Blocked,
};

struct UiMountItem {
    api::RegistrationHandle handle;
    api::UiManifest         manifest;
    UiMountDecision         decision{UiMountDecision::Mount};
    std::string             reason;
    OverlaySpec             spec;
};

struct UiMountPlan {
    api::ContextId           contextId;
    api::PageScope           pageScope{api::PageScope::Any};
    std::vector<UiMountItem> items;
    std::size_t              mounted{0};
    std::size_t              skipped{0};
    std::size_t              blocked{0};
    std::vector<api::Error>  errors;
    bool                     success{true};
};

} // namespace dearoreui::ui
