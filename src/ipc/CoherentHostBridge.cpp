#include "ipc/CoherentHostBridge.h"

#include "api/types/Error.h"
#include "diagnostic/Stage0TelemetryCompat.h"
#include "diagnostic/Stage5IpcTelemetry.h"

#include "mc/external/gameface/cohtml/View.h"

#include <algorithm>
#include <string>
#include <vector>

namespace dearoreui::ipc {

void defaultCoherentExecutor(void* gamefaceView, std::string const& script) {
    auto* view = static_cast<cohtml::View*>(gamefaceView);
    view->ExecuteScript(script.c_str());
}

CoherentHostBridge::CoherentHostBridge(CoherentViewRegistry& registry, ScriptExecutor executor)
: mRegistry(registry), mExecutor(executor) {
    // The page signals script-context-ready via OnReadyForBindings; only then
    // is ExecuteScript accepted (it is silently dropped before that gate).
    mRegistry.setOnScriptContextReady([this]() { onScriptContextReady(); });
}

CoherentHostBridge::~CoherentHostBridge() = default;

bool CoherentHostBridge::isAvailable() const { return mRegistry.hasActiveView(); }

api::Result<void> CoherentHostBridge::sendScript(api::ContextId id, std::string_view script) {
    void* view = mRegistry.activeView();
    if (view == nullptr || !mRegistry.isScriptContextReady()) {
        // No live view yet (e.g. PageCreated fired before view initialization)
        // or the page's script context is not ready yet: defer into a bounded
        // queue. It is flushed when the engine signals OnReadyForBindings —
        // ExecuteScript is silently dropped if called before the context exists.
        std::lock_guard lock{mQueueMutex};
        if (mPending.size() >= kMaxPending) {
            mPending.erase(mPending.begin());
        }
        mPending.push_back(PendingScript{id, std::string{script}});
        diagnostic::recordStage5ScriptDeferred(id, mPending.size());
        diagnostic::recordStage5ScriptPreview(id, "defer", script);
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

    // Stage 8: JS->C++ channel is temporarily unavailable. cohtml BindCall /
    // RegisterForEvent both crash the client on page teardown; the WebSocket
    // loopback replacement is pending (see WebSocket回环通道-执行设计).
    diagnostic::recordStage5HostError(
        contextId,
        api::RequestId{},
        method,
        api::ErrorCode::NotSupported,
        "JS->C++ channel pending WebSocket loopback (engine bindings crash the client)"
    );
    return api::Error{api::ErrorCode::NotSupported, "JS->C++ channel not yet available"};
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

void CoherentHostBridge::onScriptContextReady() {
    void* view = mRegistry.activeView();
    if (view == nullptr) {
        return;
    }
    std::vector<PendingScript> pending;
    {
        std::lock_guard lock{mQueueMutex};
        pending.swap(mPending);
    }
    for (auto const& item : pending) {
        diagnostic::recordStage5ScriptPreview(item.contextId, "flush", item.script);
        static_cast<void>(submit(view, item.contextId, item.script));
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
    diagnostic::recordStage5ScriptPreview(id, "submit", script);
    return api::Result<void>::success();
}

} // namespace dearoreui::ipc
