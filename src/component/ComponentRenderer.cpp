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

[[nodiscard]] std::string buttonStyle(std::string_view variant, ThemeTokens const& theme) {
    std::ostringstream style;
    style << "display:inline-block;";
    style << "padding:10px 24px;";
    style << "font-family:" << theme.fontUi << ";";
    style << "font-size:" << px(theme.fontSizes[FontSize::Medium]) << ";";
    style << "line-height:1;";
    style << "color:" << theme.colorText << ";";
    style << "border:3px solid " << theme.colorText << ";";
    style << "border-radius:4px;";
    style << "cursor:pointer;";
    if (variant == "primary") {
        style << "background:" << theme.colorPrimary << ";";
    } else if (variant == "destructive") {
        style << "background:" << theme.colorDestructive << ";";
    } else if (variant == "secondary") {
        style << "background:" << theme.colorSecondary << ";";
        style << "color:#1a1a1a;";
        style << "border-color:" << theme.colorSecondary << ";";
    } else if (variant == "neutral") {
        style << "background:" << theme.colorPanel << ";";
    } else { // elevated
        style << "background:" << theme.colorPanel << ";";
        style << "border:3px solid " << theme.colorSecondary << ";";
        style << "box-shadow:0 4px 0 rgba(0,0,0,0.5);";
    }
    return style.str();
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
        button.style = buttonStyle(spec.variant, theme);
        button.attrs.push_back(render::DomAttr{"data-component", "button"});
        if (spec.disabled) {
            button.attrs.push_back(render::DomAttr{"aria-disabled", "true"});
            button.style += "opacity:0.5;cursor:default;";
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
        panel.style = "display:flex;flex-direction:column;";
        panel.style += "background:" + theme.colorPanel + ";";
        panel.style += "border:3px solid " + theme.colorSecondary + ";";
        panel.style += "border-radius:6px;padding:16px 20px;";
        panel.attrs.push_back(render::DomAttr{"data-component", "panel"});
        if (!spec.label.empty()) {
            render::DomNode title;
            title.tag   = "div";
            title.style = "font-family:" + theme.fontHeading + ";font-size:" + px(theme.fontSizes[FontSize::Large]) + ";";
            title.style += "color:" + theme.colorText + ";margin-bottom:12px;";
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
        card.style = "background:" + theme.colorPanel + ";";
        card.style += "border:2px solid " + theme.colorSecondary + ";";
        card.style += "border-radius:4px;padding:12px 16px;";
        card.attrs.push_back(render::DomAttr{"data-component", "card"});
        renderChildren(spec, theme, resolver, card.children);
        nodes.push_back(std::move(card));
        break;
    }
    case ComponentKind::ListItem: {
        render::DomNode item;
        item.tag   = "div";
        item.style = "display:flex;align-items:center;padding:10px 14px;";
        item.style += "background:" + theme.colorPanel + ";";
        item.style += "border-bottom:2px solid " + theme.colorSecondary + ";";
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
        field.style = "width:100%;box-sizing:border-box;padding:10px 12px;";
        field.style += "background:#1e1e1f;color:" + theme.colorText + ";";
        field.style += "border:2px solid " + theme.colorSecondary + ";border-radius:4px;";
        field.style += "font-family:" + theme.fontUi + ";font-size:" + px(theme.fontSizes[FontSize::Medium]) + ";";
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
        bar.style = "display:flex;gap:4px;";
        bar.attrs.push_back(render::DomAttr{"data-component", "tabBar"});
        for (auto const& child : spec.children) {
            render::DomNode tab;
            tab.tag   = "div";
            tab.text  = child.label;
            tab.style = "padding:8px 18px;font-family:" + theme.fontUi + ";";
            tab.style += "font-size:" + px(theme.fontSizes[FontSize::Small]) + ";color:" + theme.colorText + ";";
            tab.style += "background:" + theme.colorPanel + ";border:2px solid " + theme.colorSecondary + ";";
            tab.style += "border-radius:4px 4px 0 0;";
            bar.children.push_back(std::move(tab));
        }
        nodes.push_back(std::move(bar));
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
    out += '<';
    out += node.tag.empty() ? "div" : node.tag;
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
    out += "</";
    out += node.tag.empty() ? "div" : node.tag;
    out += '>';
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
