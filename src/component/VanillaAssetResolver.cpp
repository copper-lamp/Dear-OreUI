#include "component/VanillaAssetResolver.h"

namespace dearoreui::component {

std::string VanillaAssetResolver::resolveTexture(TextureSpec const& texture) const {
    // TextureSpec.source is the theme CSS url(...) value verbatim
    // (e.g. /hbui/assets/pressable_neutral_default-xxx.png), already absolute
    // in the running OreUI document.
    return texture.source;
}

} // namespace dearoreui::component
