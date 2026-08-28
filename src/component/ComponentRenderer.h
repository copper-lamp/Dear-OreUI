#pragma once

#include "api/types/ComponentSpec.h"
#include "component/IAssetResolver.h"
#include "component/ThemeTokens.h"
#include "component/VanillaAssetResolver.h"
#include "api/types/DomNode.h"

#include <vector>

namespace dearoreui::component {

// Default resolver for the game runtime: textures are served from the game's
// own /hbui/assets/... paths (already loaded by the OreUI document).
[[nodiscard]] VanillaAssetResolver const& defaultAssetResolver();

// Renders a declarative ComponentSpec into an api::DomNode forest using the
// given theme tokens and asset resolver. The result is serialized by the
// universal renderer and built through CSSOM on the page, so components share
// the exact same rendering path as raw htmlBody overlays. The renderer emits
// concrete CSS (url(...) resolved through `resolver`), so runtime and offline
// toolchain produce visually identical DOM (stage 8.1 plan 2.2).
[[nodiscard]] std::vector<api::DomNode> renderComponent(
    api::ComponentSpec const& spec,
    ThemeTokens const&    theme    = defaultThemeTokens(),
    IAssetResolver const& resolver = defaultAssetResolver()
);

// Serializes an already-rendered DomNode forest back into an htmlBody string
// (DomNode -> HTML). R4: components render ONCE into DomNodes; htmlBody is
// only a derived view kept for legacy transform/export compatibility — it is
// never parsed back into DOM (injection prefers domNodes).
[[nodiscard]] std::string serializeDomNodesToHtml(std::vector<api::DomNode> const& nodes);

// Convenience wrapper: renderComponent(spec) then serializeDomNodesToHtml.
// Kept for callers that need a one-shot htmlBody (e.g. component tests).
[[nodiscard]] std::string renderComponentToHtml(
    api::ComponentSpec const& spec,
    ThemeTokens const&    theme    = defaultThemeTokens(),
    IAssetResolver const& resolver = defaultAssetResolver()
);

} // namespace dearoreui::component