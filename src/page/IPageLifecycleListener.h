#pragma once

#include "api/types/Id.h"
#include "page/PageContext.h"

namespace dearoreui::page {

class IPageLifecycleListener {
public:
    virtual ~IPageLifecycleListener() = default;

    virtual void onPageCreated(api::ContextId id, PageContext const& context) = 0;
    virtual void onPageDestroyed(api::ContextId id)                           = 0;
};

} // namespace dearoreui::page
