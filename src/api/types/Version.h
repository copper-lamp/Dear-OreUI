#pragma once

#include "api/types/Result.h"

#include <cstdint>
#include <optional>
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

} // namespace dearoreui::api
