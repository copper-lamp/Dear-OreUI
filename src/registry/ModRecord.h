#pragma once

#include "api/manifest/ModManifest.h"

#include <chrono>

namespace dearoreui::registry {

struct ModRecord {
    api::ModManifest                      manifest;
    bool                                  enabled{true};
    std::chrono::system_clock::time_point registeredAt;
};

} // namespace dearoreui::registry
