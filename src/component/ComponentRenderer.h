#pragma once

#include "component/ComponentSpec.h"
#include "component/IAssetResolver.h"
#include "component/ThemeTokens.h"
#include "component/VanillaAssetResolver.h"
#include "render/DomNode.h"

#include <vector>

namespace dearoreui::component {

// Default resolver for the game runtime: textures are served from the game's
// own /hbui/assets/... paths (already loaded by the OreUI document).
[[nodiscard]] VanillaAssetResolver const& defaultAssetResolver();

// Renders a declarative ComponentSpec into a render::DomNode forest using the
// given theme tokens and asset resolver. The result is serialized by the
// universal renderer and built through CSSOM on the page, so components share
// the exact same rendering path as raw htmlBody overlays. The renderer emits
// concrete CSS (url(...) resolved through `resolver`), so runtime and offline
// toolchain produce visually identical DOM (stage 8.1 plan 2.2).
[[nodiscard]] std::vector<render::DomNode> renderComponent(
    ComponentSpec const&   spec,
    ThemeTokens const&     theme    = defaultThemeTokens(),
    IAssetResolver const&  resolver = defaultAssetResolver()
);

// Renders a ComponentSpec into an htmlBody string (DomNode -> HTML). This lets
// registerComponent reuse the existing htmlBody injection pipeline unchanged.
[[nodiscard]] std::string renderComponentToHtml(
    ComponentSpec const&   spec,
    ThemeTokens const&     theme    = defaultThemeTokens(),
    IAssetResolver const&  resolver = defaultAssetResolver()
);

} // namespace dearoreui::component