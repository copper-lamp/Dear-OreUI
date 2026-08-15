#pragma once

#include "component/ComponentSpec.h"
#include "component/ThemeTokens.h"
#include "render/DomNode.h"

#include <vector>

namespace dearoreui::component {

// Renders a declarative ComponentSpec into a render::DomNode forest using the
// given theme tokens. The result is serialized by the universal renderer and
// built through CSSOM on the page, so components share the exact same
// rendering path as raw htmlBody overlays.
[[nodiscard]] std::vector<render::DomNode> renderComponent(
    ComponentSpec const& spec,
    ThemeTokens const&   theme = defaultThemeTokens()
);

// Renders a ComponentSpec into an htmlBody string (DomNode -> HTML). This lets
// registerComponent reuse the existing htmlBody injection pipeline unchanged.
[[nodiscard]] std::string renderComponentToHtml(
    ComponentSpec const& spec,
    ThemeTokens const&   theme = defaultThemeTokens()
);

} // namespace dearoreui::component
