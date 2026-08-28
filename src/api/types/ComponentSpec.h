#pragma once

#include "api/types/DomNode.h"

#include <string>
#include <vector>

namespace dearoreui::api {

// Declarative component spec (Public API). A component is rendered into a
// DOM tree by the component renderer using theme tokens; the resulting tree
// flows through the universal CSSOM renderer, so Mod overlays and library
// components share one rendering path.
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
    // Stage 8.1.4: layout (L), composite (B), navigation (N), interaction (I),
    // data (D) components.
    Stack,
    Grid,
    ScrollView,
    Section,
    Spacer,
    Modal,
    Menu,
    ScrollingList,
    Dropdown,
    Form,
    NavigationBar,
    Toast,
    SearchField,
    Toggle,
    Breadcrumb,
    Pager,
    TextArea,
    Slider,
    Stepper,
    Picker,
    Icon,
    Image,
    Badge,
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
    case ComponentKind::Stack:
        return "stack";
    case ComponentKind::Grid:
        return "grid";
    case ComponentKind::ScrollView:
        return "scrollView";
    case ComponentKind::Section:
        return "section";
    case ComponentKind::Spacer:
        return "spacer";
    case ComponentKind::Modal:
        return "modal";
    case ComponentKind::Menu:
        return "menu";
    case ComponentKind::ScrollingList:
        return "scrollingList";
    case ComponentKind::Dropdown:
        return "dropdown";
    case ComponentKind::Form:
        return "form";
    case ComponentKind::NavigationBar:
        return "navigationBar";
    case ComponentKind::Toast:
        return "toast";
    case ComponentKind::SearchField:
        return "searchField";
    case ComponentKind::Toggle:
        return "toggle";
    case ComponentKind::Breadcrumb:
        return "breadcrumb";
    case ComponentKind::Pager:
        return "pager";
    case ComponentKind::TextArea:
        return "textArea";
    case ComponentKind::Slider:
        return "slider";
    case ComponentKind::Stepper:
        return "stepper";
    case ComponentKind::Picker:
        return "picker";
    case ComponentKind::Icon:
        return "icon";
    case ComponentKind::Image:
        return "image";
    case ComponentKind::Badge:
        return "badge";
    }
    return "unknown";
}

struct ComponentSpec {
    ComponentKind kind{ComponentKind::Panel};
    std::string   id;              // declarative layout anchor (R2): emitted as the
                                   // rendered root node's `id` attr so page scripts
                                   // can reference component-tree nodes by id.
    std::string   label;           // primary text (button/panel title/input hint)
    std::string   variant;         // button: primary/secondary/neutral/destructive
    std::string   style{"normal"}; // button: normal|elevated; panel: default|dark|furnace|chest|...
    bool          disabled{false};
    std::string   state{"default"};      // default|hovered|focused|pressed|disabled (+pressedFocused/disabledFocused)
    std::vector<std::string>     events; // "click" / "change" (declared for wiring)
    std::vector<ComponentSpec>   children;
    std::vector<DomNode>         body; // raw DOM content (panel/card body)
    // Stage 8.1.4: layout / composite / data component fields.
    std::string orientation{"column"}; // stack: column|row
    int         columns{1};            // grid: column count
    std::string icon;                  // icon/image: asset key (VanillaAssets::icon)
    std::string src;                   // image: explicit URL (overrides icon)
    std::string value;                 // slider/stepper/pager: current value
    std::string min, max;              // slider: range
};

} // namespace dearoreui::api