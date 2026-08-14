#include "page/PageContextManager.h"

#include <algorithm>

namespace dearoreui::page {

api::ContextId PageContextManager::createContext(api::PageInfo info) {
    auto        contextId = nextContextId();
    PageContext context;
    context.id        = contextId;
    context.page      = std::move(info);
    context.createdAt = std::chrono::system_clock::now();

    {
        std::lock_guard lock{mMutex};
        mContexts.emplace(contextId, std::move(context));
    }

    if (auto found = find(contextId); found) {
        notifyCreated(contextId, *found);
    }
    return contextId;
}

bool PageContextManager::destroyContext(api::ContextId id) {
    {
        std::lock_guard lock{mMutex};
        if (mContexts.erase(id) == 0) {
            return false;
        }
    }
    notifyDestroyed(id);
    return true;
}

std::optional<PageContext> PageContextManager::find(api::ContextId id) const {
    std::lock_guard lock{mMutex};
    auto            iterator = mContexts.find(id);
    if (iterator == mContexts.end()) {
        return std::nullopt;
    }
    return iterator->second;
}

std::vector<api::ContextId> PageContextManager::activeContexts() const {
    std::lock_guard             lock{mMutex};
    std::vector<api::ContextId> result;
    result.reserve(mContexts.size());
    for (auto const& [id, context] : mContexts) {
        static_cast<void>(context);
        result.push_back(id);
    }
    return result;
}

void PageContextManager::subscribe(std::weak_ptr<IPageLifecycleListener> listener) {
    std::lock_guard lock{mMutex};
    mListeners.push_back(std::move(listener));
}

void PageContextManager::clear() {
    std::vector<api::ContextId> ids;
    {
        std::lock_guard lock{mMutex};
        ids.reserve(mContexts.size());
        for (auto const& [id, context] : mContexts) {
            static_cast<void>(context);
            ids.push_back(id);
        }
        mContexts.clear();
    }
    for (auto id : ids) {
        notifyDestroyed(id);
    }
}

api::PageInfo PageContextManager::pageInfoFromUrl(std::string_view url) {
    auto          path = normalizePath(url);
    api::PageInfo info;
    info.id    = api::PageId{std::string(path)};
    info.scope = inferScope(path);
    return info;
}

api::ContextId PageContextManager::nextContextId() {
    return api::ContextId{mNextId.fetch_add(1, std::memory_order_relaxed)};
}

void PageContextManager::notifyCreated(api::ContextId id, PageContext const& context) {
    std::lock_guard lock{mMutex};
    for (auto& weak : mListeners) {
        if (auto listener = weak.lock()) {
            listener->onPageCreated(id, context);
        }
    }
}

void PageContextManager::notifyDestroyed(api::ContextId id) {
    std::lock_guard lock{mMutex};
    for (auto& weak : mListeners) {
        if (auto listener = weak.lock()) {
            listener->onPageDestroyed(id);
        }
    }
}

api::PageScope PageContextManager::inferScope(std::string_view path) {
    if (path == "/hbui/index.html") return api::PageScope::MainMenu;
    if (path.starts_with("/play/")) return api::PageScope::PlayScreen;
    if (path.starts_with("/settings/")) return api::PageScope::Settings;
    if (path.starts_with("/pause/")) return api::PageScope::Pause;
    if (path == "/gameplay.html" || path.starts_with("/game/")) return api::PageScope::InGame;
    return api::PageScope::Custom;
}

std::string PageContextManager::normalizePath(std::string_view url) {
    if (url.empty()) return "/";

    auto end  = url.find_first_of("?#");
    auto path = url.substr(0, end);
    if (path.empty()) return "/";
    return std::string(path);
}

} // namespace dearoreui::page
