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
    }
    return "unknown";
}

struct ComponentSpec {
    ComponentKind             kind{ComponentKind::Panel};
    std::string               label;    // primary text (button/panel title/input hint)
    std::string               variant;  // button: primary/secondary/neutral/destructive/elevated
    bool                      disabled{false};
    std::vector<std::string>  events;   // "click" / "change" (declared for wiring)
    std::vector<ComponentSpec> children;
    std::vector<render::DomNode> body; // raw DOM content (panel/card body)
};

} // namespace dearoreui::component
