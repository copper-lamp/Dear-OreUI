#pragma once

#include "api/types/Page.h"
#include "api/types/Version.h"

#include <string>
#include <vector>

namespace dearoreui::api {

enum class CompatibilityStatus { Compatible, Unsupported, Unknown };

struct CompatibilityRequirement {
    std::string   minecraftVersion;
    Version       oreuiVersion;
    std::string   coherentVersion;
    std::uint32_t protocolVersion{0};
    PageScope     pageScope{PageScope::Any};
    std::string   fingerprint;
};

struct CompatibilityReport {
    CompatibilityStatus      status{CompatibilityStatus::Unknown};
    std::vector<std::string> reasons;
    std::vector<std::string> warnings;
    std::uint32_t            protocolVersion{0};
    Version                  oreuiVersion;
    std::string              minecraftVersion;
    std::string              coherentVersion;
    PageScope                pageScope{PageScope::Any};
    std::string              fingerprint;
};

} // namespace dearoreui::api
