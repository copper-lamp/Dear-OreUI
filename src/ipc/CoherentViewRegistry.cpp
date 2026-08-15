#include "ipc/CoherentViewRegistry.h"

namespace dearoreui::ipc {

void CoherentViewRegistry::registerView(void* gamefaceView) {
    std::function<void(void*)> observer;
    {
        std::lock_guard lock{mMutex};
        if (gamefaceView != nullptr) {
            mActiveView = gamefaceView;
        }
        observer = mOnViewRegistered;
    }
    if (observer && gamefaceView != nullptr) {
        observer(gamefaceView);
    }
}

void* CoherentViewRegistry::activeView() const {
    std::lock_guard lock{mMutex};
    return mActiveView;
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
    std::lock_guard lock{mMutex};
    mActiveView = nullptr;
}

void CoherentViewRegistry::setOnViewRegistered(std::function<void(void*)> observer) {
    std::lock_guard lock{mMutex};
    mOnViewRegistered = std::move(observer);
}

} // namespace dearoreui::ipc
