#pragma once

#include "api/manifest/Permission.h"
#include "api/types/Id.h"
#include "api/types/Page.h"
#include "api/types/Result.h"

#include <cstddef>
#include <string>
#include <vector>

namespace dearoreui::api {

struct TransformRequest {
    ModId owner;
    ContextId context;
    PageScope scope{PageScope::Any};
    std::string targetFingerprint;
    bool requireUniqueMatch{true};
};

struct TransformOperationInfo {
    RegistrationHandle handle;
    ModId owner;
    std::string path;
    std::string fingerprint;
    bool applicable{false};
    std::string reason;
};

struct TransformReport {
    ContextId context;
    PageScope scope{PageScope::Any};
    bool preview{true};
    bool success{false};
    std::size_t applicable{0};
    std::size_t blocked{0};
    std::vector<TransformOperationInfo> operations;
    std::vector<Error> errors;
};

} // namespace dearoreui::api
