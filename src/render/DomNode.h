#pragma once

#include "api/types/DomNode.h"

namespace dearoreui::render {

// Compatibility alias. New external code must use api::DomNode / api::DomAttr.
using DomAttr = api::DomAttr;
using DomNode = api::DomNode;

} // namespace dearoreui::render