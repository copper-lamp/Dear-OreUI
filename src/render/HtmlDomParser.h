#pragma once

#include "render/DomNode.h"

#include <string>
#include <string_view>
#include <vector>

namespace dearoreui::render {

// Parses a small, safe HTML fragment into a DomNode forest.
//
// This is deliberately NOT a full HTML5 parser. It targets the subset of HTML
// a DearOreUI overlay/component realistically uses:
//   - start/end tags, attributes (single or double quoted)
//   - void elements (br, img, input, hr)
//   - text content (whitespace preserved)
//   - nested children
//
// The style attribute is extracted into DomNode::style (cssText) because
// cohtml discards style="" set through innerHTML. Unbalanced tags cause the
// parser to stop and return what it has (callers treat structural failure as
// a render fallback, never as a crash).
[[nodiscard]] std::vector<DomNode> parseHtmlFragment(std::string_view html);

} // namespace dearoreui::render
