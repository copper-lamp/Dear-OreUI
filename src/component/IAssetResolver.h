#pragma once

#include "component/TextureSpec.h"

#include <string>

namespace dearoreui::component {

// Resolves vanilla texture specs into concrete URLs for a target environment.
// The ComponentRenderer emits concrete CSS (url(...) resolved through this
// interface), so the game runtime and the offline toolchain produce visually
// identical DOM with a single, environment-agnostic component implementation
// (stage 8.1 plan 2.2, design 3.2).
class IAssetResolver {
public:
    virtual ~IAssetResolver() = default;

    // Resolves a texture to a renderable URL:
    //  - runtime (VanillaAssetResolver): "/hbui/assets/xxx.png" (already loaded
    //    by the OreUI page, zero asset packaging).
    //  - offline (BundleAssetResolver):  local relative path into the snapshot
    //    bundle ("./assets/xxx.png").
    [[nodiscard]] virtual std::string resolveTexture(TextureSpec const& texture) const = 0;

    // Offline builds: inlined theme CSS. Runtime: "" (the document already
    // loads menus/gameplay-theme). Filled by M8.1.3.
    [[nodiscard]] virtual std::string resolveThemeCss() const = 0;

    // Offline builds: inlined @font-face. Runtime: "". Filled by M8.1.3.
    [[nodiscard]] virtual std::string resolveFontFace() const = 0;
};

} // namespace dearoreui::component
