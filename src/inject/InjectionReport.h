#pragma once

#include "api/types/Error.h"
#include "api/types/Id.h"

#include <string>
#include <vector>

namespace dearoreui::inject {

struct InjectionReport {
    api::ContextId           contextId;
    bool                     success{false};
    bool                     hostBridgeAvailable{false};
    std::size_t              hostCalls{0};
    std::size_t              uiCount{0};
    std::vector<std::string> injectedScripts;
    std::vector<std::string> injectedStyleSheets;
    std::vector<api::Error>  errors;
};

} // namespace dearoreui::inject
