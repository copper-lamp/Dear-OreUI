#pragma once

#include "api/types/Page.h"
#include "api/types/Result.h"

#include <cstddef>
#include <string>

namespace dearoreui::api {

struct ResourceInfo {
    std::string uri;
    std::string contentType;
    std::size_t size{0};
    PageScope   scope{PageScope::Any};
};

struct ResourceReadOptions {
    ModId       requester;
    std::size_t maxBytes{1024 * 1024};
};

struct ResourceBytes {
    ResourceInfo info;
    std::string  data;
};

} // namespace dearoreui::api
