#pragma once

#include "api/manifest/Permission.h"
#include "api/types/Page.h"
#include "api/types/Version.h"

#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

namespace dearoreui::api {

struct HostMethodManifest {
    std::string               name;
    Version                   version{1, 0, 0};
    std::string               requestSchema;
    std::string               responseSchema;
    PermissionSet             permissions;
    std::vector<PageScope>    pageScopes;
    std::chrono::milliseconds timeout{5000};
    std::size_t               maxRequestBytes{64 * 1024};
    std::size_t               maxResponseBytes{256 * 1024};
};

} // namespace dearoreui::api
