#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <type_traits>

namespace dearoreui::api {

template <typename Tag, typename Value>
class StrongId {
public:
    using ValueType = Value;

    StrongId() = default;

    explicit StrongId(Value value) : mValue(std::move(value)) {}

    [[nodiscard]] Value const& value() const { return mValue; }

    [[nodiscard]] bool isValid() const
        requires std::is_arithmetic_v<Value>
    {
        return mValue != Value{};
    }

    [[nodiscard]] bool isValid() const
        requires std::is_same_v<Value, std::string>
    {
        return !mValue.empty();
    }

    [[nodiscard]] bool operator==(StrongId const& other) const { return mValue == other.mValue; }

    [[nodiscard]] bool operator!=(StrongId const& other) const { return !(*this == other); }

    [[nodiscard]] bool operator<(StrongId const& other) const { return mValue < other.mValue; }

private:
    Value mValue{};
};

using ContextId          = StrongId<struct ContextIdTag, std::uint64_t>;
using ModId              = StrongId<struct ModIdTag, std::string>;
using PageId             = StrongId<struct PageIdTag, std::string>;
using ChangeId           = StrongId<struct ChangeIdTag, std::uint64_t>;
using RequestId          = StrongId<struct RequestIdTag, std::uint64_t>;
using DiagnosticId       = StrongId<struct DiagnosticIdTag, std::uint64_t>;
using RegistrationHandle = StrongId<struct RegistrationHandleTag, std::uint64_t>;
using SubscriptionHandle = StrongId<struct SubscriptionHandleTag, std::uint64_t>;

} // namespace dearoreui::api

namespace std {

template <typename Tag, typename Value>
struct hash<dearoreui::api::StrongId<Tag, Value>> {
    [[nodiscard]] std::size_t operator()(dearoreui::api::StrongId<Tag, Value> const& id) const {
        return std::hash<Value>{}(id.value());
    }
};

} // namespace std
