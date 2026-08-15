#include "ipc/CoherentViewRegistry.h"

namespace dearoreui::ipc {

void CoherentViewRegistry::registerView(void* gamefaceView) {
    std::function<void(void*)> observer;
    {
        std::lock_guard lock{mMutex};
        if (gamefaceView != nullptr) {
            mActiveView = gamefaceView;
            // A (possibly reused) view starts with a fresh page context; the
            // ready flag is only re-raised by markScriptContextReady().
            mScriptContextReady = false;
        }
        observer = mOnViewRegistered;
    }
    if (observer && gamefaceView != nullptr) {
        observer(gamefaceView);
    }
}

void CoherentViewRegistry::markScriptContextReady() {
    std::function<void()> observer;
    {
        std::lock_guard lock{mMutex};
        mScriptContextReady = true;
        observer            = mOnScriptContextReady;
    }
    if (observer) {
        observer();
    }
}

void CoherentViewRegistry::onViewDestroyed(void* gamefaceView) {
    std::function<void(void*)> observer;
    {
        std::lock_guard lock{mMutex};
        if (mActiveView != nullptr && (gamefaceView == nullptr || gamefaceView == mActiveView)) {
            mActiveView         = nullptr;
            mScriptContextReady = false;
        }
        observer = mOnViewDestroyed;
    }
    // Notify the bridge (if any) so it can unbind native handlers before the
    // engine tears the view down.
    if (observer) {
        observer(gamefaceView);
    }
}

void* CoherentViewRegistry::activeView() const {
    std::lock_guard lock{mMutex};
    return mActiveView;
}

bool CoherentViewRegistry::isScriptContextReady() const {
    std::lock_guard lock{mMutex};
    return mScriptContextReady;
}

bool CoherentViewRegistry::hasActiveView() const {
    std::lock_guard lock{mMutex};
    return mActiveView != nullptr;
}

std::size_t CoherentViewRegistry::count() const {
    std::lock_guard lock{mMutex};
    return mActiveView != nullptr ? 1 : 0;
}

void CoherentViewRegistry::clear() {
    std::function<void(void*)> observer;
    {
        std::lock_guard lock{mMutex};
        mActiveView         = nullptr;
        mScriptContextReady = false;
        observer            = mOnViewDestroyed;
    }
    // Runtime::disable path: release the native handler too.
    if (observer) {
        observer(nullptr);
    }
}

void CoherentViewRegistry::setOnViewRegistered(std::function<void(void*)> observer) {
    std::lock_guard lock{mMutex};
    mOnViewRegistered = std::move(observer);
}

void CoherentViewRegistry::setOnScriptContextReady(std::function<void()> observer) {
    std::lock_guard lock{mMutex};
    mOnScriptContextReady = std::move(observer);
}

void CoherentViewRegistry::setOnViewDestroyed(std::function<void(void*)> observer) {
    std::lock_guard lock{mMutex};
    mOnViewDestroyed = std::move(observer);
}

} // namespace dearoreui::ipc
