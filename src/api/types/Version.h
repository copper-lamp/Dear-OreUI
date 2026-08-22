#pragma once

#include "api/types/Result.h"

#include <charconv>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace dearoreui::api {

class Version {
public:
    struct Components {
        std::uint32_t major{};
        std::uint32_t minor{};
        std::uint32_t patch{};
        std::string   prerelease;
        std::string   build;
    };

    Version() = default;

    Version(std::uint32_t major, std::uint32_t minor, std::uint32_t patch) : mComponents{major, minor, patch, {}, {}} {}

    explicit Version(Components components) : mComponents(std::move(components)) {}

    [[nodiscard]] static Result<Version> parse(std::string_view text);

    [[nodiscard]] std::uint32_t      major() const { return mComponents.major; }
    [[nodiscard]] std::uint32_t      minor() const { return mComponents.minor; }
    [[nodiscard]] std::uint32_t      patch() const { return mComponents.patch; }
    [[nodiscard]] std::string const& prerelease() const { return mComponents.prerelease; }
    [[nodiscard]] std::string const& build() const { return mComponents.build; }

    [[nodiscard]] std::string toString() const;

    [[nodiscard]] bool operator==(Version const& other) const;
    [[nodiscard]] bool operator!=(Version const& other) const { return !(*this == other); }
    [[nodiscard]] bool operator<(Version const& other) const;
    [[nodiscard]] bool operator>(Version const& other) const { return other < *this; }
    [[nodiscard]] bool operator<=(Version const& other) const { return !(other < *this); }
    [[nodiscard]] bool operator>=(Version const& other) const { return !(*this < other); }

    // Semver-ish compatibility: same major, minor/patch >= required, prerelease is ignored.
    [[nodiscard]] bool satisfies(Version const& required) const;

private:
    Components mComponents;
};

namespace detail {

// Helpers internal to the header-only Version implementation. Each translation
// unit gets its own internal-linkage copy; no ODR risk.
inline Result<std::uint32_t> parseVersionNumber(std::string_view text) {
    std::uint32_t value{};
    auto [pointer, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || pointer != text.data() + text.size()) {
        return Error{ErrorCode::InvalidFormat, "Version component is not a valid number"};
    }
    return value;
}

inline Result<Version::Components> parseVersionCore(std::string_view text) {
    Version::Components components{};
    std::string_view    remaining = text;

    auto prereleaseStart = remaining.find('-');
    auto buildStart      = remaining.find('+');

    if (prereleaseStart != std::string_view::npos && buildStart != std::string_view::npos) {
        if (prereleaseStart > buildStart) {
            return Error{ErrorCode::InvalidFormat, "Prerelease marker must precede build marker"};
        }
        components.prerelease = std::string{remaining.substr(prereleaseStart + 1, buildStart - prereleaseStart - 1)};
        components.build      = std::string{remaining.substr(buildStart + 1)};
        remaining             = remaining.substr(0, prereleaseStart);
    } else if (prereleaseStart != std::string_view::npos) {
        components.prerelease = std::string{remaining.substr(prereleaseStart + 1)};
        remaining             = remaining.substr(0, prereleaseStart);
    } else if (buildStart != std::string_view::npos) {
        components.build = std::string{remaining.substr(buildStart + 1)};
        remaining        = remaining.substr(0, buildStart);
    }

    std::vector<std::uint32_t> numbers;
    std::size_t                start = 0;
    while (start <= remaining.size()) {
        auto end  = remaining.find('.', start);
        auto part = remaining.substr(start, end == std::string_view::npos ? remaining.size() - start : end - start);
        if (part.empty()) {
            return Error{ErrorCode::InvalidFormat, "Empty version component"};
        }
        auto result = parseVersionNumber(part);
        if (result.isErr()) return result.error();
        numbers.push_back(result.value());
        if (end == std::string_view::npos) break;
        start = end + 1;
    }

    if (numbers.size() < 2 || numbers.size() > 4) {
        return Error{ErrorCode::InvalidFormat, "Version must have 2 to 4 numeric components"};
    }

    components.major = numbers[0];
    components.minor = numbers[1];
    if (numbers.size() >= 3) components.patch = numbers[2];

    return components;
}

inline int comparePrerelease(std::string const& left, std::string const& right) {
    if (left.empty() && right.empty()) return 0;
    if (left.empty()) return 1;
    if (right.empty()) return -1;

    std::size_t lpos = 0;
    std::size_t rpos = 0;
    while (lpos < left.size() || rpos < right.size()) {
        auto lend  = left.find('.', lpos);
        auto rend  = right.find('.', rpos);
        auto lpart = left.substr(lpos, lend == std::string_view::npos ? left.size() - lpos : lend - lpos);
        auto rpart = right.substr(rpos, rend == std::string_view::npos ? right.size() - rpos : rend - rpos);

        auto lnum = parseVersionNumber(lpart);
        auto rnum = parseVersionNumber(rpart);
        if (lnum.isOk() && rnum.isOk()) {
            auto lv = lnum.value();
            auto rv = rnum.value();
            if (lv < rv) return -1;
            if (lv > rv) return 1;
        } else if (lnum.isOk()) {
            return -1;
        } else if (rnum.isOk()) {
            return 1;
        } else {
            if (lpart < rpart) return -1;
            if (lpart > rpart) return 1;
        }

        lpos = lend == std::string_view::npos ? left.size() : lend + 1;
        rpos = rend == std::string_view::npos ? right.size() : rend + 1;
    }
    return 0;
}

} // namespace detail

inline Result<Version> Version::parse(std::string_view text) {
    auto result = detail::parseVersionCore(text);
    if (result.isErr()) return result.error();
    return Version(result.value());
}

inline std::string Version::toString() const {
    std::ostringstream output;
    output << mComponents.major << '.' << mComponents.minor;
    if (mComponents.patch != 0 || !mComponents.prerelease.empty() || !mComponents.build.empty()) {
        output << '.' << mComponents.patch;
    }
    if (!mComponents.prerelease.empty()) output << '-' << mComponents.prerelease;
    if (!mComponents.build.empty()) output << '+' << mComponents.build;
    return output.str();
}

inline bool Version::operator==(Version const& other) const {
    return std::tie(mComponents.major, mComponents.minor, mComponents.patch)
            == std::tie(other.mComponents.major, other.mComponents.minor, other.mComponents.patch)
        && mComponents.prerelease == other.mComponents.prerelease;
}

inline bool Version::operator<(Version const& other) const {
    auto left  = std::tie(mComponents.major, mComponents.minor, mComponents.patch);
    auto right = std::tie(other.mComponents.major, other.mComponents.minor, other.mComponents.patch);
    if (left != right) return left < right;
    return detail::comparePrerelease(mComponents.prerelease, other.mComponents.prerelease) < 0;
}

inline bool Version::satisfies(Version const& required) const {
    if (mComponents.major != required.mComponents.major) return false;
    if (mComponents.minor < required.mComponents.minor) return false;
    if (mComponents.minor == required.mComponents.minor && mComponents.patch < required.mComponents.patch) {
        return false;
    }
    return true;
}

} // namespace dearoreui::api
