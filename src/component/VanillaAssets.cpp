#include "component/VanillaAssets.h"

#include <map>
#include <string>
#include <utility>

namespace dearoreui::component {
namespace {

// ---- geometry constants (extracted from menus/gameplay-theme CSS) ----------

// pressable (non-elevated) state geometry.
char const* const kSlicePressableDefault  = "2 2 2 2 fill";
char const* const kWidthPressableDefault  = "0.4rem 0.4rem 0.4rem 0.4rem";
char const* const kSlicePressableFocused  = "3 3 3 3 fill";
char const* const kWidthPressableFocused  = "0.6rem 0.6rem 0.6rem 0.6rem";
char const* const kOutsetPressableFocused = "0.2rem 0.2rem 0.2rem 0.2rem";
char const* const kSlicePressableDisabled = "1 1 1 1 fill";
char const* const kWidthPressableDisabled = "0.2rem 0.2rem 0.2rem 0.2rem";
char const* const kOutsetZero             = "0 0 0 0";

// pressable elevated state geometry (bottom edge thicker for the 3D lip).
char const* const kSliceElevatedDefault  = "2 2 4 2 fill";
char const* const kWidthElevatedDefault  = "0.4rem 0.4rem 0.8rem 0.4rem";
char const* const kSliceElevatedFocused  = "3 3 5 3 fill";
char const* const kWidthElevatedFocused  = "0.6rem 0.6rem 1rem 0.6rem";
char const* const kSliceElevatedDisabled = "1 1 3 1 fill";
char const* const kWidthElevatedDisabled = "0.2rem 0.2rem 0.6rem 0.2rem";

// pressable focused/pressed reuses the flat pressed slice/width/outset.
char const* const kSlicePressed = "2 2 2 2 fill";
char const* const kWidthPressed = "0.4rem 0.4rem 0.4rem 0.4rem";

[[nodiscard]] TextureSpec tex(
    std::string_view source,
    std::string_view slice,
    std::string_view width,
    std::string_view outset
) {
    return TextureSpec{std::string(source), std::string(slice), std::string(width), std::string(outset)};
}

using TextureTable = std::map<std::string, TextureSpec, std::less<>>;
using KeyIconTable = std::map<std::string, KeyIconSpec, std::less<>>;

TextureTable const& textureTable() {
    static TextureTable const table = [] {
        TextureTable m;
        auto add = [&m](std::string key, TextureSpec spec) { m.emplace(std::move(key), std::move(spec)); };

        // ---- pressable (menus-theme :root) ----
        add("pressable.neutral.default", tex("/hbui/assets/pressable_neutral_default-e45d49d1a163c007150f.png", kSlicePressableDefault, kWidthPressableDefault, kOutsetZero));
        add("pressable.neutral.hovered", tex("/hbui/assets/pressable_neutral_hovered-230f9ad5ccca66468c38.png", kSlicePressableDefault, kWidthPressableDefault, kOutsetZero));
        add("pressable.neutral.focused", tex("/hbui/assets/pressable_neutral_focused-c5a2eaeda7402012316c.png", kSlicePressableFocused, kWidthPressableFocused, kOutsetPressableFocused));
        add("pressable.neutral.pressed", tex("/hbui/assets/pressable_neutral_pressed-aad8b64fc0f155676218.png", kSlicePressed, kWidthPressed, kOutsetZero));
        add("pressable.neutral.disabled", tex("/hbui/assets/pressable_disabled-3555c5a3dc9f40fda713.png", kSlicePressableDisabled, kWidthPressableDisabled, kOutsetZero));
        add("pressable.primary.default", tex("/hbui/assets/pressable_primary_default-c4a5a4c1b74b2ff70122.png", kSlicePressableDefault, kWidthPressableDefault, kOutsetZero));
        add("pressable.primary.hovered", tex("/hbui/assets/pressable_primary_hovered-1c8d9125d478e583f435.png", kSlicePressableDefault, kWidthPressableDefault, kOutsetZero));
        add("pressable.primary.focused", tex("/hbui/assets/pressable_primary_focused-84b112b857049e079b29.png", kSlicePressableFocused, kWidthPressableFocused, kOutsetPressableFocused));
        add("pressable.primary.pressed", tex("/hbui/assets/pressable_primary_pressed-13b0f01c6c00e07322ca.png", kSlicePressed, kWidthPressed, kOutsetZero));
        add("pressable.primary.disabled", tex("/hbui/assets/pressable_disabled-3555c5a3dc9f40fda713.png", kSlicePressableDisabled, kWidthPressableDisabled, kOutsetZero));
        add("pressable.secondary.default", tex("/hbui/assets/pressable_secondary_default-cc6424da5b7759ebfbf5.png", kSlicePressableDefault, kWidthPressableDefault, kOutsetZero));
        add("pressable.secondary.hovered", tex("/hbui/assets/pressable_secondary_hovered-a18c14fc21f49a22efbf.png", kSlicePressableDefault, kWidthPressableDefault, kOutsetZero));
        add("pressable.secondary.focused", tex("/hbui/assets/pressable_secondary_focused-932363b4cf5981b70bc2.png", kSlicePressableFocused, kWidthPressableFocused, kOutsetPressableFocused));
        add("pressable.secondary.pressed", tex("/hbui/assets/pressable_secondary_pressed-87bba1faba891ebfa7fc.png", kSlicePressed, kWidthPressed, kOutsetZero));
        add("pressable.secondary.disabled", tex("/hbui/assets/pressable_disabled-3555c5a3dc9f40fda713.png", kSlicePressableDisabled, kWidthPressableDisabled, kOutsetZero));
        add("pressable.destructive.default", tex("/hbui/assets/pressable_destructive_default-afc23b5e2d0cd6814724.png", kSlicePressableDefault, kWidthPressableDefault, kOutsetZero));
        add("pressable.destructive.hovered", tex("/hbui/assets/pressable_destructive_hovered-13953e14ee36befae1c1.png", kSlicePressableDefault, kWidthPressableDefault, kOutsetZero));
        add("pressable.destructive.focused", tex("/hbui/assets/pressable_destructive_focused-3f550a8290ff176b432c.png", kSlicePressableFocused, kWidthPressableFocused, kOutsetPressableFocused));
        add("pressable.destructive.pressed", tex("/hbui/assets/pressable_destructive_pressed-d5d4943d1b774ef42f85.png", kSlicePressed, kWidthPressed, kOutsetZero));
        add("pressable.destructive.disabled", tex("/hbui/assets/pressable_disabled-3555c5a3dc9f40fda713.png", kSlicePressableDisabled, kWidthPressableDisabled, kOutsetZero));

        // ---- pressable.elevated (3D lip) ----
        add("pressable.elevated.neutral.default", tex("/hbui/assets/pressable_elevated_neutral_default-48cdf8535bfd48b47c2a.png", kSliceElevatedDefault, kWidthElevatedDefault, kOutsetZero));
        add("pressable.elevated.neutral.hovered", tex("/hbui/assets/pressable_elevated_neutral_hovered-dcbd873ad9e83d24dc7a.png", kSliceElevatedDefault, kWidthElevatedDefault, kOutsetZero));
        add("pressable.elevated.neutral.focused", tex("/hbui/assets/pressable_elevated_neutral_focused-b28fc482cadde858740c.png", kSliceElevatedFocused, kWidthElevatedFocused, kOutsetPressableFocused));
        add("pressable.elevated.neutral.pressed", tex("/hbui/assets/pressable_neutral_pressed-aad8b64fc0f155676218.png", kSlicePressed, kWidthPressed, kOutsetZero));
        add("pressable.elevated.neutral.disabled", tex("/hbui/assets/pressable_elevated_disabled-af3383540a78827c5a84.png", kSliceElevatedDisabled, kWidthElevatedDisabled, kOutsetZero));
        add("pressable.elevated.primary.default", tex("/hbui/assets/pressable_elevated_primary_default-7a6e2edf98a626b182c2.png", kSliceElevatedDefault, kWidthElevatedDefault, kOutsetZero));
        add("pressable.elevated.primary.hovered", tex("/hbui/assets/pressable_elevated_primary_hovered-bcb21ab092446cbcecb6.png", kSliceElevatedDefault, kWidthElevatedDefault, kOutsetZero));
        add("pressable.elevated.primary.focused", tex("/hbui/assets/pressable_elevated_primary_focused-cc7df0bf833f92bb08dd.png", kSliceElevatedFocused, kWidthElevatedFocused, kOutsetPressableFocused));
        add("pressable.elevated.primary.pressed", tex("/hbui/assets/pressable_primary_pressed-13b0f01c6c00e07322ca.png", kSlicePressed, kWidthPressed, kOutsetZero));
        add("pressable.elevated.primary.disabled", tex("/hbui/assets/pressable_elevated_disabled-af3383540a78827c5a84.png", kSliceElevatedDisabled, kWidthElevatedDisabled, kOutsetZero));
        add("pressable.elevated.secondary.default", tex("/hbui/assets/pressable_elevated_secondary_default-0aacffbfa726a8184fc5.png", kSliceElevatedDefault, kWidthElevatedDefault, kOutsetZero));
        add("pressable.elevated.secondary.hovered", tex("/hbui/assets/pressable_elevated_secondary_hovered-758970b376da9eb299da.png", kSliceElevatedDefault, kWidthElevatedDefault, kOutsetZero));
        add("pressable.elevated.secondary.focused", tex("/hbui/assets/pressable_elevated_secondary_focused-05d59cd4654230c9b01d.png", kSliceElevatedFocused, kWidthElevatedFocused, kOutsetPressableFocused));
        add("pressable.elevated.secondary.pressed", tex("/hbui/assets/pressable_secondary_pressed-87bba1faba891ebfa7fc.png", kSlicePressed, kWidthPressed, kOutsetZero));
        add("pressable.elevated.secondary.disabled", tex("/hbui/assets/pressable_elevated_disabled-af3383540a78827c5a84.png", kSliceElevatedDisabled, kWidthElevatedDisabled, kOutsetZero));
        add("pressable.elevated.destructive.default", tex("/hbui/assets/pressable_elevated_destructive_default-a5fb3f173720065fcc13.png", kSliceElevatedDefault, kWidthElevatedDefault, kOutsetZero));
        add("pressable.elevated.destructive.hovered", tex("/hbui/assets/pressable_elevated_destructive_hovered-63736c555d7fc6479755.png", kSliceElevatedDefault, kWidthElevatedDefault, kOutsetZero));
        add("pressable.elevated.destructive.focused", tex("/hbui/assets/pressable_elevated_destructive_focused-cfa496a1b2e3a06a9db4.png", kSliceElevatedFocused, kWidthElevatedFocused, kOutsetPressableFocused));
        add("pressable.elevated.destructive.pressed", tex("/hbui/assets/pressable_destructive_pressed-d5d4943d1b774ef42f85.png", kSlicePressed, kWidthPressed, kOutsetZero));
        add("pressable.elevated.destructive.disabled", tex("/hbui/assets/pressable_elevated_disabled-af3383540a78827c5a84.png", kSliceElevatedDisabled, kWidthElevatedDisabled, kOutsetZero));

        // ---- detailedCard ----
        add("detailedCard.base", tex("/hbui/assets/detailed-card-7c80abcb49930a828a00.png", "0 1 2 1 fill", "0 0.2rem 0.4rem 0.2rem", "0 0.2rem 0.2rem 0"));
        add("detailedCard.action.default", tex("/hbui/assets/detailed-card-action_default-1eacdbf7075ae21526da.png", "1 2 2 1 fill", "0.2rem 0.4rem 0.4rem 0.2rem", "0 0.2rem 0 0"));
        add("detailedCard.action.hovered", tex("/hbui/assets/detailed-card-action_hovered-987e725848caeabcf02e.png", "1 2 2 1 fill", "0.2rem 0.4rem 0.4rem 0.2rem", "0 0.2rem 0 0"));
        add("detailedCard.action.focused", tex("/hbui/assets/detailed-card-action_focused-6b947cba4cdee937c3d4.png", "2 2 4 2 fill", "0.4rem 0.4rem 0.8rem 0.4rem", "0.2rem 0.2rem 0.4rem 0.2rem"));
        add("detailedCard.action.pressed", tex("/hbui/assets/detailed-card-action_pressed-375eebb4a4eea54b415f.png", "3 2 0 1 fill", "0.6rem 0.4rem 0 0.2rem", "0 0.2rem 0 0"));
        add("detailedCard.action.pressedFocused", tex("/hbui/assets/detailed-card-action_pressed_focused-a905f0b3cc3538b55f2d.png", "2 2 2 2 fill", "0.4rem 0.4rem 0.4rem 0.4rem", "0 0.2rem 0.4rem 0.2rem"));
        add("detailedCard.action.disabled", tex("/hbui/assets/detailed-card-action_disabled-8dd883d650cd0ad144b2.png", "1 2 0 1 fill", "0.2rem 0.4rem 0 0.2rem", "0 0.2rem 0 0"));
        add("detailedCard.action.disabledFocused", tex("/hbui/assets/detailed-card-action_disabled_focused-b335a2c0b3fbda554a86.png", "2 2 2 2 fill", "0.4rem 0.4rem 0.4rem 0.4rem", "0.2rem 0.2rem 0.4rem 0.2rem"));
        add("detailedCard.additionalAction.default", tex("/hbui/assets/detailed-card-additional-action_default-bb25448c4bff7d9e0fd5.png", "1 2 2 1 fill", "0.2rem 0.4rem 0.4rem 0.2rem", "0 0.2rem 0 0"));
        add("detailedCard.additionalAction.hovered", tex("/hbui/assets/detailed-card-additional-action_hovered-c2eacb29e052db2f0cc0.png", "1 2 2 1 fill", "0.2rem 0.4rem 0.4rem 0.2rem", "0 0.2rem 0 0"));
        add("detailedCard.additionalAction.focused", tex("/hbui/assets/detailed-card-additional-action_focused-b079a1825eb256e61c44.png", "3 2 4 2 fill", "0.6rem 0.4rem 0.8rem 0.4rem", "0.4rem 0.2rem 0.4rem 0.2rem"));
        add("detailedCard.additionalAction.pressed", tex("/hbui/assets/detailed-card-additional-action_pressed-cc4a706dcab1406fe9dc.png", "3 2 0 1 fill", "0.6rem 0.4rem 0 0.2rem", "0 0.2rem 0 0"));
        add("detailedCard.additionalAction.pressedFocused", tex("/hbui/assets/detailed-card-additional-action_pressed_focused-df9f9b879e5ce495073d.png", "3 2 2 2 fill", "0.6rem 0.4rem 0.4rem 0.4rem", "0.2rem 0.2rem 0.4rem 0.2rem"));

        // ---- listItem ----
        add("listItem.base.default", tex("/hbui/assets/listItem_disabled_default-3c13faa164820f219a4a.png", "1 1 1 1 fill", "0.2rem 0.2rem 0.2rem 0.2rem", kOutsetZero));
        add("listItem.base.focused", tex("/hbui/assets/listItem_disabled_focused-58778a0e0bbcd1814386.png", "2 2 2 2 fill", "0.4rem 0.4rem 0.4rem 0.4rem", "0.2rem 0.2rem 0.2rem 0.2rem"));
        add("listItem.action.default", tex("/hbui/assets/listItem_action_default-98ec95bb538d5afbcf21.png", "1 1 1 1 fill", "0.2rem 0.2rem 0.2rem 0.2rem", kOutsetZero));
        add("listItem.action.hovered", tex("/hbui/assets/listItem_action_hovered-25f289553ec4efa5ec61.png", "1 1 1 1 fill", "0.2rem 0.2rem 0.2rem 0.2rem", kOutsetZero));
        add("listItem.action.pressed", tex("/hbui/assets/listItem_action_pressed-e222de76778d18c90218.png", "1 1 1 1 fill", "0.2rem 0.2rem 0.2rem 0.2rem", kOutsetZero));
        add("listItem.action.focused", tex("/hbui/assets/listItem_action_focused-cbd1f45efe632665d4f2.png", "3 3 3 3 fill", "0.6rem 0.6rem 0.6rem 0.6rem", "0.4rem 0.4rem 0.4rem 0.4rem"));
        add("listItem.action.pressedFocused", tex("/hbui/assets/listItem_action_pressed_focused-37eefa99d65e1495bef9.png", "3 3 3 3 fill", "0.6rem 0.6rem 0.6rem 0.6rem", "0.4rem 0.4rem 0.4rem 0.4rem"));
        add("listItem.action.disabled", tex("/hbui/assets/listItem_action_disabled-e34c7d807fefee0b1152.png", "0 0 0 0 fill", "0 0 0 0", kOutsetZero));
        add("listItem.action.disabledFocused", tex("/hbui/assets/listItem_action_disabled_focused-866c570916913c3f41de.png", "2 2 2 2 fill", "0.4rem 0.4rem 0.4rem 0.4rem", "0.4rem 0.4rem 0.4rem 0.4rem"));
        add("listItem.additionalAction.default", tex("/hbui/assets/listItem_action_default-98ec95bb538d5afbcf21.png", "1 1 1 1 fill", "0.2rem 0.2rem 0.2rem 0.2rem", kOutsetZero));
        add("listItem.additionalAction.hovered", tex("/hbui/assets/listItem_action_hovered-25f289553ec4efa5ec61.png", "1 1 1 1 fill", "0.2rem 0.2rem 0.2rem 0.2rem", kOutsetZero));
        add("listItem.additionalAction.pressed", tex("/hbui/assets/listItem_action_pressed-e222de76778d18c90218.png", "1 1 1 1 fill", "0.2rem 0.2rem 0.2rem 0.2rem", kOutsetZero));
        add("listItem.additionalAction.focused", tex("/hbui/assets/listItem_action_focused-cbd1f45efe632665d4f2.png", "3 3 3 3 fill", "0.6rem 0.6rem 0.6rem 0.6rem", "0.4rem 0.4rem 0.4rem 0.4rem"));
        add("listItem.additionalAction.pressedFocused", tex("/hbui/assets/listItem_action_pressed_focused-37eefa99d65e1495bef9.png", "3 3 3 3 fill", "0.6rem 0.6rem 0.6rem 0.6rem", "0.4rem 0.4rem 0.4rem 0.4rem"));

        // ---- tabBar ----
        add("tabBar.neutral.default", tex("/hbui/assets/tabBar_neutral_default-40e26ac9318c12909f42.png", "2 2 4 2 fill", "0.4rem 0.4rem 0.8rem 0.4rem", kOutsetZero));
        add("tabBar.neutral.hovered", tex("/hbui/assets/tabBar_neutral_hovered-b73d7029874500714b0f.png", "2 2 4 2 fill", "0.4rem 0.4rem 0.8rem 0.4rem", kOutsetZero));
        add("tabBar.neutral.focused", tex("/hbui/assets/tabBar_neutral_default_focused-812a9fcda5a4e49d93d5.png", "3 3 5 3 fill", "0.6rem 0.6rem 1rem 0.6rem", kOutsetPressableFocused));
        add("tabBar.neutral.pressed", tex("/hbui/assets/tabBar_neutral_pressed-9d14d8d1343a1fc3836d.png", "2 2 2 2 fill", "0.4rem 0.4rem 0.4rem 0.4rem", kOutsetZero));
        add("tabBar.neutral.pressedFocused", tex("/hbui/assets/tabBar_neutral_pressed_focused-c05012af3863c527b0a2.png", "3 3 3 3 fill", "0.6rem 0.6rem 0.6rem 0.6rem", kOutsetPressableFocused));

        // ---- bubble ----
        add("bubble.base.default", tex("/hbui/assets/bubble_action_default-bbef25eabe79ed131a72.png", "4 4 4 4 fill", "0.8rem 0.8rem 0.8rem 0.8rem", kOutsetZero));
        add("bubble.action.default", tex("/hbui/assets/bubble_action_default-bbef25eabe79ed131a72.png", "4 4 4 4 fill", "0.8rem 0.8rem 0.8rem 0.8rem", kOutsetZero));

        // ---- filterBar ----
        add("filterBar.base.default", tex("/hbui/assets/filterBar_action_default-d47c1536b27a56dc8182.png", "1 2 2 1 fill", "0.2rem 0.4rem 0.4rem 0.2rem", kOutsetZero));
        add("filterBar.action.default", tex("/hbui/assets/filterBar_action_default-d47c1536b27a56dc8182.png", "1 2 1 2 fill", "0.2rem 0.4rem 0.2rem 0.4rem", kOutsetZero));

        // ---- panel (gameplay-theme, container variants) ----
        add("panel.default", tex("/hbui/assets/container_default-855c528e8ab00e0d45a0.png", "4 4 5 4 fill", "0.8rem 0.8rem 1rem 0.8rem", kOutsetZero));
        add("panel.dark", tex("/hbui/assets/container_default_dark-1f864a7a2019892ece5f.png", "4 4 5 4 fill", "0.8rem 0.8rem 1rem 0.8rem", kOutsetZero));
        add("panel.furnace", tex("/hbui/assets/container_furnace_input-ddcdacda8e17517488f9.png", "5 5 8 6 fill", "1rem 1rem 1.6rem 1.2rem", kOutsetZero));
        add("panel.chest", tex("/hbui/assets/container_chest_default-540af0fa638ddf3bad5a.png", "5 5 8 6 fill", "1rem 1rem 1.6rem 1.2rem", kOutsetZero));
        add("panel.enderChest", tex("/hbui/assets/container_chest_ender-f717b696c57cef3ef3c5.png", "5 5 8 6 fill", "1rem 1rem 1.6rem 1.2rem", kOutsetZero));
        add("panel.shulkerBox", tex("/hbui/assets/container_shulker_box-59843294a40ef3035246.png", "5 5 8 6 fill", "1rem 1rem 1.6rem 1.2rem", kOutsetZero));
        add("panel.barrel", tex("/hbui/assets/container_barrel-4f0cb202d0974492f965.png", "8 7 8 7 fill", "1.6rem 1.4rem 1.6rem 1.4rem", kOutsetZero));
        add("panel.modalForm", tex("/hbui/assets/neutral-elevated-control-panel-959f59be1463847c54aa.png", "4 4 6 4 fill", "0.8rem 0.8rem 1.2rem 0.8rem", kOutsetZero));

        // ---- containerItem (gameplay-theme :root) ----
        add("containerItem.default", tex("/hbui/assets/item_slot_default-d77a7eb05889e5de6db8.png", "1 1 1 1 fill", "0.2rem 0.2rem 0.2rem 0.2rem", kOutsetZero));
        add("containerItem.highlight", tex("/hbui/assets/hover-91a80e612ae835842a9a.png", "1 1 1 1 fill", "0.2rem 0.2rem 0.2rem 0.2rem", kOutsetZero));
        add("containerItem.touchSelection", tex("/hbui/assets/touch_selection-dce3bdba6c6eed1ada2c.png", "6 6 6 6 fill", "1.2rem 1.2rem 1.2rem 1.2rem", kOutsetZero));

        // ---- containerItem container tones (gameplay-theme .dark/.furnace/... scopes) ----
        add("containerItem.dark.default", tex("/hbui/assets/item_slot_dark-53401ed574cc560b3b70.png", "1 1 1 1 fill", "0.2rem 0.2rem 0.2rem 0.2rem", kOutsetZero));
        add("containerItem.furnace.default", tex("/hbui/assets/item_slot_furnace-6ec951b202a4cac3ea75.png", "1 1 1 1 fill", "0.2rem 0.2rem 0.2rem 0.2rem", kOutsetZero));
        add("containerItem.chest.default", tex("/hbui/assets/item_slot_chest-68f36114aceb511609da.png", "1 1 1 1 fill", "0.2rem 0.2rem 0.2rem 0.2rem", kOutsetZero));
        add("containerItem.enderChest.default", tex("/hbui/assets/item_slot_ender_chest-192b82ee63ae3e54a94c.png", "1 1 1 1 fill", "0.2rem 0.2rem 0.2rem 0.2rem", kOutsetZero));
        add("containerItem.shulkerBox.default", tex("/hbui/assets/item_slot_shulker_box-5312333f4673585e0b54.png", "1 1 1 1 fill", "0.2rem 0.2rem 0.2rem 0.2rem", kOutsetZero));
        add("containerItem.barrel.default", tex("/hbui/assets/item_slot_barrels-f6698289d7519d32fa1e.png", "1 1 1 1 fill", "0.2rem 0.2rem 0.2rem 0.2rem", kOutsetZero));

        // ---- tooltip ----
        add("tooltip.default", tex("/hbui/assets/tooltip-2712aa22c3be49bc6874.png", "2 2 2 2 fill", "0.4rem 0.4rem 0.4rem 0.4rem", kOutsetZero));

        // ---- progress (gameplay-theme .furnace scope) ----
        add("progress.radial.bg", tex("/hbui/assets/radial_progress_bg_furnace-f9fe20e604d7c1215e77.png", "2 2 2 2 fill", "0.4rem 0.4rem 0.4rem 0.4rem", kOutsetZero));
        add("progress.radial.fill", tex("/hbui/assets/radial_progress_fill_furnace-6ccd48bb535318bf0db2.png", "2 2 2 2 fill", "0.4rem 0.4rem 0.4rem 0.4rem", kOutsetZero));
        add("progress.linear.bg", tex("/hbui/assets/fuel_slot_flame_bg_furnace-ec1792022067c1ab7e0c.png", "1 1 1 1 fill", "0.2rem 0.2rem 0.2rem 0.2rem", kOutsetZero));
        add("progress.linear.fill", tex("/hbui/assets/fuel_slot_flame_fill_furnace-74da4661ec5665539dbf.png", "1 1 1 1 fill", "0.2rem 0.2rem 0.2rem 0.2rem", kOutsetZero));

        // ---- keyIcon.keyboard (shared key cap, buttonIconKeyboardBorderImageSource) ----
        add("keyIcon.keyboard", tex("/hbui/assets/keyboard_button_fill_default-0ae720e12e917707309f.png", "3 4 5 4 fill", "0.6rem 0.8rem 1rem 0.8rem", kOutsetZero));

        return m;
    }();
    return table;
}

KeyIconTable const& keyIconTable() {
    static KeyIconTable const table = [] {
        KeyIconTable m;
        auto add = [&m](std::string name, KeyIconSpec spec) { m.emplace(std::move(name), std::move(spec)); };
        auto icon = [](std::string_view source, std::string_view width, std::string_view height) {
            return KeyIconSpec{std::string(source), std::string(width), std::string(height)};
        };
        // Standard key icon: 2.4rem x 2.4rem (--buttonIconKeyboard<Name>Width/Height).
        char const* const kSizeStd = "2.4rem";
        char const* const kSizeStdH = "2.4rem";

        add("A", icon("/hbui/assets/A-4cccefbe0659f849ce10.png", kSizeStd, kSizeStdH));
        add("B", icon("/hbui/assets/B-0ed99dcc7c88ed3000db.png", kSizeStd, kSizeStdH));
        add("C", icon("/hbui/assets/C-944036a9e8b90517c859.png", kSizeStd, kSizeStdH));
        add("D", icon("/hbui/assets/D-2f4f8e6472b0c5843702.png", kSizeStd, kSizeStdH));
        add("E", icon("/hbui/assets/E-8337e463cb01c98dedea.png", kSizeStd, kSizeStdH));
        add("F", icon("/hbui/assets/F-15a6b46bc59bc2aaf6c1.png", kSizeStd, kSizeStdH));
        add("G", icon("/hbui/assets/G-99a3b43d2b16626fc047.png", kSizeStd, kSizeStdH));
        add("H", icon("/hbui/assets/H-5cfca131693c11a8feb4.png", kSizeStd, kSizeStdH));
        add("I", icon("/hbui/assets/I-63b2cb793bbfc5431eae.png", kSizeStd, kSizeStdH));
        add("J", icon("/hbui/assets/J-36d1f2dcf1592738acb9.png", kSizeStd, kSizeStdH));
        add("K", icon("/hbui/assets/K-04ad0d3871a858e629a7.png", kSizeStd, kSizeStdH));
        add("L", icon("/hbui/assets/L-3802c4c50e5c3a86cd3f.png", kSizeStd, kSizeStdH));
        add("M", icon("/hbui/assets/M-2f2507a29bedabcce83e.png", kSizeStd, kSizeStdH));
        add("N", icon("/hbui/assets/N-b387201152d5dde815a1.png", kSizeStd, kSizeStdH));
        add("O", icon("/hbui/assets/O-74ce950b06a2c4b7504e.png", kSizeStd, kSizeStdH));
        add("P", icon("/hbui/assets/P-9a8caf96be3e25598bac.png", kSizeStd, kSizeStdH));
        add("Q", icon("/hbui/assets/Q-583b8e1d299ade2e0179.png", kSizeStd, kSizeStdH));
        add("R", icon("/hbui/assets/R-5a903c13003fdfc89f86.png", kSizeStd, kSizeStdH));
        add("S", icon("/hbui/assets/S-f267988ea9eba9534192.png", kSizeStd, kSizeStdH));
        add("T", icon("/hbui/assets/T-ae67c8e6cacc2bdb546d.png", kSizeStd, kSizeStdH));
        add("U", icon("/hbui/assets/U-5b8dfdce0ad2b7e3ca67.png", kSizeStd, kSizeStdH));
        add("V", icon("/hbui/assets/V-e2b398ae6837a788c35e.png", kSizeStd, kSizeStdH));
        add("W", icon("/hbui/assets/W-b16dd96ff94328e99499.png", kSizeStd, kSizeStdH));
        add("X", icon("/hbui/assets/X-9341cf863e1f6ab69e08.png", kSizeStd, kSizeStdH));
        add("Y", icon("/hbui/assets/Y-5be956ba4ecb539b31ce.png", kSizeStd, kSizeStdH));
        add("Z", icon("/hbui/assets/Z-73a8f3084fde7e4623ec.png", kSizeStd, kSizeStdH));

        add("F2", icon("/hbui/assets/F2-4374e2425a94c5f4cb6f.png", "2.4rem", "2.2rem"));
        add("Alt", icon("/hbui/assets/Alt-ec90bbeffda2576dab2e.png", kSizeStd, kSizeStdH));
        add("ArrowDown", icon("/hbui/assets/ArrowDown-5033b996f059017dd726.png", kSizeStd, kSizeStdH));
        add("ArrowLeft", icon("/hbui/assets/ArrowLeft-fab0e626f5386a3e1249.png", kSizeStd, kSizeStdH));
        add("ArrowRight", icon("/hbui/assets/ArrowRight-9c9785a63cfe3f0f2378.png", kSizeStd, kSizeStdH));
        add("ArrowUp", icon("/hbui/assets/ArrowUp-c0df10b5fe3308dcf4bf.png", kSizeStd, kSizeStdH));
        add("BracketClose", icon("/hbui/assets/BracketClose-87c37b74d76848b340d0.png", kSizeStd, kSizeStdH));
        add("BracketOpen", icon("/hbui/assets/BracketOpen-7c876ddebe566313a1df.png", kSizeStd, kSizeStdH));
        add("BracketCloseSmall", icon("/hbui/assets/BracketCloseSmall-bd95afef77ddb7b09e3c.png", kSizeStd, kSizeStdH));
        add("BracketOpenSmall", icon("/hbui/assets/BracketOpenSmall-7ca04e5e522aa59efdb4.png", kSizeStd, kSizeStdH));
        add("Control", icon("/hbui/assets/Control-698268b1ea8010ea6f14.png", kSizeStd, kSizeStdH));
        add("CtrlSmall", icon("/hbui/assets/CtrlSmall-8bfdaec233dfde58923d.png", kSizeStd, kSizeStdH));
        add("Enter", icon("/hbui/assets/Enter-11578057714f83f227ba.png", kSizeStd, kSizeStdH));
        add("Escape", icon("/hbui/assets/Escape-4e08ce0727505a6fe341.png", kSizeStd, kSizeStdH));
        add("Mouse", icon("/hbui/assets/Mouse-b6f3da7be9695f280589.png", kSizeStd, kSizeStdH));
        add("MouseLeftClick", icon("/hbui/assets/MouseLeftClick-4d521abc7a9bd481305d.png", kSizeStd, kSizeStdH));
        add("MouseRightClick", icon("/hbui/assets/MouseRightClick-fdaeb5fbd3535529840d.png", kSizeStd, kSizeStdH));
        add("MouseScrollClick", icon("/hbui/assets/MouseScrollClick-eaef7ca3e5f8e21215db.png", kSizeStd, kSizeStdH));
        add("MouseScrollWheel", icon("/hbui/assets/MouseScrollWheel-40f02a7efc79706e4ff7.png", kSizeStd, kSizeStdH));
        add("Shift", icon("/hbui/assets/Shift-cd31a7909780d17768e1.png", kSizeStd, kSizeStdH));
        add("Space", icon("/hbui/assets/Space-1fbfbac2e325e2255057.png", kSizeStd, kSizeStdH));
        add("SpecialCharacter", icon("/hbui/assets/SpecialCharacter-3945ec48393b41bd81fa.png", kSizeStd, kSizeStdH));
        add("Tab", icon("/hbui/assets/Tab-a810c6b2e930a6028ae0.png", kSizeStd, kSizeStdH));

        // Mouse movement / button glyphs (--buttonIconMouse*).
        add("MouseMovement", icon("/hbui/assets/mouse_directional-f2848c60d09257399182.png", "4.6rem", "5.2rem"));
        add("MouseButtonLeft", icon("/hbui/assets/mouse_left-c1a02cdde2eded95ef22.png", "2.2rem", "2.8rem"));
        add("MouseButtonRight", icon("/hbui/assets/mouse_right-bbee5b8889f82dcea7a3.png", "2.2rem", "2.8rem"));
        add("MouseButtonMiddle", icon("/hbui/assets/mouse_middle-d6f4b7223567766842f4.png", "2.2rem", "2.8rem"));
        add("MouseWheel", icon("/hbui/assets/mouse_scroll-bbcaa13f231433fd90cc.png", "3rem", "3.4rem"));

        return m;
    }();
    return table;
}

} // namespace

namespace VanillaAssets {

TextureSpec const* texture(std::string_view key) {
    auto const& table = textureTable();
    auto const it = table.find(key);
    return it == table.end() ? nullptr : &it->second;
}

KeyIconSpec const* keyIcon(std::string_view name) {
    auto const& table = keyIconTable();
    auto const it = table.find(name);
    return it == table.end() ? nullptr : &it->second;
}

std::size_t textureCount() {
    return textureTable().size();
}

std::size_t keyIconCount() {
    return keyIconTable().size();
}

std::vector<std::string> textureKeys() {
    std::vector<std::string> keys;
    keys.reserve(textureTable().size());
    for (auto const& [key, spec] : textureTable()) {
        (void)spec;
        keys.push_back(key);
    }
    return keys;
}

std::vector<std::string> keyIconNames() {
    std::vector<std::string> names;
    names.reserve(keyIconTable().size());
    for (auto const& [name, spec] : keyIconTable()) {
        (void)spec;
        names.push_back(name);
    }
    return names;
}

} // namespace VanillaAssets
} // namespace dearoreui::component
