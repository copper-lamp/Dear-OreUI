#pragma once

#include "api/types/ComponentSpec.h"

namespace dearoreui::component {

// Compatibility aliases. New external code must use api::ComponentSpec /
// api::ComponentKind / api::componentKindName.
using ComponentKind = api::ComponentKind;
using ComponentSpec = api::ComponentSpec;

[[nodiscard]] constexpr std::string_view componentKindName(api::ComponentKind kind) {
    return api::componentKindName(kind);
}

} // namespace dearoreui::component