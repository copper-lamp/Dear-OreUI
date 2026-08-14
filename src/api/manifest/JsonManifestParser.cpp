#include "api/manifest/JsonManifestParser.h"

#include <cctype>
#include <sstream>

namespace dearoreui::api {

JsonParser::JsonParser(std::string_view text) : mText(text) {}

Result<JsonValue> JsonParser::parse() {
    skipWhitespace();
    if (eof()) {
        return error("Empty JSON document");
    }

    auto value = parseValue();
    if (value.isErr()) {
        return value.error();
    }

    skipWhitespace();
    if (!eof()) {
        return error("Unexpected trailing content after JSON value");
    }
    return value.value();
}

Result<JsonValue> JsonParser::parseValue() {
    skipWhitespace();
    if (eof()) {
        return error("Unexpected end of JSON value");
    }

    char current = peek();
    switch (current) {
    case '{':
        return parseObject();
    case '[':
        return parseArray();
    case '"':
        return parseString();
    case 't':
        return parseLiteral("true", JsonValue{true});
    case 'f':
        return parseLiteral("false", JsonValue{false});
    case 'n':
        return parseLiteral("null", JsonValue{});
    default:
        if (current == '-' || std::isdigit(static_cast<unsigned char>(current))) {
            return parseNumber();
        }
        return error(std::string{"Unexpected character: "} + current);
    }
}

Result<JsonValue> JsonParser::parseObject() {
    std::map<std::string, JsonValue> object;
    advance(); // consume '{'

    skipWhitespace();
    if (consume('}')) {
        return JsonValue{std::move(object)};
    }

    while (!eof()) {
        skipWhitespace();
        if (peek() != '"') {
            return error("Expected object key string");
        }

        auto keyResult = parseString();
        if (keyResult.isErr()) {
            return keyResult.error();
        }
        std::string key = keyResult.value().asString();

        skipWhitespace();
        if (!consume(':')) {
            return error("Expected ':' after object key");
        }

        auto valueResult = parseValue();
        if (valueResult.isErr()) {
            return valueResult.error();
        }
        object.emplace(std::move(key), std::move(valueResult.value()));

        skipWhitespace();
        if (consume(',')) {
            continue;
        }
        if (consume('}')) {
            return JsonValue{std::move(object)};
        }
        return error("Expected ',' or '}' in object");
    }

    return error("Unterminated object");
}

Result<JsonValue> JsonParser::parseArray() {
    std::vector<JsonValue> array;
    advance(); // consume '['

    skipWhitespace();
    if (consume(']')) {
        return JsonValue{std::move(array)};
    }

    while (!eof()) {
        auto valueResult = parseValue();
        if (valueResult.isErr()) {
            return valueResult.error();
        }
        array.push_back(std::move(valueResult.value()));

        skipWhitespace();
        if (consume(',')) {
            continue;
        }
        if (consume(']')) {
            return JsonValue{std::move(array)};
        }
        return error("Expected ',' or ']' in array");
    }

    return error("Unterminated array");
}

Result<JsonValue> JsonParser::parseString() {
    advance(); // consume opening '"'

    std::string result;
    while (!eof()) {
        char current = advance();
        if (current == '"') {
            return JsonValue{std::move(result)};
        }
        if (current != '\\') {
            result.push_back(current);
            continue;
        }

        if (eof()) {
            return error("Unterminated escape sequence");
        }
        char escaped = advance();
        switch (escaped) {
        case '"':
            result.push_back('"');
            break;
        case '\\':
            result.push_back('\\');
            break;
        case '/':
            result.push_back('/');
            break;
        case 'b':
            result.push_back('\b');
            break;
        case 'f':
            result.push_back('\f');
            break;
        case 'n':
            result.push_back('\n');
            break;
        case 'r':
            result.push_back('\r');
            break;
        case 't':
            result.push_back('\t');
            break;
        case 'u': {
            if (mPos + 4 > mText.size()) {
                return error("Incomplete unicode escape");
            }
            auto hex  = mText.substr(mPos, 4);
            mPos     += 4;
            std::uint32_t      codePoint{};
            std::istringstream stream{std::string{hex}};
            stream >> std::hex >> codePoint;
            if (stream.fail()) {
                return error("Invalid unicode escape");
            }
            if (codePoint <= 0x7F) {
                result.push_back(static_cast<char>(codePoint));
            } else if (codePoint <= 0x7FF) {
                result.push_back(static_cast<char>(0xC0 | ((codePoint >> 6) & 0x1F)));
                result.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
            } else {
                result.push_back(static_cast<char>(0xE0 | ((codePoint >> 12) & 0x0F)));
                result.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
                result.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
            }
            break;
        }
        default:
            return error(std::string{"Invalid escape character: "} + escaped);
        }
    }

    return error("Unterminated string");
}

Result<JsonValue> JsonParser::parseNumber() {
    std::size_t start = mPos;
    if (peek() == '-') {
        advance();
    }

    while (!eof() && std::isdigit(static_cast<unsigned char>(peek()))) {
        advance();
    }

    if (!eof() && peek() == '.') {
        advance();
        while (!eof() && std::isdigit(static_cast<unsigned char>(peek()))) {
            advance();
        }
    }

    if (!eof() && (peek() == 'e' || peek() == 'E')) {
        advance();
        if (!eof() && (peek() == '+' || peek() == '-')) {
            advance();
        }
        while (!eof() && std::isdigit(static_cast<unsigned char>(peek()))) {
            advance();
        }
    }

    auto token = mText.substr(start, mPos - start);
    try {
        double value = std::stod(std::string{token});
        return JsonValue{value};
    } catch (...) {
        return error("Invalid number format");
    }
}

Result<JsonValue> JsonParser::parseLiteral(std::string_view literal, JsonValue value) {
    if (mPos + literal.size() > mText.size()) {
        return error(std::string{"Expected literal: "} + std::string{literal});
    }
    if (mText.substr(mPos, literal.size()) == literal) {
        mPos += literal.size();
        return value;
    }
    return error(std::string{"Expected literal: "} + std::string{literal});
}

void JsonParser::skipWhitespace() {
    while (!eof()) {
        char current = peek();
        if (current == ' ' || current == '\t' || current == '\n' || current == '\r') {
            advance();
        } else {
            break;
        }
    }
}

char JsonParser::peek() const {
    if (eof()) {
        return '\0';
    }
    return mText[mPos];
}

char JsonParser::advance() {
    if (eof()) {
        return '\0';
    }
    return mText[mPos++];
}

bool JsonParser::consume(char expected) {
    if (peek() == expected) {
        ++mPos;
        return true;
    }
    return false;
}

Error JsonParser::error(std::string message) const { return Error{ErrorCode::InvalidFormat, std::move(message)}; }

Result<JsonValue> parseJson(std::string_view text) {
    JsonParser parser{text};
    return parser.parse();
}

namespace {

void serializeJsonValue(JsonValue const& value, std::ostringstream& stream);

void escapeJsonString(std::string_view text, std::ostringstream& stream) {
    stream << '"';
    for (char c : text) {
        switch (c) {
        case '"':
            stream << "\\\"";
            break;
        case '\\':
            stream << "\\\\";
            break;
        case '\b':
            stream << "\\b";
            break;
        case '\f':
            stream << "\\f";
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
                char buffer[7];
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

void serializeJsonObject(std::map<std::string, JsonValue> const& object, std::ostringstream& stream) {
    stream << '{';
    bool first = true;
    for (auto const& [key, value] : object) {
        if (!first) stream << ',';
        first = false;
        escapeJsonString(key, stream);
        stream << ':';
        serializeJsonValue(value, stream);
    }
    stream << '}';
}

void serializeJsonArray(std::vector<JsonValue> const& array, std::ostringstream& stream) {
    stream << '[';
    bool first = true;
    for (auto const& value : array) {
        if (!first) stream << ',';
        first = false;
        serializeJsonValue(value, stream);
    }
    stream << ']';
}

void serializeJsonValue(JsonValue const& value, std::ostringstream& stream) {
    switch (value.type()) {
    case JsonValue::Type::Null:
        stream << "null";
        break;
    case JsonValue::Type::Boolean:
        stream << (value.asBoolean() ? "true" : "false");
        break;
    case JsonValue::Type::Number: {
        std::ostringstream numberStream;
        numberStream << value.asNumber();
        auto text = numberStream.str();
        if (text.find('.') == std::string::npos && text.find('e') == std::string::npos
            && text.find('E') == std::string::npos) {
            text += ".0";
        }
        stream << text;
        break;
    }
    case JsonValue::Type::String:
        escapeJsonString(value.asString(), stream);
        break;
    case JsonValue::Type::Array:
        serializeJsonArray(value.asArray(), stream);
        break;
    case JsonValue::Type::Object:
        serializeJsonObject(value.asObject(), stream);
        break;
    }
}

} // namespace

std::string serializeJson(JsonValue const& value) {
    std::ostringstream stream;
    serializeJsonValue(value, stream);
    return stream.str();
}

} // namespace dearoreui::api
