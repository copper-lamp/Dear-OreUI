#pragma once

#include <string>

namespace dearoreui::component {

// A 9-slice border-image texture extracted verbatim from the vanilla OreUI
// theme CSS (menus-theme-*.css / gameplay-theme-*.css in gui/dist/hbui).
// `source` is the url(...) target; slice/width/outset are the
// border-image-slice/width/outset values. The renderer combines a TextureSpec
// with an IAssetResolver to emit concrete CSS (stage 8.1 plan 2.2).
struct TextureSpec {
    std::string source; // e.g. /hbui/assets/pressable_neutral_default-xxx.png
    std::string slice;  // border-image-slice, e.g. "2 2 2 2 fill"
    std::string width;  // border-image-width, e.g. "0.4rem 0.4rem 0.4rem 0.4rem"
    std::string outset; // border-image-outset, e.g. "0 0 0 0"
};

// A key icon (keyIcon component): background-image texture plus its intrinsic
// display size (--buttonIconKeyboard<Name>Width/Height / --buttonIconMouse*).
struct KeyIconSpec {
    std::string source; // e.g. /hbui/assets/A-xxx.png
    std::string width;  // e.g. "2.4rem"
    std::string height; // e.g. "2.4rem"
};

} // namespace dearoreui::component
