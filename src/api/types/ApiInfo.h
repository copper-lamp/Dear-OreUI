#pragma once

#include "api/types/Version.h"

#include <cstdint>
#include <string>

namespace dearoreui::api {

struct ApiInfo {
    std::uint32_t protocolVersion{1};
    Version       modVersion;
    std::string   runtimeState;
    std::string   minecraftVersion;
    std::string   oreuiVersion;
    std::string   coherentVersion;
    bool          ready{false};
};

} // namespace dearoreui::api
