#pragma once

#include "api/types/Error.h"
#include "api/types/Id.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace dearoreui::source {

struct PageSourceSnapshot {
    api::ContextId                                             contextId;
    std::unordered_map<std::string, std::string>               textResources;
    std::unordered_map<std::string, std::vector<std::uint8_t>> binaryResources;
    std::chrono::system_clock::time_point                      capturedAt;
    bool                                                       partial{false};
    std::vector<api::Error>                                    errors;
};

} // namespace dearoreui::source
