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
[[nodiscard]] std::string borderImageStyle(
    std::string_view      key,
    ThemeTokens const&    theme,
    IAssetResolver const& resolver
) {
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
    std::string const& key,
    std::string const& baseStyle,
    ThemeTokens const& theme,
    IAssetResolver const& resolver
) {
    return borderImageStyle(key, theme, resolver) + baseStyle;
}

// Emits the per-state cssText map for an interactive component (M8.1.2).
// `states` is the ordered state list; `keyPrefix` + state forms the semantic
// texture key. States whose texture key is unknown (e.g. additionalAction has
// no disabled texture) are skipped. Returns the default state's full cssText
// (callers may ignore it and set node.style from the effective state instead).
std::string emitStateStyles(
    render::DomNode& node,
    std::vector<std::string> const& states,
    std::string const& keyPrefix,
    std::string const& baseStyle,
    ThemeTokens const& theme,
    IAssetResolver const& resolver
) {
    std::string defaultStyle;
    for (auto const& state : states) {
        auto texture = borderImageStyle(keyPrefix + state, theme, resolver);
        if (texture.empty()) {
            continue;
        }
        auto full = texture + baseStyle;
        node.stateStyles.emplace_back(state, full);
        if (state == "default") {
            defaultStyle = full;
        }
    }
    return defaultStyle;
}

