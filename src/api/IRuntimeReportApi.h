#pragma once

#include "api/types/Result.h"
#include "api/types/RuntimeReports.h"

namespace dearoreui::api {

class IRuntimeReportApi {
public:
    virtual ~IRuntimeReportApi() = default;
    [[nodiscard]] virtual Result<RuntimeReports> queryRuntimeReports(RuntimeReportQuery query) const = 0;
};

} // namespace dearoreui::api
