#include "security/HtmlSanitizer.h"

#include "api/types/DomNode.h"
#include "api/types/Error.h"
#include "render/HtmlDomParser.h"
#include "resource/ResourceUri.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_set>

namespace dearoreui::security {

namespace {

[[nodiscard]] std::string toLower(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (char c : text) {
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return result;
}

[[nodiscard]] bool iequals(std::string_view lhs, std::string_view rhs) {
    return lhs.size() == rhs.size()
        && std::equal(lhs.begin(), lhs.end(), rhs.begin(), [](char a, char b) {
               return std::tolower(static_cast<unsigned char>(a))
                   == std::tolower(static_cast<unsigned char>(b));
           });
}

[[nodiscard]] bool startsWithI(std::string_view text, std::string_view prefix) {
    if (text.size() < prefix.size()) {
        return false;
    }
    return iequals(text.substr(0, prefix.size()), prefix);
}

[[nodiscard]] bool isWhitespace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f'; }

// Trims ASCII whitespace from both ends.
[[nodiscard]] std::string_view trim(std::string_view text) {
    std::size_t begin = 0;
    std::size_t end   = text.size();
    while (begin < end && isWhitespace(text[begin])) {
        ++begin;
    }
    while (end > begin && isWhitespace(text[end - 1])) {
        --end;
    }
    return text.substr(begin, end - begin);
}

// Whitelisted element tags (lower-case). Empty tag == text node.
[[nodiscard]] bool isAllowedTag(std::string const& tag) {
    static std::unordered_set<std::string> const allowed = {
        "div",  "span",  "button", "img", "br",   "hr",   "input",
        "label", "ul",    "ol",     "li",  "table", "thead", "tbody",
        "tr",   "th",    "td",     "section", "p",  "strong", "em",
        "b",    "i",     "h1",     "h2",  "h3",  "h4",   "h5",
        "h6",
    };
    return allowed.find(tag) != allowed.end();
}

// Explicitly-forbidden tags (fail even if someday added to the whitelist).
[[nodiscard]] bool isForbiddenTag(std::string const& tag) {
    static std::unordered_set<std::string> const forbidden = {
        "script",  "iframe", "object", "embed",  "link",   "meta",
        "style",   "form",   "base",   "template", "svg",  "math",
        "frameset", "frame", "audio",  "video",  "source",
    };
    return forbidden.find(tag) != forbidden.end();
}

[[nodiscard]] bool isEventAttribute(std::string_view name) {
    return name.size() > 2 && startsWithI(name, "on");
}

// Does the value contain an HTML entity (decimal/hex) that could obfuscate a
// dangerous URL scheme after Coherent decodes it?
[[nodiscard]] bool containsHtmlEntity(std::string_view value) {
    return value.find("&#") != std::string_view::npos;
}

// Rejects dangerous URL protocols. Only relative paths, site-absolute paths
// (starting with '/') and oreui:// internal URIs are allowed.
[[nodiscard]] bool isSafeUrl(std::string_view raw) {
    auto value = trim(raw);
    if (value.empty()) {
        return true;
    }
    if (containsHtmlEntity(value)) {
        return false; // entity-encoded scheme: suspicious
    }
    auto lowered = toLower(value);
    if (startsWithI(lowered, "javascript:")) return false;
    if (startsWithI(lowered, "vbscript:")) return false;
    if (startsWithI(lowered, "file:")) return false;
    if (startsWithI(lowered, "http:")) return false;
    if (startsWithI(lowered, "https:")) return false;
    if (startsWithI(lowered, "data:")) return false;
    if (startsWithI(lowered, "//")) return false; // protocol-relative external
    if (startsWithI(lowered, "oreui://")) {
        // Internal resource: must parse as a valid oreui:// URI (scheme,
        // namespace, path; no traversal).
        auto parsed = resource::ResourceUri::parse(value);
        return parsed.isOk();
    }
    return true;
}

// Extracts the first url(...) token from a cssText declaration.
[[nodiscard]] bool styleHasSafeUrls(std::string_view css) {
    std::string lowered = toLower(css);
    if (lowered.find("expression(") != std::string::npos) {
        return false;
    }
    if (lowered.find("@import") != std::string::npos) {
        return false;
    }
    if (lowered.find("behavior:") != std::string::npos) {
        return false;
    }
    // Check every url(...) token.
    std::size_t pos = 0;
    while ((pos = lowered.find("url(", pos)) != std::string::npos) {
        pos += 4;
        while (pos < lowered.size() && isWhitespace(lowered[pos])) {
            ++pos;
        }
        bool quoted = false;
        if (pos < lowered.size() && (lowered[pos] == '"' || lowered[pos] == '\'')) {
            quoted = true;
            char const quote = lowered[pos];
            ++pos;
            auto start = pos;
            while (pos < lowered.size() && lowered[pos] != quote) {
                ++pos;
            }
            if (!isSafeUrl(lowered.substr(start, pos - start))) {
                return false;
            }
            continue;
        }
        auto start = pos;
        while (pos < lowered.size() && lowered[pos] != ')') {
            ++pos;
        }
        if (!quoted && !isSafeUrl(lowered.substr(start, pos - start))) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] api::Error reject(char const* reason, std::vector<std::string> details = {}) {
    api::Error error{api::ErrorCode::InvalidArgument, std::string{"htmlBody rejected: "} + reason};
    error.details = std::move(details);
    return error;
}

// Recursively validates one parsed DomNode.
[[nodiscard]] api::Result<void> checkNode(api::DomNode const& node) {
    auto tag = toLower(node.tag);
    if (!tag.empty()) {
        if (isForbiddenTag(tag)) {
            return reject("forbidden tag", {tag});
        }
        if (!isAllowedTag(tag)) {
            return reject("tag not in whitelist", {tag});
        }
    }

    for (auto const& attr : node.attrs) {
        if (isEventAttribute(attr.name)) {
            return reject("event attribute", {attr.name});
        }
        auto loweredName = toLower(attr.name);
        if (loweredName == "href" || loweredName == "src" || loweredName == "action") {
            if (!isSafeUrl(attr.value)) {
                return reject("unsafe url in attribute", {loweredName, attr.value});
            }
        }
    }

    if (!styleHasSafeUrls(node.style)) {
        return reject("unsafe style declaration");
    }

    for (auto const& child : node.children) {
        auto result = checkNode(child);
        if (result.isErr()) {
            return result;
        }
    }

    // Text content: catch stray script markers (e.g. comment-wrapped).
    std::string loweredText = toLower(node.text);
    if (loweredText.find("<script") != std::string::npos) {
        return reject("script marker in text content");
    }

    return api::Result<void>::success();
}

} // namespace

api::Result<void> HtmlSanitizer::validate(std::string_view htmlBody) {
    // Fast-fail on obvious script/iframe markers before parsing.
    std::string lowered = toLower(htmlBody);
    if (lowered.find("<script") != std::string::npos) {
        return reject("<script> tag");
    }
    if (lowered.find("<iframe") != std::string::npos) {
        return reject("<iframe> tag");
    }

    auto forest = render::parseHtmlFragment(htmlBody);
    for (auto const& node : forest) {
        auto result = checkNode(node);
        if (result.isErr()) {
            return result;
        }
    }
    return api::Result<void>::success();
}

} // namespace dearoreui::security
