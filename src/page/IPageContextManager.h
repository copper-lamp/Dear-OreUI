#pragma once

#include "api/types/Id.h"
#include "api/types/Page.h"
#include "page/IPageLifecycleListener.h"
#include "page/PageContext.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

namespace dearoreui::page {

class IPageContextManager {
public:
    virtual ~IPageContextManager() = default;

    [[nodiscard]] virtual api::ContextId              createContext(api::PageInfo info)                         = 0;
    [[nodiscard]] virtual bool                        destroyContext(api::ContextId id)                         = 0;
    [[nodiscard]] virtual std::optional<PageContext>  find(api::ContextId id) const                             = 0;
    [[nodiscard]] virtual std::vector<api::ContextId> activeContexts() const                                    = 0;
    virtual void                                      subscribe(std::weak_ptr<IPageLifecycleListener> listener) = 0;
    virtual void                                      clear()                                                   = 0;
};

} // namespace dearoreui::page
