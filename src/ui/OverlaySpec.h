#pragma once

#include "api/types/Id.h"
#include "ui/UiManifest.h"

#include <string>

namespace dearoreui::ui {

struct OverlaySpec {
    api::RegistrationHandle handle;
    std::string             modNamespace;
    std::string             uiId;
    api::UiKind             kind{api::UiKind::Overlay};
    std::string             containerId;
    api::UiAnchor           anchor{api::UiAnchor::FullScreen};
    bool                    pointerEvents{false};
    std::string             htmlBody;
    std::vector<std::string> scripts;
    std::vector<std::string> styles;
};

} // namespace dearoreui::ui
