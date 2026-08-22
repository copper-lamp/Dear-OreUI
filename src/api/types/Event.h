#pragma once

#include "api/types/Id.h"
#include "api/types/Result.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace dearoreui::api {

struct EventPublishOptions {
    ModId owner;
    ContextId context;
    std::string name;
    std::string payload;
};

struct EventPublishResult {
    std::size_t bytes{0};
    bool queued{false};
};

[[nodiscard]] inline Result<void> validateEventName(std::string_view name) {
    if (name.empty() || name.size() > 128) return Error{ErrorCode::InvalidArgument, "event name length is invalid"};
    for (char c : name) {
        if (!(c == '.' || c == '_' || c == '-' || c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c >= '0' && c <= '9')) {
            return Error{ErrorCode::InvalidArgument, "event name contains invalid character"};
        }
    }
    return Result<void>::success();
}

} // namespace dearoreui::api
