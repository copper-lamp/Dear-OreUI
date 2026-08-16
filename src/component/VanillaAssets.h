#pragma once

#include "component/TextureSpec.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace dearoreui::component {

// Semantic texture directory extracted from the vanilla OreUI theme CSS
// (menus-theme-*.css :root / gameplay-theme-*.css). Keys use the
// `<family>.<variant>.<state>` naming defined by the stage 8.1 plan (2.3) and
// map 1:1 to the theme variables, so the renderer can switch textures purely
// by key while ThemeTokens provides override hooks (S2).
namespace VanillaAssets {

// Border-image texture for a semantic key, or nullptr if the key is unknown.
[[nodiscard]] TextureSpec const* texture(std::string_view key);

// Background-image key icon (keyIcon component) by name, or nullptr.
[[nodiscard]] KeyIconSpec const* keyIcon(std::string_view name);

[[nodiscard]] std::size_t textureCount();
[[nodiscard]] std::size_t keyIconCount();

// Test helpers: complete key/name listing for integrity assertions.
[[nodiscard]] std::vector<std::string> textureKeys();
[[nodiscard]] std::vector<std::string> keyIconNames();

} // namespace VanillaAssets
} // namespace dearoreui::component
