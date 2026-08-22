#pragma once

#include "component/TextureSpec.h"

#include <cstddef>
#include <map>
#include <string>
#include <string_view>

namespace dearoreui::component {

// Named index into ThemeTokens::fontSizes, aligned with the vanilla
// --fontSizes0..7 scale.
namespace FontSize {
inline constexpr std::size_t Tiny    = 0; // --fontSizes0
inline constexpr std::size_t Small   = 1; // --fontSizes1
inline constexpr std::size_t Medium  = 2; // --fontSizes2
inline constexpr std::size_t Large   = 3; // --fontSizes3
inline constexpr std::size_t XLarge  = 4; // --fontSizes4
inline constexpr std::size_t Heading = 5; // --fontSizes5
inline constexpr std::size_t Display = 6; // --fontSizes6
inline constexpr std::size_t Giant   = 7; // --fontSizes7
} // namespace FontSize

// Default "menus" theme tokens extracted from the vanilla OreUI
// menus-theme-*.css / gameplay-theme-*.css (gui/dist/hbui). The vanilla theme
// relies on 9-slice border-image PNGs referenced by semantic texture keys;
// the renderer resolves them through VanillaAssets unless overridden via the
// texture override table below.
struct ThemeTokens {
    // --colors* (menus-theme :root)
    std::string colorPrimary{"#3c8527"};     // --colorsPrimary
    std::string colorSecondary{"#d0d1d4"};   // --colorsSecondary
    std::string colorDestructive{"#ca3636"}; // --colorsDestructive
    std::string colorText{"#ffffff"};        // --colorsText
    std::string colorMuted{"#b1b2b5"};       // --colorsMuted1
    std::string colorDisabled{"#d0d1d4"};    // --colorsDisabled
    std::string colorPanel{"rgba(0,0,0,0.72)"};

    // --fonts*
    std::string fontHeading{"Minecraft Ten v2"};     // --fontsHeading
    std::string fontSubheading{"Minecraft Five v2"}; // --fontsSubHeading
    std::string fontUi{"Minecraft Seven v2"};        // --fontsUi
    std::string fontBody{"Noto Sans"};               // --fontsBody

    // --fontSizes0..7 in px
    int fontSizes[8]{10, 14, 16, 20, 24, 32, 48, 64};
    // --lineHeights0..5 in px
    int lineHeights[6]{20, 24, 28, 40, 60, 80};
    // --letterSpacings0..1 in px
    int letterSpacings[2]{4, 8};
    // --space0..7 in px
    int spaces[8]{4, 8, 12, 16, 20, 24, 32, 64};

    // Texture override table keyed by semantic key (<family>.<variant>.<state>).
    // Empty by default: the renderer falls back to VanillaAssets::texture(key).
    // Mods override individual textures through setTheme() (design 3.5).
    std::map<std::string, TextureSpec, std::less<>> textures;

    // Returns the override texture for `key`, or nullptr when the renderer
    // should fall back to the vanilla table.
    [[nodiscard]] TextureSpec const* texture(std::string_view key) const {
        auto const it = textures.find(key);
        return it == textures.end() ? nullptr : &it->second;
    }
};

[[nodiscard]] ThemeTokens const& defaultThemeTokens();

} // namespace dearoreui::component
