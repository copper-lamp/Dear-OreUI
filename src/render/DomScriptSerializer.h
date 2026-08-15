#pragma once

#include "render/DomNode.h"

#include <string>
#include <vector>

namespace dearoreui::render {

// Serializes a DomNode forest into a compact JS array literal consumed by the
// bootstrap renderer (dearOreUiBuildDom). Each node becomes an object:
//   {t:"div", s:"...cssText...", a:[["class","x"],...], x:"text", c:[children]}
// Only non-empty fields are emitted so the generated script stays small.
// The output is an expression, not a statement: callers embed it directly,
// e.g. `var nodes = <serialize...>;`.
[[nodiscard]] std::string serializeDomForest(std::vector<DomNode> const& nodes);

} // namespace dearoreui::render
