#pragma once

#include <string_view>

namespace dearoreui::diagnostic {

enum class Severity {
    Debug,
    Info,
    Warning,
    Error,
    Critical,
};

[[nodiscard]] constexpr std::string_view severityName(Severity severity) {
    switch (severity) {
    case Severity::Debug:
        return "debug";
    case Severity::Info:
        return "info";
    case Severity::Warning:
        return "warning";
    case Severity::Error:
        return "error";
    case Severity::Critical:
        return "critical";
    }
    return "unknown";
}

} // namespace dearoreui::diagnostic
