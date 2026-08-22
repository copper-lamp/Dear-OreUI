#pragma once

#include "api/types/DiagnosticQuery.h"
#include "api/types/Result.h"

namespace dearoreui::api {

class IDiagnosticApi {
public:
    virtual ~IDiagnosticApi() = default;
    [[nodiscard]] virtual Result<DiagnosticList> queryDiagnostics(DiagnosticQuery const& query) const = 0;
};

} // namespace dearoreui::api
