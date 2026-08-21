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
    }
    if (!node.stateStyles.empty()) {
        // M8.1.2: per-state cssText variants (state -> full cssText). The
        // bootstrap stores them on the element and swaps element.style.cssText
        // on hover/pressed/focused events.
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
