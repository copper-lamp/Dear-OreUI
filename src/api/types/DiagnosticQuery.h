#pragma once

#include "api/types/Error.h"
#include "diagnostic/DiagnosticSeverity.h"
#include "api/types/Id.h"

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace dearoreui::api {

enum class DiagnosticSeverity { Debug, Info, Warning, Error, Critical };

struct DiagnosticQuery {
    ModId requester;
    std::optional<ContextId> context;
    std::optional<ModId> mod;
    std::optional<DiagnosticSeverity> minimumSeverity;
    std::optional<std::chrono::system_clock::time_point> since;
    std::size_t limit{200};
};

struct DiagnosticInfo {
    DiagnosticId id;
    std::chrono::system_clock::time_point timestamp;
    DiagnosticSeverity severity{DiagnosticSeverity::Info};
    std::string category;
    std::string event;
    std::optional<ContextId> context;
    std::optional<ModId> mod;
    std::optional<PageId> page;
    std::optional<ErrorCode> error;
    std::string message;
};

struct DiagnosticList {
    std::vector<DiagnosticInfo> items;
    bool truncated{false};
};

} // namespace dearoreui::api
