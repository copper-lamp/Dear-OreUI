#pragma once

#include <string>

namespace dearoreui::api {

struct Dependency {
    std::string modNamespace;
    std::string versionRange;
    bool        optional{false};

    [[nodiscard]] bool operator==(Dependency const& other) const {
        return modNamespace == other.modNamespace && versionRange == other.versionRange && optional == other.optional;
    }

    [[nodiscard]] bool operator!=(Dependency const& other) const { return !(*this == other); }
};

} // namespace dearoreui::api
