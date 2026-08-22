#pragma once

#include "api/types/Id.h"
#include "api/types/Page.h"
#include "api/types/Result.h"

#include <functional>

namespace dearoreui::api {

enum class PageEvent {
    Created,
    Ready,
    Destroyed,
};

struct PageContextView {
    ContextId id;
    PageInfo  page;
};

using PageCallback = std::function<void(PageContextView const&)>;

class IPageApi {
public:
    virtual ~IPageApi() = default;

    [[nodiscard]] virtual Result<SubscriptionHandle> subscribePage(
        PageEvent event,
        PageCallback callback
    ) = 0;

    [[nodiscard]] virtual Result<void> unsubscribePage(SubscriptionHandle handle) = 0;

    [[nodiscard]] virtual Result<PageContextView> getPageContext(ContextId id) const = 0;
};

} // namespace dearoreui::api
