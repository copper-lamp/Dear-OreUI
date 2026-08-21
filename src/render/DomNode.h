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
    // M8.1.2: per-state cssText variants for interactive components
    // (state name -> full cssText). The bootstrap swaps element.style.cssText
    // on hover/pressed/focused events; `style` holds the default-state cssText.
    std::vector<std::pair<std::string, std::string>> stateStyles;
};

} // namespace dearoreui::render
