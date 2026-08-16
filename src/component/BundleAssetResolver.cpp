#include "component/BundleAssetResolver.h"

#include <string_view>

namespace dearoreui::component {

std::string BundleAssetResolver::resolveTexture(TextureSpec const& texture) const {
    // Rewrite the runtime /hbui/assets/... URL into a local relative path so
    // offline previews (App / web docs) resolve against their snapshot bundle.
    constexpr std::string_view kHbuiPrefix = "/hbui/";
    if (texture.source.rfind(kHbuiPrefix, 0) == 0) {
        return "./" + texture.source.substr(kHbuiPrefix.size());
    }
    return texture.source;
}

} // namespace dearoreui::component
