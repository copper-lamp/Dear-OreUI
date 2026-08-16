#pragma once

#include "render/DomNode.h"

#include <string>
#include <vector>

namespace dearoreui::component {

// Declarative component spec (Stage 8). A component is rendered into a
// render::DomNode by ComponentRenderer using the theme tokens; the resulting
// tree flows through the universal CSSOM renderer (render/ module), so Mod
// overlays and library components share one rendering path.
enum class ComponentKind {
    Button,
    Panel,
    Text,
    Card,
    ListItem,
    Input,
    TabBar,
    Divider,
    Tooltip,
    ContainerSlot,
    KeyIcon,
    Bubble,
    FilterBar,
    Progress,
};

[[nodiscard]] constexpr std::string_view componentKindName(ComponentKind kind) {
    switch (kind) {
    case ComponentKind::Button:
        return "button";
    case ComponentKind::Panel:
        return "panel";
    case ComponentKind::Text:
        return "text";
    case ComponentKind::Card:
        return "card";
    case ComponentKind::ListItem:
        return "listItem";
    case ComponentKind::Input:
        return "input";
    case ComponentKind::TabBar:
        return "tabBar";
    case ComponentKind::Divider:
        return "divider";
    case ComponentKind::Tooltip:
        return "tooltip";
    case ComponentKind::ContainerSlot:
        return "containerSlot";
    case ComponentKind::KeyIcon:
        return "keyIcon";
    case ComponentKind::Bubble:
        return "bubble";
    case ComponentKind::FilterBar:
        return "filterBar";
    case ComponentKind::Progress:
        return "progress";
    }
    return "unknown";
}

struct ComponentSpec {
    ComponentKind             kind{ComponentKind::Panel};
    std::string               label;    // primary text (button/panel title/input hint)
    std::string               variant;  // button: primary/secondary/neutral/destructive
    std::string               style{"normal"}; // button: normal|elevated; panel: default|dark|furnace|chest|...
    bool                      disabled{false};
    std::string               state{"default"}; // default|hovered|focused|pressed|disabled (+pressedFocused/disabledFocused)
    std::vector<std::string>  events;   // "click" / "change" (declared for wiring)
    std::vector<ComponentSpec> children;
    std::vector<render::DomNode> body; // raw DOM content (panel/card body)
};

} // namespace dearoreui::component
