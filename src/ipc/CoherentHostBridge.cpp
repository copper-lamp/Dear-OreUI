#include "ipc/CoherentHostBridge.h"

#include "api/types/Error.h"
#include "diagnostic/Stage5IpcTelemetry.h"

#include "mc/external/gameface/cohtml/View.h"

#include <algorithm>

namespace dearoreui::ipc {

void defaultCoherentExecutor(void* gamefaceView, std::string const& script) {
    auto* view = static_cast<cohtml::View*>(gamefaceView);
    view->ExecuteScript(script.c_str());
}

CoherentHostBridge::CoherentHostBridge(
    CoherentViewRegistry& registry,
    ScriptExecutor        executor
)
: mRegistry(registry), mExecutor(executor) {
    mRegistry.setOnViewRegistered(
        [this](void* gamefaceView) { onViewRegistered(gamefaceView); }
    );
}

bool CoherentHostBridge::isAvailable() const { return mRegistry.hasActiveView(); }

api::Result<void> CoherentHostBridge::sendScript(api::ContextId id, std::string_view script) {
    void* view = mRegistry.activeView();
    if (view == nullptr) {
        // No live view yet (e.g. PageCreated fired before view initialization):
        // defer into a bounded queue flushed on the next registerView.
        std::lock_guard lock{mQueueMutex};
        if (mPending.size() >= kMaxPending) {
            mPending.erase(mPending.begin());
        }
        mPending.push_back(PendingScript{id, std::string{script}});
        diagnostic::recordStage5ScriptDeferred(id, mPending.size());
        return api::Result<void>::success();
    }
    return submit(view, id, std::string{script});
}

api::Result<IpcMessage> CoherentHostBridge::callHost(
    api::RequestId /*requestId*/,
    api::ContextId contextId,
    std::string    method,
    std::string    /*payload*/,
    std::chrono::milliseconds /*timeout*/
) {
    if (!isAvailable()) {
        return api::Error{api::ErrorCode::NotSupported, "host bridge is not available"};
    }

    diagnostic::recordStage5HostError(
        contextId,
        api::RequestId{},
        method,
        api::ErrorCode::NotSupported,
        "CoherentHostBridge call path is not yet implemented"
    );
    return api::Error{api::ErrorCode::NotSupported, "CoherentHostBridge call path is not yet implemented"};
}

void CoherentHostBridge::cancel(api::RequestId /*requestId*/) {}

void CoherentHostBridge::invalidateContext(api::ContextId id) {
    std::lock_guard lock{mQueueMutex};
    mPending.erase(
        std::remove_if(
            mPending.begin(),
            mPending.end(),
            [id](PendingScript const& item) { return item.contextId.value() == id.value(); }
        ),
        mPending.end()
    );
}

void CoherentHostBridge::onViewRegistered(void* gamefaceView) {
    if (gamefaceView == nullptr) {
        return;
    }
    std::vector<PendingScript> pending;
    {
        std::lock_guard lock{mQueueMutex};
        pending.swap(mPending);
    }
    for (auto const& item : pending) {
        static_cast<void>(submit(gamefaceView, item.contextId, item.script));
    }
}

api::Result<void> CoherentHostBridge::submit(void* gamefaceView, api::ContextId id, std::string const& script) {
    try {
        mExecutor(gamefaceView, script);
    } catch (...) {
        diagnostic::recordStage5ScriptFailed(id, "coherent ExecuteScript threw an exception");
        return api::Error{api::ErrorCode::InternalError, "coherent ExecuteScript threw an exception"};
    }
    diagnostic::recordStage5ScriptSubmitted(id, script.size());
    return api::Result<void>::success();
}

} // namespace dearoreui::ipc
