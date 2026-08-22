#include "component/ComponentRenderer.h"
#include "component/VanillaAssets.h"

#include <sstream>

namespace dearoreui::component {

namespace {

[[nodiscard]] std::string px(int value) { return std::to_string(value) + "px"; }

// Emits the vanilla 9-slice border-image cssText for a semantic texture key:
//   border-image:url(<resolved>) <slice>; border-width:<width>; border-image-outset:<outset>;
// The theme override table wins; otherwise the vanilla table is used. Returns
// an empty string when the key is unknown (renderer falls back to plain CSS).
[[nodiscard]] std::string
borderImageStyle(std::string_view key, ThemeTokens const& theme, IAssetResolver const& resolver) {
    auto const* spec = theme.texture(key);
    if (spec == nullptr) {
        spec = VanillaAssets::texture(key);
    }
    if (spec == nullptr) {
        return {};
    }
    std::ostringstream style;
    style << "border-image:url(" << resolver.resolveTexture(*spec) << ") " << spec->slice << ";";
    style << "border-width:" << spec->width << ";";
    style << "border-image-outset:" << spec->outset << ";";
    return style.str();
}

// Full cssText for a texture-family component in a given state: the resolved
// border-image texture followed by the non-texture base style.
[[nodiscard]] std::string textureStyle(
    std::string const&    key,
    std::string const&    baseStyle,
    ThemeTokens const&    theme,
    IAssetResolver const& resolver
) {
    return borderImageStyle(key, theme, resolver) + baseStyle;
}

// Emits the per-state texture cssText map for an interactive component
// (M8.1.2). `states` is the ordered state list; `keyPrefix` + state forms the
// semantic texture key. States whose texture key is unknown (e.g.
// additionalAction has no disabled texture) are skipped. To keep the injected
// script small (cohtml ExecuteScript silently drops large scripts), only the
// state-specific texture cssText is stored; the shared base style goes into
// node.baseStyle. Returns the default state's full cssText (texture + base).
std::string emitStateStyles(
    render::DomNode&                node,
    std::vector<std::string> const& states,
    std::string const&              keyPrefix,
    std::string const&              baseStyle,
    ThemeTokens const&              theme,
    IAssetResolver const&           resolver
) {
    node.baseStyle = baseStyle;
    std::string defaultStyle;
    for (auto const& state : states) {
        auto texture = borderImageStyle(keyPrefix + state, theme, resolver);
        if (texture.empty()) {
            continue;
        }
        node.stateStyles.emplace_back(state, texture);
        if (state == "default") {
            defaultStyle = texture + baseStyle;
        }
    }
    return defaultStyle;
}

// Renders nested component children (excluding raw body) plus raw body nodes.
void renderChildren(
    ComponentSpec const&          spec,
    ThemeTokens const&            theme,
    IAssetResolver const&         resolver,
    std::vector<render::DomNode>& out
) {
    for (auto const& child : spec.children) {
        auto rendered = renderComponent(child, theme, resolver);
        for (auto& node : rendered) {
            out.push_back(std::move(node));
        }
    }
    for (auto const& node : spec.body) {
        out.push_back(node);
    }
}

// Effective interaction state: `disabled` wins over the declared state
// (stage 8.1 plan 2.2: "disabled 与 state=disabled 等价"). Returns a view into
// spec.state (a stable member) or a literal — never a temporary.
[[nodiscard]] std::string_view effectiveState(ComponentSpec const& spec) {
    if (spec.disabled) {
        return "disabled";
    }
    if (spec.state.empty()) {
        return "default";
    }
    return spec.state;
}

// Semantic texture key for a component, following the stage 8.1 plan 2.3
// naming (<family>.<variant>.<state>). Unknown keys fall back to the vanilla
// table lookup inside borderImageStyle.
[[nodiscard]] std::string textureKeyFor(ComponentSpec const& spec) {
    auto const state = effectiveState(spec);
    switch (spec.kind) {
    case ComponentKind::Button: {
        auto const variant = spec.variant.empty() ? "neutral" : spec.variant;
        if (spec.style == "elevated") {
            return "pressable.elevated." + variant + "." + std::string(state);
        }
        return "pressable." + variant + "." + std::string(state);
    }
    case ComponentKind::TabBar:
        return "tabBar.neutral." + std::string(state);
    case ComponentKind::Bubble:
        return spec.variant == "action" ? "bubble.action.default" : "bubble.base.default";
    case ComponentKind::FilterBar:
        return spec.variant == "action" ? "filterBar.action.default" : "filterBar.base.default";
    case ComponentKind::ListItem: {
        if (spec.variant == "action") {
            return "listItem.action." + std::string(state);
        }
        if (spec.variant == "additionalAction") {
            return "listItem.additionalAction." + std::string(state);
        }
        // listItem.base only has default/focused textures.
        auto const baseState = (state == "focused") ? "focused" : "default";
        return "listItem.base." + std::string(baseState);
    }
    case ComponentKind::Card: {
        if (spec.variant == "action") {
            return "detailedCard.action." + std::string(state);
        }
        if (spec.variant == "additionalAction") {
            return "detailedCard.additionalAction." + std::string(state);
        }
        return "detailedCard.base";
    }
    default:
        return {};
    }
}

} // namespace

VanillaAssetResolver const& defaultAssetResolver() {
    static VanillaAssetResolver const resolver;
    return resolver;
}

std::vector<render::DomNode>
renderComponent(ComponentSpec const& spec, ThemeTokens const& theme, IAssetResolver const& resolver) {
    std::vector<render::DomNode> nodes;

    switch (spec.kind) {
    case ComponentKind::Button: {
        render::DomNode button;
        button.tag = "div";
        // pressable texture family: variant x style(elevated) x state.
        auto const variant = spec.variant.empty() ? "neutral" : spec.variant;
        auto const keyPrefix =
            (spec.style == "elevated") ? "pressable.elevated." + variant + "." : "pressable." + variant + ".";
        std::string base  = "display:inline-block;";
        base             += "padding:0.8rem 1.6rem;"; // vanilla padding unknown; reasonable default
        base             += "font-family:" + theme.fontUi + ";";
        base             += "font-size:" + px(theme.fontSizes[FontSize::Medium]) + ";";
        base             += "line-height:1;color:" + theme.colorText + ";";
        base             += "cursor:pointer;";
        if (spec.disabled) {
            base += "cursor:default;";
        }
        // M8.1.2: node.style reflects the effective state; stateStyles carries
        // every state so the bootstrap can swap cssText on hover/pressed/focus.
        button.style = textureStyle(keyPrefix + std::string(effectiveState(spec)), base, theme, resolver);
        emitStateStyles(
            button,
            {"default", "hovered", "focused", "pressed", "disabled"},
            keyPrefix,
            base,
            theme,
            resolver
        );
        button.attrs.push_back(render::DomAttr{"data-component", "button"});
        button.attrs.push_back(render::DomAttr{"tabindex", "0"});
        if (spec.disabled) {
            button.attrs.push_back(render::DomAttr{"aria-disabled", "true"});
        }
        if (spec.label.empty()) {
            renderChildren(spec, theme, resolver, button.children);
        } else {
            button.text = spec.label;
        }
        nodes.push_back(std::move(button));
        break;
    }
    case ComponentKind::Panel: {
        render::DomNode panel;
        panel.tag = "div";
        // panel texture family: 8 container variants (default/dark/furnace/...).
        // The "transparent" variant is a review container: no texture, fully
        // transparent background, scrollable so the underlying UI stays visible.
        auto const variant = (spec.style.empty() || spec.style == "normal") ? "default" : spec.style;
        if (variant == "transparent") {
            panel.style  = "background:transparent;";
            panel.style += "height:100%;overflow-y:auto;";
        } else if (variant == "translucent") {
            // Review container with a semi-transparent black backdrop
            // (--colorsPanel rgba(0,0,0,0.72)) so the underlying UI stays
            // dimly visible while the component library is inspected.
            panel.style  = "background:rgba(0,0,0,0.72);";
            panel.style += "height:100%;overflow-y:auto;";
        } else {
            panel.style = borderImageStyle("panel." + variant, theme, resolver);
        }
        panel.style += "display:flex;flex-direction:column;";
        panel.style += "padding:1.6rem 1.6rem 2rem 1.6rem;"; // --panelPadding*
        panel.attrs.push_back(render::DomAttr{"data-component", "panel"});
        if (!spec.label.empty()) {
            render::DomNode title;
            title.tag = "div";
            title.style =
                "font-family:" + theme.fontHeading + ";font-size:" + px(theme.fontSizes[FontSize::Large]) + ";";
            title.style += "color:" + theme.colorText + ";margin-bottom:1.2rem;";
            title.text   = spec.label;
            panel.children.push_back(std::move(title));
        }
        renderChildren(spec, theme, resolver, panel.children);
        nodes.push_back(std::move(panel));
        break;
    }
    case ComponentKind::Text: {
        render::DomNode text;
        text.tag  = "div";
        text.text = spec.label;
        if (spec.variant == "heading") {
            text.style =
                "font-family:" + theme.fontHeading + ";font-size:" + px(theme.fontSizes[FontSize::Heading]) + ";";
            text.style += "color:" + theme.colorText + ";";
        } else if (spec.variant == "subheading") {
            text.style =
                "font-family:" + theme.fontSubheading + ";font-size:" + px(theme.fontSizes[FontSize::Large]) + ";";
            text.style += "color:" + theme.colorText + ";";
        } else if (spec.variant == "muted") {
            text.style  = "font-family:" + theme.fontBody + ";font-size:" + px(theme.fontSizes[FontSize::Small]) + ";";
            text.style += "color:" + theme.colorMuted + ";";
        } else if (spec.variant == "tiny") {
            // Four-step type scale (--fontSizes0..3): tiny/small/medium/large.
            text.style  = "font-family:" + theme.fontUi + ";font-size:" + px(theme.fontSizes[FontSize::Tiny]) + ";";
            text.style += "color:" + theme.colorText + ";";
        } else { // ui/body
            text.style  = "font-family:" + theme.fontUi + ";font-size:" + px(theme.fontSizes[FontSize::Medium]) + ";";
            text.style += "color:" + theme.colorText + ";";
        }
        nodes.push_back(std::move(text));
        break;
    }
    case ComponentKind::Card: {
        render::DomNode card;
        card.tag = "div";
        // detailedCard texture family: base / action / additionalAction x state.
        std::string base = "padding:1.2rem 1.6rem;";
        if (spec.variant == "action" || spec.variant == "additionalAction") {
            auto const variant   = spec.variant.empty() ? "action" : spec.variant;
            auto const keyPrefix = "detailedCard." + variant + ".";
            card.style           = textureStyle(keyPrefix + std::string(effectiveState(spec)), base, theme, resolver);
            emitStateStyles(
                card,
                {"default", "hovered", "pressed", "focused", "pressedFocused", "disabled", "disabledFocused"},
                keyPrefix,
                base,
                theme,
                resolver
            );
            card.attrs.push_back(render::DomAttr{"tabindex", "0"});
        } else {
            card.style = textureStyle("detailedCard.base", base, theme, resolver);
        }
        card.attrs.push_back(render::DomAttr{"data-component", "card"});
        renderChildren(spec, theme, resolver, card.children);
        nodes.push_back(std::move(card));
        break;
    }
    case ComponentKind::ListItem: {
        render::DomNode item;
        item.tag = "div";
        // listItem texture family: base / action / additionalAction x state.
        std::string base = "display:flex;align-items:center;padding:0.8rem 1.4rem;";
        if (spec.variant == "action" || spec.variant == "additionalAction") {
            auto const variant   = spec.variant.empty() ? "action" : spec.variant;
            auto const keyPrefix = "listItem." + variant + ".";
            item.style           = textureStyle(keyPrefix + std::string(effectiveState(spec)), base, theme, resolver);
            emitStateStyles(
                item,
                {"default", "hovered", "pressed", "focused", "pressedFocused", "disabled", "disabledFocused"},
                keyPrefix,
                base,
                theme,
                resolver
            );
            item.attrs.push_back(render::DomAttr{"tabindex", "0"});
        } else {
            // listItem.base only has default/focused textures.
            auto const baseState = (effectiveState(spec) == "focused") ? "focused" : "default";
            item.style           = textureStyle("listItem.base." + std::string(baseState), base, theme, resolver);
            emitStateStyles(item, {"default", "focused"}, "listItem.base.", base, theme, resolver);
            item.attrs.push_back(render::DomAttr{"tabindex", "0"});
        }
        item.attrs.push_back(render::DomAttr{"data-component", "listItem"});
        if (!spec.label.empty()) {
            render::DomNode label;
            label.tag    = "div";
            label.text   = spec.label;
            label.style  = "font-family:" + theme.fontUi + ";font-size:" + px(theme.fontSizes[FontSize::Medium]) + ";";
            label.style += "color:" + theme.colorText + ";";
            item.children.push_back(std::move(label));
        }
        renderChildren(spec, theme, resolver, item.children);
        nodes.push_back(std::move(item));
        break;
    }
    case ComponentKind::Bubble: {
        render::DomNode bubble;
        bubble.tag    = "div";
        bubble.style  = borderImageStyle(textureKeyFor(spec), theme, resolver);
        bubble.style += "display:inline-block;padding:0.8rem 1.2rem;";
        bubble.style += "font-family:" + theme.fontUi + ";font-size:" + px(theme.fontSizes[FontSize::Small]) + ";";
        bubble.style += "color:" + theme.colorText + ";";
        bubble.attrs.push_back(render::DomAttr{"data-component", "bubble"});
        if (spec.label.empty()) {
            renderChildren(spec, theme, resolver, bubble.children);
        } else {
            bubble.text = spec.label;
        }
        nodes.push_back(std::move(bubble));
        break;
    }
    case ComponentKind::FilterBar: {
        render::DomNode bar;
        bar.tag    = "div";
        bar.style  = borderImageStyle(textureKeyFor(spec), theme, resolver);
        bar.style += "display:inline-block;padding:0.4rem 0.8rem;";
        bar.style += "font-family:" + theme.fontUi + ";font-size:" + px(theme.fontSizes[FontSize::Small]) + ";";
        bar.style += "color:" + theme.colorText + ";";
        bar.attrs.push_back(render::DomAttr{"data-component", "filterBar"});
        if (spec.label.empty()) {
            renderChildren(spec, theme, resolver, bar.children);
        } else {
            bar.text = spec.label;
        }
        nodes.push_back(std::move(bar));
        break;
    }
    case ComponentKind::Input: {
        render::DomNode wrapper;
        wrapper.tag = "div";
        // inputLegend (menus-theme): wrapper background + hint typography.
        wrapper.style  = "display:flex;flex-direction:column;";
        wrapper.style += "background:#1e1e1f;";                     // --inputLegendWrapperBackgroundColor
        wrapper.style += "padding:0 2rem;";                         // --inputLegendWrapperPaddingLeft/Right
        wrapper.style += "text-shadow:0.2rem 0.2rem 0rem #303438;"; // --inputLegendWrapperTextShadow
        wrapper.attrs.push_back(render::DomAttr{"data-component", "input"});
        if (!spec.label.empty()) {
            render::DomNode hint;
            hint.tag  = "div";
            hint.text = spec.label;
            // --inputLegendInputHint*: Minecraft Seven v2, 1.6rem.
            hint.style  = "font-family:" + theme.fontUi + ";";
            hint.style += "font-size:1.6rem;line-height:1.6rem;letter-spacing:0.04rem;";
            hint.style += "color:#fff;margin-bottom:0.8rem;"; // --inputLegendInputHintSpaceToLabel
            wrapper.children.push_back(std::move(hint));
        }
        render::DomNode field;
        field.tag              = "input";
        std::string fieldBase  = "width:100%;box-sizing:border-box;padding:10px 12px;";
        fieldBase             += "background:#1e1e1f;color:" + theme.colorText + ";";
        fieldBase             += "border:2px solid " + theme.colorSecondary + ";border-radius:4px;";
        fieldBase   += "font-family:" + theme.fontUi + ";font-size:" + px(theme.fontSizes[FontSize::Medium]) + ";";
        field.style  = fieldBase;
        // M8.1.2: focus highlight for the input field (no vanilla texture).
        // baseStyle + texture-only stateStyles keeps the injected script small.
        field.baseStyle = fieldBase;
        field.stateStyles.emplace_back("default", "");
        field.stateStyles.emplace_back("focused", "border-color:#fff;outline:none;");
        field.attrs.push_back(render::DomAttr{"type", "text"});
        if (spec.disabled) {
            field.attrs.push_back(render::DomAttr{"disabled", "true"});
        }
        wrapper.children.push_back(std::move(field));
        nodes.push_back(std::move(wrapper));
        break;
    }
    case ComponentKind::Divider: {
        render::DomNode divider;
        divider.tag = "div";
        // formDivider (gameplay-theme .modalForm): 0.4rem hairline with a dark
        // top / light bottom border.
        divider.style  = "height:0.4rem;";                                    // --formDividerHeight
        divider.style += "border-top:0.2rem solid rgba(0,0,0,0.3);";          // --formDividerBorderTopColor
        divider.style += "border-bottom:0.2rem solid rgba(255,255,255,0.1);"; // --formDividerBorderBottomColor
        divider.attrs.push_back(render::DomAttr{"data-component", "divider"});
        nodes.push_back(std::move(divider));
        break;
    }
    case ComponentKind::Tooltip: {
        render::DomNode tip;
        tip.tag = "div";
        // Single-texture tooltip (gameplay-theme --tooltip*).
        tip.style  = borderImageStyle("tooltip.default", theme, resolver);
        tip.style += "display:inline-block;padding:0.8rem 1.2rem;";
        tip.style += "color:#fff;"; // --tooltipTextColor
        tip.style += "font-family:" + theme.fontUi + ";font-size:" + px(theme.fontSizes[FontSize::Small]) + ";";
        tip.attrs.push_back(render::DomAttr{"data-component", "tooltip"});
        if (spec.label.empty()) {
            renderChildren(spec, theme, resolver, tip.children);
        } else {
            tip.text = spec.label;
        }
        nodes.push_back(std::move(tip));
        break;
    }
    case ComponentKind::ContainerSlot: {
        render::DomNode slot;
        slot.tag = "div";
        // Texture key: state picks default/highlight/touchSelection; style
        // picks the container tone (chest/barrel/shulker/furnace/...).
        std::string key;
        if (spec.state == "highlight") {
            key = "containerItem.highlight";
        } else if (spec.state == "touchSelection") {
            key = "containerItem.touchSelection";
        } else {
            auto const tone = (spec.style.empty() || spec.style == "normal") ? "default" : spec.style;
            key             = (tone == "default") ? "containerItem.default" : "containerItem." + tone + ".default";
        }
        slot.style  = borderImageStyle(key, theme, resolver);
        slot.style += "position:relative;";
        slot.style += "width:3.8rem;height:3.8rem;"; // --containerItemWidth/Height
        slot.attrs.push_back(render::DomAttr{"data-component", "containerSlot"});
        if (!spec.label.empty()) {
            // Stack amount badge (--containerItemAmountDefaultColor/TextShadow).
            render::DomNode amount;
            amount.tag    = "div";
            amount.text   = spec.label;
            amount.style  = "position:absolute;right:0.2rem;bottom:0.2rem;";
            amount.style += "color:#fff;text-shadow:0.2rem 0.2rem 0rem rgba(0,0,0,0.4);";
            amount.style += "font-family:" + theme.fontUi + ";font-size:" + px(theme.fontSizes[FontSize::Small]) + ";";
            slot.children.push_back(std::move(amount));
        }
        nodes.push_back(std::move(slot));
        break;
    }
    case ComponentKind::KeyIcon: {
        // Key cap (border-image) + per-key glyph (background-image + size).
        auto const* icon = VanillaAssets::keyIcon(spec.label);
        if (icon == nullptr) {
            break; // unknown key name -> render nothing
        }
        render::DomNode key;
        key.tag    = "div";
        key.style  = borderImageStyle("keyIcon.keyboard", theme, resolver);
        key.style += "display:inline-flex;align-items:center;justify-content:center;";
        key.style += "padding:1rem 0.8rem 1.2rem 0.8rem;"; // --buttonIconKeyboardPadding*
        key.attrs.push_back(render::DomAttr{"data-component", "keyIcon"});
        render::DomNode glyph;
        glyph.tag    = "div";
        glyph.style  = "width:" + icon->width + ";height:" + icon->height + ";";
        glyph.style += "background-image:url(" + resolver.resolveTexture(TextureSpec{icon->source, {}, {}, {}}) + ");";
        glyph.style += "background-size:contain;background-repeat:no-repeat;background-position:center;";
        key.children.push_back(std::move(glyph));
        nodes.push_back(std::move(key));
        break;
    }
    case ComponentKind::TabBar: {
        render::DomNode bar;
        bar.tag   = "div";
        bar.style = "display:flex;gap:0.4rem;";
        bar.attrs.push_back(render::DomAttr{"data-component", "tabBar"});
        for (auto const& child : spec.children) {
            render::DomNode tab;
            tab.tag  = "div";
            tab.text = child.label;
            // Each tab uses the tabBar texture family with its own state.
            auto const  state = child.state.empty() ? "default" : child.state;
            std::string base  = "padding:0.8rem 1.8rem;font-family:" + theme.fontUi + ";";
            base      += "font-size:" + px(theme.fontSizes[FontSize::Small]) + ";color:" + theme.colorText + ";";
            tab.style  = textureStyle("tabBar.neutral." + std::string(state), base, theme, resolver);
            emitStateStyles(
                tab,
                {"default", "hovered", "focused", "pressed", "pressedFocused"},
                "tabBar.neutral.",
                base,
                theme,
                resolver
            );
            tab.attrs.push_back(render::DomAttr{"data-component", "tab"});
            tab.attrs.push_back(render::DomAttr{"tabindex", "0"});
            bar.children.push_back(std::move(tab));
        }
        nodes.push_back(std::move(bar));
        break;
    }
    case ComponentKind::Progress: {
        // Furnace progress: radial (smelting ring) or linear (fuel flame).
        // The keyframes (clip-path polygons) are already loaded by the page
        // (gameplay-theme); the renderer references them by name.
        auto const      radial = (spec.variant == "linear") ? false : true;
        render::DomNode progress;
        progress.tag   = "div";
        progress.style = "position:relative;";
        progress.attrs.push_back(render::DomAttr{"data-component", "progress"});
        if (radial) {
            progress.style += "width:2.6rem;height:2.6rem;"; // radial ring footprint
            render::DomNode bg;
            bg.tag    = "div";
            bg.style  = borderImageStyle("progress.radial.bg", theme, resolver);
            bg.style += "position:absolute;inset:0;";
            progress.children.push_back(std::move(bg));
            render::DomNode fill;
            fill.tag    = "div";
            fill.style  = borderImageStyle("progress.radial.fill", theme, resolver);
            fill.style += "position:absolute;inset:0;";
            fill.style += "animation:animation-249c45b 1s steps(9) infinite;"; // --progressRadialAnimation
            progress.children.push_back(std::move(fill));
        } else {
            progress.style += "width:3.8rem;height:3.8rem;"; // fuel slot footprint
            render::DomNode bg;
            bg.tag    = "div";
            bg.style  = borderImageStyle("progress.linear.bg", theme, resolver);
            bg.style += "position:absolute;inset:0;";
            progress.children.push_back(std::move(bg));
            render::DomNode fill;
            fill.tag    = "div";
            fill.style  = borderImageStyle("progress.linear.fill", theme, resolver);
            fill.style += "position:absolute;inset:0;";
            fill.style += "animation:animation--2c0f9be3 1s steps(1) infinite;"; // --progressLinearAnimation
            progress.children.push_back(std::move(fill));
        }
        nodes.push_back(std::move(progress));
        break;
    }
    // ---- Stage 8.1.4: layout (L) components (pure CSS, no texture) ----
    case ComponentKind::Stack: {
        render::DomNode stack;
        stack.tag       = "div";
        auto const dir  = (spec.orientation == "row") ? "row" : "column";
        stack.style     = "display:flex;flex-direction:" + std::string(dir) + ";";
        stack.style    += "gap:" + px(theme.spaces[2]) + ";"; // --space2
        stack.attrs.push_back(render::DomAttr{"data-component", "stack"});
        renderChildren(spec, theme, resolver, stack.children);
        nodes.push_back(std::move(stack));
        break;
    }
    case ComponentKind::Grid: {
        render::DomNode grid;
        grid.tag         = "div";
        auto const cols  = spec.columns > 0 ? spec.columns : 1;
        grid.style       = "display:grid;grid-template-columns:repeat(" + std::to_string(cols) + ",1fr);";
        grid.style      += "gap:" + px(theme.spaces[2]) + ";"; // --space2
        grid.attrs.push_back(render::DomAttr{"data-component", "grid"});
        renderChildren(spec, theme, resolver, grid.children);
        nodes.push_back(std::move(grid));
        break;
    }
    case ComponentKind::ScrollView: {
        render::DomNode scroll;
        scroll.tag   = "div";
        scroll.style = "overflow-y:auto;height:100%;";
        scroll.attrs.push_back(render::DomAttr{"data-component", "scrollView"});
        renderChildren(spec, theme, resolver, scroll.children);
        nodes.push_back(std::move(scroll));
        break;
    }
    case ComponentKind::Section: {
        render::DomNode section;
        section.tag   = "div";
        section.style = "display:flex;flex-direction:column;gap:" + px(theme.spaces[2]) + ";";
        section.attrs.push_back(render::DomAttr{"data-component", "section"});
        if (!spec.label.empty()) {
            render::DomNode title;
            title.tag  = "div";
            title.text = spec.label;
            title.style =
                "font-family:" + theme.fontSubheading + ";font-size:" + px(theme.fontSizes[FontSize::Large]) + ";";
            title.style += "color:" + theme.colorText + ";";
            section.children.push_back(std::move(title));
        }
        renderChildren(spec, theme, resolver, section.children);
        nodes.push_back(std::move(section));
        break;
    }
    case ComponentKind::Spacer: {
        render::DomNode spacer;
        spacer.tag   = "div";
        spacer.style = "flex:1;";
        spacer.attrs.push_back(render::DomAttr{"data-component", "spacer"});
        nodes.push_back(std::move(spacer));
        break;
    }
    // ---- Stage 8.1.4: composite (B) components ----
    case ComponentKind::Modal: {
        // backdrop (semi-transparent overlay) -> panel(modalForm) -> content.
        render::DomNode backdrop;
        backdrop.tag    = "div";
        backdrop.style  = "position:fixed;top:0;left:0;right:0;bottom:0;";
        backdrop.style += "background:rgba(0,0,0,0.5);display:flex;align-items:center;justify-content:center;";
        backdrop.attrs.push_back(render::DomAttr{"data-component", "modal"});
        render::DomNode panel;
        panel.tag    = "div";
        panel.style  = borderImageStyle("panel.modalForm", theme, resolver);
        panel.style += "display:flex;flex-direction:column;gap:" + px(theme.spaces[2]) + ";";
        panel.style += "padding:1.6rem;min-width:24rem;max-width:80%;";
        if (!spec.label.empty()) {
            render::DomNode title;
            title.tag  = "div";
            title.text = spec.label;
            title.style =
                "font-family:" + theme.fontHeading + ";font-size:" + px(theme.fontSizes[FontSize::Large]) + ";";
            title.style += "color:" + theme.colorText + ";";
            panel.children.push_back(std::move(title));
        }
        renderChildren(spec, theme, resolver, panel.children);
        backdrop.children.push_back(std::move(panel));
        nodes.push_back(std::move(backdrop));
        break;
    }
    case ComponentKind::Menu: {
        // panel -> listItem group.
        render::DomNode menu;
        menu.tag    = "div";
        menu.style  = borderImageStyle("panel.default", theme, resolver);
        menu.style += "display:flex;flex-direction:column;padding:0.8rem;min-width:16rem;";
        menu.attrs.push_back(render::DomAttr{"data-component", "menu"});
        renderChildren(spec, theme, resolver, menu.children);
        nodes.push_back(std::move(menu));
        break;
    }
    case ComponentKind::ScrollingList: {
        // scrollView -> listItem group.
        render::DomNode scroll;
        scroll.tag   = "div";
        scroll.style = "overflow-y:auto;height:100%;display:flex;flex-direction:column;";
        scroll.attrs.push_back(render::DomAttr{"data-component", "scrollingList"});
        renderChildren(spec, theme, resolver, scroll.children);
        nodes.push_back(std::move(scroll));
        break;
    }
    case ComponentKind::Dropdown: {
        // trigger button + options panel (children as listItems).
        render::DomNode dropdown;
        dropdown.tag   = "div";
        dropdown.style = "display:inline-block;position:relative;";
        dropdown.attrs.push_back(render::DomAttr{"data-component", "dropdown"});
        render::DomNode trigger;
        trigger.tag    = "div";
        trigger.style  = "display:inline-flex;align-items:center;gap:0.6rem;";
        trigger.style += "padding:0.8rem 1.6rem;font-family:" + theme.fontUi + ";";
        trigger.style += "font-size:" + px(theme.fontSizes[FontSize::Medium]) + ";color:" + theme.colorText + ";";
        trigger.style += "cursor:pointer;";
        trigger.style += borderImageStyle("pressable.neutral.default", theme, resolver);
        trigger.attrs.push_back(render::DomAttr{"data-component", "dropdownTrigger"});
        trigger.attrs.push_back(render::DomAttr{"tabindex", "0"});
        if (!spec.label.empty()) {
            trigger.text = spec.label;
        }
        dropdown.children.push_back(std::move(trigger));
        if (!spec.children.empty()) {
            render::DomNode options;
            options.tag    = "div";
            options.style  = "position:absolute;top:100%;left:0;margin-top:0.4rem;";
            options.style += "display:flex;flex-direction:column;min-width:100%;";
            options.style += borderImageStyle("panel.default", theme, resolver);
            options.style += "padding:0.4rem;z-index:10;";
            options.attrs.push_back(render::DomAttr{"data-component", "dropdownOptions"});
            for (auto const& child : spec.children) {
                auto rendered = renderComponent(child, theme, resolver);
                for (auto& node : rendered) {
                    options.children.push_back(std::move(node));
                }
            }
            dropdown.children.push_back(std::move(options));
        }
        nodes.push_back(std::move(dropdown));
        break;
    }
    case ComponentKind::Form: {
        // panel -> input group + formButton + formDivider.
        render::DomNode form;
        form.tag    = "div";
        form.style  = borderImageStyle("panel.modalForm", theme, resolver);
        form.style += "display:flex;flex-direction:column;gap:" + px(theme.spaces[2]) + ";";
        form.style += "padding:1.6rem;";
        form.attrs.push_back(render::DomAttr{"data-component", "form"});
        if (!spec.label.empty()) {
            render::DomNode title;
            title.tag  = "div";
            title.text = spec.label;
            title.style =
                "font-family:" + theme.fontHeading + ";font-size:" + px(theme.fontSizes[FontSize::Large]) + ";";
            title.style += "color:" + theme.colorText + ";";
            form.children.push_back(std::move(title));
        }
        renderChildren(spec, theme, resolver, form.children);
        nodes.push_back(std::move(form));
        break;
    }
    case ComponentKind::NavigationBar: {
        // brand + tabBar + keyIcon hint.
        render::DomNode bar;
        bar.tag    = "div";
        bar.style  = "display:flex;align-items:center;gap:" + px(theme.spaces[3]) + ";";
        bar.style += "padding:0.8rem 1.6rem;";
        bar.attrs.push_back(render::DomAttr{"data-component", "navigationBar"});
        if (!spec.label.empty()) {
            render::DomNode brand;
            brand.tag  = "div";
            brand.text = spec.label;
            brand.style =
                "font-family:" + theme.fontHeading + ";font-size:" + px(theme.fontSizes[FontSize::Large]) + ";";
            brand.style += "color:" + theme.colorText + ";";
            bar.children.push_back(std::move(brand));
        }
        renderChildren(spec, theme, resolver, bar.children);
        nodes.push_back(std::move(bar));
        break;
    }
    case ComponentKind::Toast: {
        // bubble + text.
        render::DomNode toast;
        toast.tag    = "div";
        toast.style  = borderImageStyle("bubble.base.default", theme, resolver);
        toast.style += "display:inline-block;padding:0.8rem 1.2rem;";
        toast.style += "font-family:" + theme.fontUi + ";font-size:" + px(theme.fontSizes[FontSize::Small]) + ";";
        toast.style += "color:" + theme.colorText + ";";
        toast.attrs.push_back(render::DomAttr{"data-component", "toast"});
        if (spec.label.empty()) {
            renderChildren(spec, theme, resolver, toast.children);
        } else {
            toast.text = spec.label;
        }
        nodes.push_back(std::move(toast));
        break;
    }
    case ComponentKind::SearchField: {
        // input + search icon.
        render::DomNode wrapper;
        wrapper.tag    = "div";
        wrapper.style  = "display:flex;align-items:center;gap:0.6rem;";
        wrapper.style += "background:#1e1e1f;padding:0 1rem;";
        wrapper.attrs.push_back(render::DomAttr{"data-component", "searchField"});
        render::DomNode field;
        field.tag    = "input";
        field.style  = "flex:1;background:transparent;border:none;outline:none;";
        field.style += "color:" + theme.colorText + ";font-family:" + theme.fontUi + ";";
        field.style += "font-size:" + px(theme.fontSizes[FontSize::Medium]) + ";padding:0.8rem 0;";
        field.attrs.push_back(render::DomAttr{"type", "text"});
        if (!spec.label.empty()) {
            field.attrs.push_back(render::DomAttr{"placeholder", spec.label});
        }
        wrapper.children.push_back(std::move(field));
        if (auto const* icon = VanillaAssets::icon("search"); icon != nullptr) {
            render::DomNode glyph;
            glyph.tag   = "div";
            glyph.style = "width:" + icon->width + ";height:" + icon->height + ";";
            glyph.style +=
                "background-image:url(" + resolver.resolveTexture(TextureSpec{icon->source, {}, {}, {}}) + ");";
            glyph.style += "background-size:contain;background-repeat:no-repeat;background-position:center;";
            wrapper.children.push_back(std::move(glyph));
        }
        nodes.push_back(std::move(wrapper));
        break;
    }
    case ComponentKind::Toggle: {
        // button state + checkmark icon (approximation of vanilla facet).
        render::DomNode toggle;
        toggle.tag    = "div";
        toggle.style  = "display:inline-flex;align-items:center;gap:0.6rem;cursor:pointer;";
        toggle.style += "padding:0.6rem 1.2rem;font-family:" + theme.fontUi + ";";
        toggle.style += "font-size:" + px(theme.fontSizes[FontSize::Medium]) + ";color:" + theme.colorText + ";";
        toggle.style += borderImageStyle("pressable.neutral.default", theme, resolver);
        toggle.attrs.push_back(render::DomAttr{"data-component", "toggle"});
        toggle.attrs.push_back(render::DomAttr{"tabindex", "0"});
        if (spec.state == "on" || spec.state == "checked") {
            if (auto const* icon = VanillaAssets::icon("checkmark"); icon != nullptr) {
                render::DomNode glyph;
                glyph.tag   = "div";
                glyph.style = "width:" + icon->width + ";height:" + icon->height + ";";
                glyph.style +=
                    "background-image:url(" + resolver.resolveTexture(TextureSpec{icon->source, {}, {}, {}}) + ");";
                glyph.style += "background-size:contain;background-repeat:no-repeat;background-position:center;";
                toggle.children.push_back(std::move(glyph));
            }
        }
        if (!spec.label.empty()) {
            render::DomNode label;
            label.tag    = "div";
            label.text   = spec.label;
            label.style  = "font-family:" + theme.fontUi + ";font-size:" + px(theme.fontSizes[FontSize::Medium]) + ";";
            label.style += "color:" + theme.colorText + ";";
            toggle.children.push_back(std::move(label));
        }
        nodes.push_back(std::move(toggle));
        break;
    }
    // ---- Stage 8.1.4: navigation (N) / interaction (I) / data (D) ----
    case ComponentKind::Breadcrumb: {
        render::DomNode crumb;
        crumb.tag   = "div";
        crumb.style = "display:flex;align-items:center;gap:0.6rem;";
        crumb.attrs.push_back(render::DomAttr{"data-component", "breadcrumb"});
        bool first = true;
        for (auto const& child : spec.children) {
            if (!first) {
                render::DomNode sep;
                sep.tag   = "div";
                sep.text  = "/";
                sep.style = "color:" + theme.colorMuted + ";font-family:" + theme.fontUi + ";";
                crumb.children.push_back(std::move(sep));
            }
            first         = false;
            auto rendered = renderComponent(child, theme, resolver);
            for (auto& node : rendered) {
                crumb.children.push_back(std::move(node));
            }
        }
        nodes.push_back(std::move(crumb));
        break;
    }
    case ComponentKind::Pager: {
        // button group (dots/pages).
        render::DomNode pager;
        pager.tag   = "div";
        pager.style = "display:flex;align-items:center;gap:0.4rem;";
        pager.attrs.push_back(render::DomAttr{"data-component", "pager"});
        auto const count = spec.columns > 0 ? spec.columns : 1;
        for (int i = 0; i < count; ++i) {
            render::DomNode dot;
            dot.tag           = "div";
            auto const active = (spec.value == std::to_string(i));
            dot.style         = "width:1rem;height:1rem;border-radius:50%;";
            dot.style += active ? "background:" + theme.colorPrimary + ";" : "background:" + theme.colorMuted + ";";
            dot.attrs.push_back(render::DomAttr{"data-component", "pagerDot"});
            pager.children.push_back(std::move(dot));
        }
        nodes.push_back(std::move(pager));
        break;
    }
    case ComponentKind::TextArea: {
        render::DomNode wrapper;
        wrapper.tag    = "div";
        wrapper.style  = "display:flex;flex-direction:column;";
        wrapper.style += "background:#1e1e1f;padding:0 2rem;";
        wrapper.attrs.push_back(render::DomAttr{"data-component", "textArea"});
        if (!spec.label.empty()) {
            render::DomNode hint;
            hint.tag    = "div";
            hint.text   = spec.label;
            hint.style  = "font-family:" + theme.fontUi + ";font-size:1.6rem;line-height:1.6rem;";
            hint.style += "color:#fff;margin-bottom:0.8rem;";
            wrapper.children.push_back(std::move(hint));
        }
        render::DomNode area;
        area.tag    = "textarea";
        area.style  = "width:100%;box-sizing:border-box;padding:10px 12px;";
        area.style += "background:#1e1e1f;color:" + theme.colorText + ";";
        area.style += "border:2px solid " + theme.colorSecondary + ";border-radius:4px;";
        area.style += "font-family:" + theme.fontUi + ";font-size:" + px(theme.fontSizes[FontSize::Medium]) + ";";
        area.style += "min-height:6rem;resize:vertical;";
        if (spec.disabled) {
            area.attrs.push_back(render::DomAttr{"disabled", "true"});
        }
        wrapper.children.push_back(std::move(area));
        nodes.push_back(std::move(wrapper));
        break;
    }
    case ComponentKind::Slider: {
        // progress track + button handle.
        render::DomNode slider;
        slider.tag   = "div";
        slider.style = "display:flex;align-items:center;gap:0.8rem;";
        slider.attrs.push_back(render::DomAttr{"data-component", "slider"});
        render::DomNode track;
        track.tag    = "div";
        track.style  = "flex:1;height:0.8rem;position:relative;";
        track.style += borderImageStyle("progress.linear.bg", theme, resolver);
        slider.children.push_back(std::move(track));
        render::DomNode handle;
        handle.tag    = "div";
        handle.style  = "width:1.6rem;height:1.6rem;";
        handle.style += borderImageStyle("pressable.neutral.default", theme, resolver);
        handle.attrs.push_back(render::DomAttr{"data-component", "sliderHandle"});
        slider.children.push_back(std::move(handle));
        if (!spec.value.empty()) {
            render::DomNode val;
            val.tag    = "div";
            val.text   = spec.value;
            val.style  = "font-family:" + theme.fontUi + ";font-size:" + px(theme.fontSizes[FontSize::Small]) + ";";
            val.style += "color:" + theme.colorText + ";min-width:2rem;text-align:right;";
            slider.children.push_back(std::move(val));
        }
        nodes.push_back(std::move(slider));
        break;
    }
    case ComponentKind::Stepper: {
        // button(-) + value + button(+).
        render::DomNode stepper;
        stepper.tag   = "div";
        stepper.style = "display:inline-flex;align-items:center;gap:0.4rem;";
        stepper.attrs.push_back(render::DomAttr{"data-component", "stepper"});
        auto makeStepBtn = [&](std::string const& glyph) {
            render::DomNode btn;
            btn.tag    = "div";
            btn.style  = "padding:0.4rem 0.8rem;cursor:pointer;";
            btn.style += borderImageStyle("pressable.neutral.default", theme, resolver);
            btn.style += "font-family:" + theme.fontUi + ";color:" + theme.colorText + ";";
            btn.attrs.push_back(render::DomAttr{"data-component", "stepperButton"});
            btn.attrs.push_back(render::DomAttr{"tabindex", "0"});
            btn.text = glyph;
            return btn;
        };
        stepper.children.push_back(makeStepBtn("-"));
        render::DomNode val;
        val.tag    = "div";
        val.text   = spec.value.empty() ? "0" : spec.value;
        val.style  = "font-family:" + theme.fontUi + ";font-size:" + px(theme.fontSizes[FontSize::Medium]) + ";";
        val.style += "color:" + theme.colorText + ";min-width:2.4rem;text-align:center;";
        stepper.children.push_back(std::move(val));
        stepper.children.push_back(makeStepBtn("+"));
        nodes.push_back(std::move(stepper));
        break;
    }
    case ComponentKind::Picker: {
        // dropdown + list (reuse dropdown rendering with a label).
        render::DomNode picker;
        picker.tag   = "div";
        picker.style = "display:inline-block;position:relative;";
        picker.attrs.push_back(render::DomAttr{"data-component", "picker"});
        render::DomNode trigger;
        trigger.tag    = "div";
        trigger.style  = "display:inline-flex;align-items:center;gap:0.6rem;";
        trigger.style += "padding:0.8rem 1.6rem;font-family:" + theme.fontUi + ";";
        trigger.style += "font-size:" + px(theme.fontSizes[FontSize::Medium]) + ";color:" + theme.colorText + ";";
        trigger.style += "cursor:pointer;";
        trigger.style += borderImageStyle("pressable.neutral.default", theme, resolver);
        trigger.attrs.push_back(render::DomAttr{"data-component", "pickerTrigger"});
        trigger.attrs.push_back(render::DomAttr{"tabindex", "0"});
        trigger.text = spec.value.empty() ? (spec.label.empty() ? "Select" : spec.label) : spec.value;
        picker.children.push_back(std::move(trigger));
        if (!spec.children.empty()) {
            render::DomNode options;
            options.tag    = "div";
            options.style  = "position:absolute;top:100%;left:0;margin-top:0.4rem;";
            options.style += "display:flex;flex-direction:column;min-width:100%;";
            options.style += borderImageStyle("panel.default", theme, resolver);
            options.style += "padding:0.4rem;z-index:10;";
            options.attrs.push_back(render::DomAttr{"data-component", "pickerOptions"});
            for (auto const& child : spec.children) {
                auto rendered = renderComponent(child, theme, resolver);
                for (auto& node : rendered) {
                    options.children.push_back(std::move(node));
                }
            }
            picker.children.push_back(std::move(options));
        }
        nodes.push_back(std::move(picker));
        break;
    }
    case ComponentKind::Icon: {
        auto const* icon = VanillaAssets::icon(spec.icon);
        if (icon == nullptr) {
            break; // unknown icon -> render nothing
        }
        render::DomNode ic;
        ic.tag    = "div";
        ic.style  = "width:" + icon->width + ";height:" + icon->height + ";";
        ic.style += "background-image:url(" + resolver.resolveTexture(TextureSpec{icon->source, {}, {}, {}}) + ");";
        ic.style += "background-size:contain;background-repeat:no-repeat;background-position:center;";
        ic.attrs.push_back(render::DomAttr{"data-component", "icon"});
        nodes.push_back(std::move(ic));
        break;
    }
    case ComponentKind::Image: {
        render::DomNode img;
        img.tag        = "img";
        auto const src = spec.src.empty() ? spec.icon : spec.src;
        if (src.empty()) {
            break; // no source -> render nothing
        }
        img.attrs.push_back(render::DomAttr{"src", src});
        img.attrs.push_back(render::DomAttr{"data-component", "image"});
        if (!spec.style.empty() && spec.style != "normal") {
            img.style = spec.style;
        }
        nodes.push_back(std::move(img));
        break;
    }
    case ComponentKind::Badge: {
        // bubble + text (corner badge).
        render::DomNode badge;
        badge.tag    = "div";
        badge.style  = borderImageStyle("bubble.base.default", theme, resolver);
        badge.style += "display:inline-block;padding:0.2rem 0.6rem;";
        badge.style += "font-family:" + theme.fontUi + ";font-size:" + px(theme.fontSizes[FontSize::Tiny]) + ";";
        badge.style += "color:" + theme.colorText + ";";
        badge.attrs.push_back(render::DomAttr{"data-component", "badge"});
        if (spec.label.empty()) {
            renderChildren(spec, theme, resolver, badge.children);
        } else {
            badge.text = spec.label;
        }
        nodes.push_back(std::move(badge));
        break;
    }
    default:
        // Kinds not yet implemented render an empty forest.
        break;
    }

    return nodes;
}

namespace {

void appendEscapedHtml(std::string& out, std::string_view value) {
    for (char c : value) {
        switch (c) {
        case '&':
            out += "&amp;";
            break;
        case '<':
            out += "&lt;";
            break;
        case '>':
            out += "&gt;";
            break;
        case '"':
            out += "&quot;";
            break;
        default:
            out.push_back(c);
            break;
        }
    }
}

void appendDomNodeHtml(std::string& out, render::DomNode const& node) {
    auto const tag  = node.tag.empty() ? "div" : node.tag;
    out            += '<';
    out            += tag;
    if (!node.style.empty()) {
        out += " style=\"";
        appendEscapedHtml(out, node.style);
        out += '"';
    }
    for (auto const& attr : node.attrs) {
        out += ' ';
        out += attr.name;
        out += "=\"";
        appendEscapedHtml(out, attr.value);
        out += '"';
    }
    out += '>';
    if (!node.text.empty()) {
        appendEscapedHtml(out, node.text);
    }
    for (auto const& child : node.children) {
        appendDomNodeHtml(out, child);
    }
    // Void elements (input/img/br/...) have no closing tag in HTML.
    if (tag != "input" && tag != "img" && tag != "br" && tag != "hr" && tag != "meta" && tag != "link") {
        out += "</";
        out += tag;
        out += '>';
    }
}

} // namespace

std::string renderComponentToHtml(ComponentSpec const& spec, ThemeTokens const& theme, IAssetResolver const& resolver) {
    std::string html;
    for (auto const& node : renderComponent(spec, theme, resolver)) {
        appendDomNodeHtml(html, node);
    }
    return html;
}

} // namespace dearoreui::component
