#include "ipc/CoherentHostBridge.h"

#include "api/types/Error.h"
#include "diagnostic/DiagnosticLogger.h"
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
: mRegistry(registry),
  mExecutor(executor) {
    // The page signals script-context-ready via OnReadyForBindings; only then
    // is ExecuteScript accepted (it is silently dropped before that gate).
    mRegistry.setOnScriptContextReady([this]() { onScriptContextReady(); });
    // Stage 8 finding: RegisterForEvent with a mod-allocated handler crashes
    // the client at ANY registration timing, so onViewRegistered stays wired
    // but does NOT register an engine handler (see onViewRegistered comment).
    mRegistry.setOnViewRegistered([this](void* view) { onViewRegistered(view); });
}

CoherentHostBridge::~CoherentHostBridge() = default;

void CoherentHostBridge::setHostDispatcher(HostDispatcher& dispatcher) { mDispatcher = &dispatcher; }

void CoherentHostBridge::onViewRegistered(void* gamefaceView) {
    // Stage 8 finding: RegisterForEvent with a mod-allocated handler crashes
    // the client at ANY registration timing. Both experiments crashed on
    // entering /play/all:
    //   * registered at OnReadyForBindings (historical B1c/G/H1): crash
    //   * registered early at view_initialize (this stage): crash
    // The exception is raised deep inside msxml6 (0x40080201, XSD parse) - the
    // game's own unhandled-error bug, triggered by the engine binding-state
    // change. The vanilla game registers facet:request etc. from engine/init
    // modules during engine initialization, a path mods cannot replicate.
    // => The RegisterForEvent/BindCall binding channel is unusable for mods.
    //    The bridge stays C++->JS only (ExecuteScript + defer queue).
    diagnostic::globalLogger()
        .info("js", "handler_register_skipped")
        .withField("view", std::to_string(reinterpret_cast<std::uintptr_t>(gamefaceView)))
        .emit();
}

bool CoherentHostBridge::isAvailable() const { return mRegistry.hasActiveView(); }

api::Result<void> CoherentHostBridge::sendScript(api::ContextId id, std::string_view script) {
    void* view = mRegistry.activeView();
    if (view == nullptr || !mRegistry.isScriptContextReady()) {
        // No live view yet (e.g. PageCreated fired before view initialization)
        // or the page's script context is not ready yet: defer into a bounded
        // queue. It is flushed when the engine signals OnReadyForBindings -
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
    std::string /*payload*/,
    std::chrono::milliseconds /*timeout*/
) {
    if (!isAvailable()) {
        return api::Error{api::ErrorCode::NotSupported, "host bridge is not available"};
    }

    // Stage 8: C++-side callHost is intentionally unused. The JS->C++ channel
    // via engine bindings (RegisterForEvent/BindCall) crashes the client at
    // any registration timing, and the cohtml build has no WebSocket/XHR/fetch.
    // This client exposes NO usable JS->C++ channel for mods; the bridge stays
    // C++->JS only (ExecuteScript + defer queue).
    diagnostic::recordStage5HostError(
        contextId,
        api::RequestId{},
        method,
        api::ErrorCode::NotSupported,
        "no JS->C++ channel available on this client (engine bindings crash; no network APIs)"
    );
    return api::Error{api::ErrorCode::NotSupported, "no JS->C++ channel available on this client"};
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
