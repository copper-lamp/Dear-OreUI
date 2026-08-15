#pragma once

#include <string>

namespace dearoreui::component {

// Default "menus" theme tokens extracted from the vanilla OreUI
// menus-theme-*.css (gui/dist/hbui). The vanilla theme relies on 9-slice
// border-image PNGs; the DearOreUI default theme approximates the same look
// with plain CSS (colors/fonts/spacing) that cohtml renders reliably.
struct ThemeTokens {
    std::string colorPrimary{"#3c8527"};     // --colorsPrimary
    std::string colorSecondary{"#d0d1d4"};   // --colorsSecondary
    std::string colorDestructive{"#ca3636"}; // --colorsDestructive
    std::string colorText{"#ffffff"};        // --colorsText
    std::string colorMuted{"#b1b2b5"};       // --colorsMuted1
    std::string colorDisabled{"#7a7a7a"};
    std::string colorPanel{"rgba(0,0,0,0.72)"};

    std::string fontHeading{"Minecraft Ten v2"};   // --fontsHeading
    std::string fontSubheading{"Minecraft Five v2"}; // --fontsSubHeading
    std::string fontUi{"Minecraft Seven v2"};        // --fontsUi
    std::string fontBody{"Noto Sans"};               // --fontsBody

    // --fontSizes 0..7 in px
    int fontSizeTiny{10};
    int fontSizeSmall{14};
    int fontSizeMedium{16};
    int fontSizeLarge{20};
    int fontSizeHeading{32};
};

[[nodiscard]] ThemeTokens const& defaultThemeTokens();

} // namespace dearoreui::component
