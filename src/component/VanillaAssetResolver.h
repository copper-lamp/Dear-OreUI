#pragma once

#include "component/IAssetResolver.h"

namespace dearoreui::component {

// Environment 1 (game runtime, default): textures are served from the game's
// own /hbui/assets/... paths, which the OreUI document has already loaded
// (menus/gameplay themes + atlas). TextureSpec.source already carries the
// absolute /hbui/... URL, so the resolver passes it through unchanged and the
// mod ships zero assets.
class VanillaAssetResolver final : public IAssetResolver {
public:
    [[nodiscard]] std::string resolveTexture(TextureSpec const& texture) const override;
    [[nodiscard]] std::string resolveThemeCss() const override { return {}; }
    [[nodiscard]] std::string resolveFontFace() const override { return {}; }
};

} // namespace dearoreui::component
