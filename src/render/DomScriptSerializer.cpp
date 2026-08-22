#include "render/DomScriptSerializer.h"

#include <cstdio>
#include <sstream>

namespace dearoreui::render {

namespace {

void appendJsString(std::ostream& stream, std::string_view value) {
    stream << '"';
    for (char c : value) {
        switch (c) {
        case '\\':
            stream << "\\\\";
            break;
        case '"':
            stream << "\\\"";
            break;
        case '\n':
            stream << "\\n";
            break;
        case '\r':
            stream << "\\r";
            break;
        case '\t':
            stream << "\\t";
            break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                // Escape control characters as \u00XX.
                char buffer[8];
                std::snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned char>(c));
                stream << buffer;
            } else {
                stream << c;
            }
            break;
        }
    }
    stream << '"';
}

void appendNode(std::ostream& stream, DomNode const& node) {
    stream << '{';
    if (!node.tag.empty()) {
        stream << "t:";
        appendJsString(stream, node.tag);
        stream << ',';
    }
    if (!node.style.empty()) {
        stream << "s:";
        appendJsString(stream, node.style);
        stream << ',';
    }
    if (!node.attrs.empty()) {
        stream << "a:[";
        for (std::size_t index = 0; index < node.attrs.size(); ++index) {
            if (index > 0) {
                stream << ',';
            }
            stream << '[';
            appendJsString(stream, node.attrs[index].name);
            stream << ',';
            appendJsString(stream, node.attrs[index].value);
            stream << ']';
        }
        stream << "],";
    }
    if (!node.text.empty()) {
        stream << "x:";
        appendJsString(stream, node.text);
        stream << ',';
    }
    if (!node.children.empty()) {
        stream << "c:[";
        for (std::size_t index = 0; index < node.children.size(); ++index) {
            if (index > 0) {
                stream << ',';
            }
            appendNode(stream, node.children[index]);
        }
        stream << ']';
        // 8.1.5 fix: children is NOT the last property when the node also
        // carries per-state styles (b:/st:) — emitting `c:[...]b:...` without
        // the comma produced a JS SyntaxError that silently killed the whole
        // ExecuteScript (observed in-game: the 5 chunk scripts containing
        // children+states nodes never ran). A trailing comma is legal in
        // object literals, so emit it unconditionally.
        stream << ',';
    }
    if (!node.stateStyles.empty()) {
        // M8.1.2: per-state texture cssText (state -> texture-only cssText).
        // The bootstrap applies baseStyle + stateStyles[state] on switch; the
        // base style is emitted separately (b:) so the injected script stays
        // small (cohtml ExecuteScript silently drops large scripts).
        if (!node.baseStyle.empty()) {
            stream << "b:";
            appendJsString(stream, node.baseStyle);
            stream << ',';
        }
        stream << "st:[";
        for (std::size_t index = 0; index < node.stateStyles.size(); ++index) {
            if (index > 0) {
                stream << ',';
            }
            stream << '[';
            appendJsString(stream, node.stateStyles[index].first);
            stream << ',';
            appendJsString(stream, node.stateStyles[index].second);
            stream << ']';
        }
        stream << ']';
    }
    stream << '}';
}

} // namespace

std::string serializeDomForest(std::vector<DomNode> const& nodes) {
    std::ostringstream stream;
    stream << '[';
    for (std::size_t index = 0; index < nodes.size(); ++index) {
        if (index > 0) {
            stream << ',';
        }
        appendNode(stream, nodes[index]);
    }
    stream << ']';
    return stream.str();
}

} // namespace dearoreui::render
