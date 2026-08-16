#pragma once

#include "component/IAssetResolver.h"

namespace dearoreui::component {

// Environment 2 (offline toolchain: App design tool / web docs / browser
// preview): textures are served from a local snapshot bundle extracted from
// gui/dist/hbui. Minimal implementation for M8.1.1 — rewrites the runtime
// /hbui/ prefix into a local relative path; the theme-css / font-face
// payloads are filled by M8.1.3 (self-contained preview page).
class BundleAssetResolver final : public IAssetResolver {
public:
    [[nodiscard]] std::string resolveTexture(TextureSpec const& texture) const override;
    [[nodiscard]] std::string resolveThemeCss() const override { return {}; }
    [[nodiscard]] std::string resolveFontFace() const override { return {}; }
};

} // namespace dearoreui::component
