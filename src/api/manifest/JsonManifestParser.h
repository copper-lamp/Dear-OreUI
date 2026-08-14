#pragma once

#include "api/types/Result.h"

#include <map>
#include <string>
#include <vector>

namespace dearoreui::api {

class JsonValue {
public:
    enum class Type {
        Null,
        Boolean,
        Number,
        String,
        Array,
        Object,
    };

    JsonValue() = default;

    explicit JsonValue(Type type) : mType(type) {}
    explicit JsonValue(bool value) : mType(Type::Boolean), mBoolean(value) {}
    explicit JsonValue(double value) : mType(Type::Number), mNumber(value) {}
    explicit JsonValue(std::string value) : mType(Type::String), mString(std::move(value)) {}
    explicit JsonValue(std::vector<JsonValue> values) : mType(Type::Array), mArray(std::move(values)) {}
    explicit JsonValue(std::map<std::string, JsonValue> values) : mType(Type::Object), mObject(std::move(values)) {}

    [[nodiscard]] Type type() const { return mType; }

    [[nodiscard]] bool isNull() const { return mType == Type::Null; }
    [[nodiscard]] bool isBoolean() const { return mType == Type::Boolean; }
    [[nodiscard]] bool isNumber() const { return mType == Type::Number; }
    [[nodiscard]] bool isString() const { return mType == Type::String; }
    [[nodiscard]] bool isArray() const { return mType == Type::Array; }
    [[nodiscard]] bool isObject() const { return mType == Type::Object; }

    [[nodiscard]] bool                                    asBoolean() const { return mBoolean; }
    [[nodiscard]] double                                  asNumber() const { return mNumber; }
    [[nodiscard]] std::string const&                      asString() const { return mString; }
    [[nodiscard]] std::vector<JsonValue> const&           asArray() const { return mArray; }
    [[nodiscard]] std::map<std::string, JsonValue> const& asObject() const { return mObject; }

    [[nodiscard]] JsonValue const* find(std::string_view key) const {
        auto iterator = mObject.find(std::string{key});
        return iterator == mObject.end() ? nullptr : &iterator->second;
    }

private:
    Type                             mType{Type::Null};
    bool                             mBoolean{false};
    double                           mNumber{0.0};
    std::string                      mString;
    std::vector<JsonValue>           mArray;
    std::map<std::string, JsonValue> mObject;
};

class JsonParser {
public:
    explicit JsonParser(std::string_view text);

    [[nodiscard]] Result<JsonValue> parse();

private:
    [[nodiscard]] Result<JsonValue> parseValue();
    [[nodiscard]] Result<JsonValue> parseObject();
    [[nodiscard]] Result<JsonValue> parseArray();
    [[nodiscard]] Result<JsonValue> parseString();
    [[nodiscard]] Result<JsonValue> parseNumber();
    [[nodiscard]] Result<JsonValue> parseLiteral(std::string_view literal, JsonValue value);

    void                skipWhitespace();
    [[nodiscard]] bool  eof() const { return mPos >= mText.size(); }
    [[nodiscard]] char  peek() const;
    char                advance();
    bool                consume(char expected);
    [[nodiscard]] Error error(std::string message) const;

    std::string_view mText;
    std::size_t      mPos{0};
};

[[nodiscard]] Result<JsonValue> parseJson(std::string_view text);
[[nodiscard]] std::string       serializeJson(JsonValue const& value);

} // namespace dearoreui::api
