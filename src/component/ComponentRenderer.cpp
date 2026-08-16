#include "component/ComponentRenderer.h"

#include <sstream>

namespace dearoreui::component {

namespace {

[[nodiscard]] std::string px(int value) { return std::to_string(value) + "px"; }

// Renders nested component children (excluding raw body) plus raw body nodes.
void renderChildren(
    ComponentSpec const&        spec,
    ThemeTokens const&          theme,
    std::vector<render::DomNode>& out
) {
    for (auto const& child : spec.children) {
        auto rendered = renderComponent(child, theme);
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

std::vector<render::DomNode> renderComponent(ComponentSpec const& spec, ThemeTokens const& theme) {
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
            renderChildren(spec, theme, button.children);
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
        renderChildren(spec, theme, panel.children);
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
        renderChildren(spec, theme, card.children);
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
        renderChildren(spec, theme, item.children);
        nodes.push_back(std::move(item));
        break;
    }
    case ComponentKind::Input: {
        render::DomNode wrapper;
        wrapper.tag   = "div";
        wrapper.style = "display:flex;flex-direction:column;";
        wrapper.attrs.push_back(render::DomAttr{"data-component", "input"});
        if (!spec.label.empty()) {
            render::DomNode hint;
            hint.tag   = "div";
            hint.text  = spec.label;
            hint.style = "font-family:" + theme.fontSubheading + ";font-size:" + px(theme.fontSizes[FontSize::Small]) + ";";
            hint.style += "color:" + theme.colorMuted + ";margin-bottom:6px;";
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

std::string renderComponentToHtml(ComponentSpec const& spec, ThemeTokens const& theme) {
    std::string html;
    for (auto const& node : renderComponent(spec, theme)) {
        appendDomNodeHtml(html, node);
    }
    return html;
}

} // namespace dearoreui::component
