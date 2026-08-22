#pragma once

#include "api/types/Id.h"
#include "api/types/Result.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <cctype>

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

[[nodiscard]] inline Result<void> validateJsonPayload(std::string_view payload) {
    if (payload.empty()) return Error{ErrorCode::InvalidFormat, "event payload is empty"};
    int depth = 0; bool quoted = false; bool escaped = false;
    for (char c : payload) {
        if (quoted) { if (escaped) escaped = false; else if (c == '\\') escaped = true; else if (c == '"') quoted = false; continue; }
        if (c == '"') { quoted = true; continue; }
        if (c == '{' || c == '[') ++depth;
        else if (c == '}' || c == ']') { if (--depth < 0) return Error{ErrorCode::InvalidFormat, "event payload has unbalanced JSON"}; }
    }
    if (quoted || depth != 0) return Error{ErrorCode::InvalidFormat, "event payload is not complete JSON"};
    return Result<void>::success();
}

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
