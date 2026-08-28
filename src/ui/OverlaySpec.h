#pragma once

#include "api/types/Id.h"
#include "api/types/DomNode.h"
#include "ui/UiManifest.h"

#include <string>
#include <vector>

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
    // M8.1.2: pre-rendered DomNode forest (component-registered UIs only).
    // Injection prefers this over parsing htmlBody so per-state cssText
    // (stateStyles) survives to the bootstrap.
    std::vector<api::DomNode> domNodes;
    std::vector<std::string>     scripts;
    std::vector<std::string>     styles;
};

} // namespace dearoreui::ui