// Renders nested component children (excluding raw body) plus raw body nodes.
void renderChildren(
    ComponentSpec const&        spec,
    ThemeTokens const&          theme,
    IAssetResolver const&       resolver,
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

std::vector<render::DomNode> renderComponent(
    ComponentSpec const&  spec,
    ThemeTokens const&    theme,
    IAssetResolver const& resolver
) {
    std::vector<render::DomNode> nodes;

    switch (spec.kind) {
    case ComponentKind::Button: {
        render::DomNode button;
        button.tag   = "div";
        // pressable texture family: variant x style(elevated) x state.
        auto const variant   = spec.variant.empty() ? "neutral" : spec.variant;
        auto const keyPrefix = (spec.style == "elevated") ? "pressable.elevated." + variant + "."
                                                          : "pressable." + variant + ".";
        std::string base = "display:inline-block;";
        base += "padding:0.8rem 1.6rem;"; // vanilla padding unknown; reasonable default
        base += "font-family:" + theme.fontUi + ";";
        base += "font-size:" + px(theme.fontSizes[FontSize::Medium]) + ";";
        base += "line-height:1;color:" + theme.colorText + ";";
        base += "cursor:pointer;";
        if (spec.disabled) {
            base += "cursor:default;";
        }
        // M8.1.2: node.style reflects the effective state; stateStyles carries
        // every state so the bootstrap can swap cssText on hover/pressed/focus.
        button.style = textureStyle(keyPrefix + std::string(effectiveState(spec)), base, theme, resolver);
        emitStateStyles(button, {"default", "hovered", "focused", "pressed", "disabled"}, keyPrefix, base, theme, resolver);
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
            panel.style = "background:transparent;";
            panel.style += "height:100%;overflow-y:auto;";
        } else {
            panel.style = borderImageStyle("panel." + variant, theme, resolver);
        }
        panel.style += "display:flex;flex-direction:column;";
        panel.style += "padding:1.6rem 1.6rem 2rem 1.6rem;"; // --panelPadding*
        panel.attrs.push_back(render::DomAttr{"data-component", "panel"});
        if (!spec.label.empty()) {
            render::DomNode title;
            title.tag   = "div";
            title.style = "font-family:" + theme.fontHeading + ";font-size:" + px(theme.fontSizes[FontSize::Large]) + ";";
            title.style += "color:" + theme.colorText + ";margin-bottom:1.2rem;";
            title.text  = spec.label;
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
            text.style = "font-family:" + theme.fontHeading + ";font-size:" + px(theme.fontSizes[FontSize::Heading]) + ";";
            text.style += "color:" + theme.colorText + ";";
        } else if (spec.variant == "subheading") {
            text.style = "font-family:" + theme.fontSubheading + ";font-size:" + px(theme.fontSizes[FontSize::Large]) + ";";
            text.style += "color:" + theme.colorText + ";";
        } else if (spec.variant == "muted") {
            text.style = "font-family:" + theme.fontBody + ";font-size:" + px(theme.fontSizes[FontSize::Small]) + ";";
            text.style += "color:" + theme.colorMuted + ";";
        } else if (spec.variant == "tiny") {
            // Four-step type scale (--fontSizes0..3): tiny/small/medium/large.
            text.style = "font-family:" + theme.fontUi + ";font-size:" + px(theme.fontSizes[FontSize::Tiny]) + ";";
            text.style += "color:" + theme.colorText + ";";
        } else { // ui/body
            text.style = "font-family:" + theme.fontUi + ";font-size:" + px(theme.fontSizes[FontSize::Medium]) + ";";
            text.style += "color:" + theme.colorText + ";";
        }
        nodes.push_back(std::move(text));
        break;
    }
    case ComponentKind::Card: {
        render::DomNode card;
        card.tag   = "div";
        // detailedCard texture family: base / action / additionalAction x state.
        std::string base = "padding:1.2rem 1.6rem;";
        if (spec.variant == "action" || spec.variant == "additionalAction") {
            auto const variant   = spec.variant.empty() ? "action" : spec.variant;
            auto const keyPrefix = "detailedCard." + variant + ".";
            card.style = textureStyle(keyPrefix + std::string(effectiveState(spec)), base, theme, resolver);
            emitStateStyles(card, {"default", "hovered", "pressed", "focused", "pressedFocused", "disabled", "disabledFocused"},
                            keyPrefix, base, theme, resolver);
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
        item.tag   = "div";
        // listItem texture family: base / action / additionalAction x state.
        std::string base = "display:flex;align-items:center;padding:0.8rem 1.4rem;";
        if (spec.variant == "action" || spec.variant == "additionalAction") {
            auto const variant   = spec.variant.empty() ? "action" : spec.variant;
            auto const keyPrefix = "listItem." + variant + ".";
            item.style = textureStyle(keyPrefix + std::string(effectiveState(spec)), base, theme, resolver);
            emitStateStyles(item, {"default", "hovered", "pressed", "focused", "pressedFocused", "disabled", "disabledFocused"},
                            keyPrefix, base, theme, resolver);
            item.attrs.push_back(render::DomAttr{"tabindex", "0"});
        } else {
            // listItem.base only has default/focused textures.
            auto const baseState = (effectiveState(spec) == "focused") ? "focused" : "default";
            item.style = textureStyle("listItem.base." + std::string(baseState), base, theme, resolver);
            emitStateStyles(item, {"default", "focused"}, "listItem.base.", base, theme, resolver);
            item.attrs.push_back(render::DomAttr{"tabindex", "0"});
        }
        item.attrs.push_back(render::DomAttr{"data-component", "listItem"});
        if (!spec.label.empty()) {
            render::DomNode label;
            label.tag   = "div";
            label.text  = spec.label;
            label.style = "font-family:" + theme.fontUi + ";font-size:" + px(theme.fontSizes[FontSize::Medium]) + ";";
            label.style += "color:" + theme.colorText + ";";
            item.children.push_back(std::move(label));
        }
        renderChildren(spec, theme, resolver, item.children);
        nodes.push_back(std::move(item));
        break;
    }
    case ComponentKind::Bubble: {
        render::DomNode bubble;
        bubble.tag = "div";
        bubble.style = borderImageStyle(textureKeyFor(spec), theme, resolver);
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
        bar.tag = "div";
        bar.style = borderImageStyle(textureKeyFor(spec), theme, resolver);
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
        wrapper.tag   = "div";
        // inputLegend (menus-theme): wrapper background + hint typography.
        wrapper.style = "display:flex;flex-direction:column;";
        wrapper.style += "background:#1e1e1f;"; // --inputLegendWrapperBackgroundColor
        wrapper.style += "padding:0 2rem;";     // --inputLegendWrapperPaddingLeft/Right
        wrapper.style += "text-shadow:0.2rem 0.2rem 0rem #303438;"; // --inputLegendWrapperTextShadow
        wrapper.attrs.push_back(render::DomAttr{"data-component", "input"});
        if (!spec.label.empty()) {
            render::DomNode hint;
            hint.tag   = "div";
            hint.text  = spec.label;
            // --inputLegendInputHint*: Minecraft Seven v2, 1.6rem.
            hint.style = "font-family:" + theme.fontUi + ";";
            hint.style += "font-size:1.6rem;line-height:1.6rem;letter-spacing:0.04rem;";
            hint.style += "color:#fff;margin-bottom:0.8rem;"; // --inputLegendInputHintSpaceToLabel
            wrapper.children.push_back(std::move(hint));
        }
        render::DomNode field;
        field.tag   = "input";
        std::string fieldBase = "width:100%;box-sizing:border-box;padding:10px 12px;";
        fieldBase += "background:#1e1e1f;color:" + theme.colorText + ";";
        fieldBase += "border:2px solid " + theme.colorSecondary + ";border-radius:4px;";
        fieldBase += "font-family:" + theme.fontUi + ";font-size:" + px(theme.fontSizes[FontSize::Medium]) + ";";
        field.style = fieldBase;
        // M8.1.2: focus highlight for the input field (no vanilla texture).
        field.stateStyles.emplace_back("default", fieldBase);
        field.stateStyles.emplace_back("focused", fieldBase + "border-color:#fff;outline:none;");
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
        divider.style = "height:0.4rem;"; // --formDividerHeight
        divider.style += "border-top:0.2rem solid rgba(0,0,0,0.3);";    // --formDividerBorderTopColor
        divider.style += "border-bottom:0.2rem solid rgba(255,255,255,0.1);"; // --formDividerBorderBottomColor
        divider.attrs.push_back(render::DomAttr{"data-component", "divider"});
        nodes.push_back(std::move(divider));
        break;
    }
    case ComponentKind::Tooltip: {
        render::DomNode tip;
        tip.tag = "div";
        // Single-texture tooltip (gameplay-theme --tooltip*).
        tip.style = borderImageStyle("tooltip.default", theme, resolver);
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
            key = (tone == "default") ? "containerItem.default" : "containerItem." + tone + ".default";
        }
        slot.style = borderImageStyle(key, theme, resolver);
        slot.style += "position:relative;";
        slot.style += "width:3.8rem;height:3.8rem;"; // --containerItemWidth/Height
        slot.attrs.push_back(render::DomAttr{"data-component", "containerSlot"});
        if (!spec.label.empty()) {
            // Stack amount badge (--containerItemAmountDefaultColor/TextShadow).
            render::DomNode amount;
            amount.tag   = "div";
            amount.text  = spec.label;
            amount.style = "position:absolute;right:0.2rem;bottom:0.2rem;";
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
        key.tag = "div";
        key.style = borderImageStyle("keyIcon.keyboard", theme, resolver);
        key.style += "display:inline-flex;align-items:center;justify-content:center;";
        key.style += "padding:1rem 0.8rem 1.2rem 0.8rem;"; // --buttonIconKeyboardPadding*
        key.attrs.push_back(render::DomAttr{"data-component", "keyIcon"});
        render::DomNode glyph;
        glyph.tag = "div";
        glyph.style = "width:" + icon->width + ";height:" + icon->height + ";";
        glyph.style += "background-image:url("
            + resolver.resolveTexture(TextureSpec{icon->source, {}, {}, {}}) + ");";
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
            auto const state = child.state.empty() ? "default" : child.state;
            std::string base = "padding:0.8rem 1.8rem;font-family:" + theme.fontUi + ";";
            base += "font-size:" + px(theme.fontSizes[FontSize::Small]) + ";color:" + theme.colorText + ";";
            tab.style = textureStyle("tabBar.neutral." + std::string(state), base, theme, resolver);
            emitStateStyles(tab, {"default", "hovered", "focused", "pressed", "pressedFocused"}, "tabBar.neutral.", base, theme, resolver);
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
        auto const radial = (spec.variant == "linear") ? false : true;
        render::DomNode progress;
        progress.tag = "div";
        progress.style = "position:relative;";
        progress.attrs.push_back(render::DomAttr{"data-component", "progress"});
        if (radial) {
            progress.style += "width:2.6rem;height:2.6rem;"; // radial ring footprint
            render::DomNode bg;
            bg.tag   = "div";
            bg.style = borderImageStyle("progress.radial.bg", theme, resolver);
            bg.style += "position:absolute;inset:0;";
            progress.children.push_back(std::move(bg));
            render::DomNode fill;
            fill.tag   = "div";
            fill.style = borderImageStyle("progress.radial.fill", theme, resolver);
            fill.style += "position:absolute;inset:0;";
            fill.style += "animation:animation-249c45b 1s steps(9) infinite;"; // --progressRadialAnimation
            progress.children.push_back(std::move(fill));
        } else {
            progress.style += "width:3.8rem;height:3.8rem;"; // fuel slot footprint
            render::DomNode bg;
            bg.tag   = "div";
            bg.style = borderImageStyle("progress.linear.bg", theme, resolver);
            bg.style += "position:absolute;inset:0;";
            progress.children.push_back(std::move(bg));
            render::DomNode fill;
            fill.tag   = "div";
            fill.style = borderImageStyle("progress.linear.fill", theme, resolver);
            fill.style += "position:absolute;inset:0;";
            fill.style += "animation:animation--2c0f9be3 1s steps(1) infinite;"; // --progressLinearAnimation
            progress.children.push_back(std::move(fill));
        }
        nodes.push_back(std::move(progress));
        break;
    }
    default:
        // Kinds not yet implemented (T1~T4 renderers land in S4~S7) render an
        // empty forest.
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
    auto const tag = node.tag.empty() ? "div" : node.tag;
    out += '<';
    out += tag;
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

std::string renderComponentToHtml(
    ComponentSpec const&  spec,
    ThemeTokens const&    theme,
    IAssetResolver const& resolver
) {
    std::string html;
    for (auto const& node : renderComponent(spec, theme, resolver)) {
        appendDomNodeHtml(html, node);
    }
    return html;
}

} // namespace dearoreui::component
