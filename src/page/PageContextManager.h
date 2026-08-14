#pragma once

#include "page/IPageContextManager.h"

#include <atomic>
#include <mutex>
#include <unordered_map>

namespace dearoreui::page {

class PageContextManager : public IPageContextManager {
public:
    PageContextManager() = default;

    [[nodiscard]] api::ContextId              createContext(api::PageInfo info) override;
    [[nodiscard]] bool                        destroyContext(api::ContextId id) override;
    [[nodiscard]] std::optional<PageContext>  find(api::ContextId id) const override;
    [[nodiscard]] std::vector<api::ContextId> activeContexts() const override;
    void                                      subscribe(std::weak_ptr<IPageLifecycleListener> listener) override;
    void                                      clear() override;

    [[nodiscard]] static api::PageInfo pageInfoFromUrl(std::string_view url);

private:
    [[nodiscard]] api::ContextId nextContextId();
    void                         notifyCreated(api::ContextId id, PageContext const& context);
    void                         notifyDestroyed(api::ContextId id);
    static api::PageScope        inferScope(std::string_view path);
    static std::string           normalizePath(std::string_view url);

    mutable std::mutex                                 mMutex;
    std::unordered_map<api::ContextId, PageContext>    mContexts;
    std::vector<std::weak_ptr<IPageLifecycleListener>> mListeners;
    std::atomic<std::uint64_t>                         mNextId{1};
};

} // namespace dearoreui::page
