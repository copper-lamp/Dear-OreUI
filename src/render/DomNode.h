#pragma once

#include <string>
#include <utility>
#include <vector>

namespace dearoreui::render {

struct DomAttr {
    std::string name;
    std::string value;
};

// A minimal DOM node tree used to generate cohtml-safe CSSOM construction
// scripts. `style` holds the full cssText declaration string: cohtml drops
// style="" set via innerHTML / setAttribute, and element.style.cssText is the
// only reliable styling channel (Stage 7.1 verified on the real engine).
struct DomNode {
    std::string          tag;      // element tag name; empty => treated as div
    std::string          style;    // cssText declaration (e.g. "color:#fff;")
    std::vector<DomAttr> attrs;    // non-style attributes (class/id/...)
    std::string          text;     // text content (leaf nodes only)
    std::vector<DomNode> children; // child nodes
    // M8.1.2: per-state cssText variants for interactive components.
    // To keep the injected script small (cohtml ExecuteScript silently drops
    // large scripts), `stateStyles` stores only the state-specific texture
    // cssText (border-image...), and `baseStyle` holds the shared non-texture
    // part. The bootstrap applies `baseStyle + stateStyles[state]` on switch;
    // `style` holds the full cssText of the effective state for initial render.
    std::string          baseStyle;
    std::vector<std::pair<std::string, std::string>> stateStyles;
};

} // namespace dearoreui::render
