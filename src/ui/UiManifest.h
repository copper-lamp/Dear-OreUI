#pragma once

#include "api/manifest/Dependency.h"
#include "api/manifest/Permission.h"
#include "api/types/Page.h"

#include <string>
#include <vector>

namespace dearoreui::api {

enum class UiKind {
    Overlay,
    Panel,
    Button,
    Page,
};

[[nodiscard]] constexpr std::string_view uiKindName(UiKind kind) {
    switch (kind) {
    case UiKind::Overlay:
        return "overlay";
    case UiKind::Panel:
        return "panel";
    case UiKind::Button:
        return "button";
    case UiKind::Page:
        return "page";
    }
    return "unknown";
}

enum class UiAnchor {
    TopLeft,
    TopCenter,
    TopRight,
    CenterLeft,
    Center,
    CenterRight,
    BottomLeft,
    BottomCenter,
    BottomRight,
    FullScreen,
};

struct UiManifest {
    std::string              modNamespace;
    std::string              id;
    UiKind                   kind{UiKind::Overlay};
    std::vector<PageScope>   pageScopes;
    UiAnchor                 anchor{UiAnchor::FullScreen};
    bool                     pointerEvents{false};
    std::string              containerId;
    std::vector<std::string> scripts;
    std::vector<std::string> styles;
    std::vector<Dependency>  dependencies;
    std::vector<std::string> conflicts;
    PermissionSet            permissions;
    std::string              fingerprint;

    [[nodiscard]] bool operator==(UiManifest const& other) const {
        return modNamespace == other.modNamespace && id == other.id && kind == other.kind
            && containerId == other.containerId && fingerprint == other.fingerprint;
    }

    [[nodiscard]] bool operator!=(UiManifest const& other) const { return !(*this == other); }
};

[[nodiscard]] inline std::string makeUiContainerId(std::string_view modNamespace, UiKind kind, std::string_view id) {
    std::string result  = "dearoreui-";
    result             += modNamespace;
    result             += "-";
    result             += uiKindName(kind);
    result             += "-";
    result             += id;
    return result;
}

} // namespace dearoreui::api
