#pragma once

#include "api/types/Error.h"
#include "api/types/Id.h"
#include "api/types/Page.h"
#include "api/types/Transform.h"

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace dearoreui::api {

struct InjectionReportView {
    ContextId context;
    bool success{false};
    bool bridgeAvailable{false};
    std::size_t scriptCount{0};
    std::size_t styleCount{0};
    std::size_t uiCount{0};
    std::vector<ErrorCode> errors;
};

struct HostCallReportView {
    ContextId context;
    RequestId request;
    std::string method;
    std::chrono::milliseconds elapsed{0};
    std::size_t requestBytes{0};
    std::size_t responseBytes{0};
    ErrorCode result{ErrorCode::None};
};

struct RuntimeReportQuery {
    ModId requester;
    ContextId context;
};

struct RuntimeReports {
    std::optional<InjectionReportView> injection;
    std::vector<HostCallReportView> hostCalls;
    std::optional<TransformReport> transform;
};

} // namespace dearoreui::api
