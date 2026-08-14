#pragma once

#include "api/types/Id.h"
#include "api/types/Page.h"

#include <chrono>

namespace dearoreui::page {

struct PageContext {
    api::ContextId                        id;
    api::PageInfo                         page;
    std::chrono::system_clock::time_point createdAt;
};

} // namespace dearoreui::page
