#include "render/HtmlDomParser.h"

#include <cctype>

namespace dearoreui::render {

namespace {

[[nodiscard]] bool isWhitespace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f'; }

[[nodiscard]] bool isNameChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '-' || c == '_' || c == ':';
}

[[nodiscard]] bool isVoidTag(std::string_view tag) {
    return tag == "br" || tag == "img" || tag == "input" || tag == "hr" || tag == "meta" || tag == "link";
}

struct Cursor {
    std::string_view source;
    std::size_t      position{0};

    [[nodiscard]] bool eof() const { return position >= source.size(); }

    [[nodiscard]] char peek(std::size_t offset = 0) const {
        return position + offset < source.size() ? source[position + offset] : '\0';
    }

    void advance(std::size_t count = 1) { position += count; }

    void skipWhitespace() {
        while (!eof() && isWhitespace(peek())) {
            advance();
        }
    }
};

[[nodiscard]] std::string readName(Cursor& cursor) {
    std::string name;
    while (!cursor.eof() && isNameChar(cursor.peek())) {
        name.push_back(cursor.peek());
        cursor.advance();
    }
    return name;
}

[[nodiscard]] std::string readAttributeValue(Cursor& cursor) {
    cursor.skipWhitespace();
    if (cursor.eof()) {
        return {};
    }
    char const quote = cursor.peek();
    if (quote != '"' && quote != '\'') {
        // Unquoted value: read until whitespace or '>'.
        std::string value;
        while (!cursor.eof() && !isWhitespace(cursor.peek()) && cursor.peek() != '>') {
            value.push_back(cursor.peek());
            cursor.advance();
        }
        return value;
    }
    cursor.advance(); // opening quote
    std::string value;
    while (!cursor.eof() && cursor.peek() != quote) {
        value.push_back(cursor.peek());
        cursor.advance();
    }
    if (!cursor.eof()) {
        cursor.advance(); // closing quote
    }
    return value;
}

// Parses attributes of an opening tag. Returns false on structural problems.
bool parseAttributes(Cursor& cursor, DomNode& node) {
    for (;;) {
        cursor.skipWhitespace();
        if (cursor.eof()) {
            return false;
        }
        if (cursor.peek() == '>') {
            cursor.advance();
            return true;
        }
        if (cursor.peek() == '/') {
            // Self-closing syntax `<tag />`: consume and expect '>'.
            cursor.advance();
            cursor.skipWhitespace();
            if (cursor.eof() || cursor.peek() != '>') {
                return false;
            }
            cursor.advance();
            return true;
        }
        auto name = readName(cursor);
        if (name.empty()) {
            return false;
        }
        cursor.skipWhitespace();
        std::string value;
        if (cursor.peek() == '=') {
            cursor.advance();
            value = readAttributeValue(cursor);
        }
        if (name == "style") {
            node.style = std::move(value);
        } else {
            node.attrs.push_back(DomAttr{std::move(name), std::move(value)});
        }
    }
}

// Reads text content up to the next '<'.
std::string readText(Cursor& cursor) {
    std::string text;
    while (!cursor.eof() && cursor.peek() != '<') {
        text.push_back(cursor.peek());
        cursor.advance();
    }
    return text;
}

// Parses a fragment starting at an opening tag. Returns the node on success.
// `isRoot` distinguishes the top-level call (may be text-only) from recursion.
bool parseNode(Cursor& cursor, DomNode& node, bool allowTextOnly) {
    cursor.skipWhitespace();
    if (cursor.eof()) {
        return false;
    }
    if (cursor.peek() != '<') {
        if (!allowTextOnly) {
            return false;
        }
        node.tag  = "";
        node.text = readText(cursor);
        return true;
    }
    cursor.advance(); // '<'
    if (cursor.peek() == '/') {
        return false; // stray closing tag at this level
    }
    auto tag = readName(cursor);
    if (tag.empty()) {
        return false;
    }
    node.tag = std::move(tag);
    if (!parseAttributes(cursor, node)) {
        return false;
    }
    if (isVoidTag(node.tag)) {
        return true;
    }
    // Children until the matching closing tag.
    for (;;) {
        cursor.skipWhitespace();
        if (cursor.eof()) {
            return false; // missing closing tag
        }
        if (cursor.peek() == '<' && cursor.peek(1) == '/') {
            cursor.advance(2);
            auto closing = readName(cursor);
            if (closing != node.tag) {
                return false; // mismatched closing tag
            }
            cursor.skipWhitespace();
            if (cursor.peek() == '>') {
                cursor.advance();
            }
            return true;
        }
        DomNode child;
        if (!parseNode(cursor, child, true)) {
            return false;
        }
        if (child.tag.empty() && child.text.empty()) {
            continue;
        }
        node.children.push_back(std::move(child));
    }
}

} // namespace

std::vector<DomNode> parseHtmlFragment(std::string_view html) {
    std::vector<DomNode> forest;
    Cursor               cursor{html, 0};

    for (;;) {
        cursor.skipWhitespace();
        if (cursor.eof()) {
            break;
        }
        DomNode node;
        if (!parseNode(cursor, node, true)) {
            break; // structural failure: return what we have
        }
        if (node.tag.empty() && node.text.empty()) {
            continue;
        }
        forest.push_back(std::move(node));
    }
    return forest;
}

} // namespace dearoreui::render
