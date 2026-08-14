#pragma once

#include "api/types/Error.h"
#include "api/types/Id.h"
#include "api/types/Page.h"
#include "diagnostic/DiagnosticSeverity.h"

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace dearoreui::diagnostic {

struct DiagnosticField {
    std::string key;
    std::string value;

    DiagnosticField() = default;
    DiagnosticField(std::string key, std::string value) : key(std::move(key)), value(std::move(value)) {}
};

struct DiagnosticEvent {
    api::DiagnosticId                     id;
    std::chrono::system_clock::time_point timestamp;
    Severity                              severity{Severity::Info};
    std::string                           category;
    std::string                           event;
    std::optional<api::ContextId>         contextId;
    std::optional<api::ModId>             modId;
    std::optional<api::PageId>            pageId;
    std::optional<api::ErrorCode>         errorCode;
    std::vector<DiagnosticField>          fields;
    std::string                           message;

    [[nodiscard]] std::string const* findField(std::string_view key) const {
        for (auto const& field : fields) {
            if (field.key == key) return &field.value;
        }
        return nullptr;
    }
};

} // namespace dearoreui::diagnostic
