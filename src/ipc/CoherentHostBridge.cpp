#include "ipc/CoherentHostBridge.h"

#include "api/types/Error.h"
#include "diagnostic/Stage0TelemetryCompat.h"
#include "diagnostic/Stage5IpcTelemetry.h"
#include "diagnostic/Stage7UiTelemetry.h"
#include "ipc/JsPayloadHandler.h"

#include "mc/external/gameface/cohtml/ArgumentsBinder.h"
#include "mc/external/gameface/cohtml/Binder.h"
#include "mc/external/gameface/cohtml/IEventHandler.h"
#include "mc/external/gameface/cohtml/View.h"

#include <algorithm>
#include <windows.h>

namespace cohtml {
// mcmeta declares cohtml::IEventHandler::~IEventHandler() but the game does
// not export a usable definition; provide our own so the handler can be
// destroyed in-process. The engine only calls Invoke through the vtable and
// never destroys our handler.
IEventHandler::~IEventHandler() = default;
} // namespace cohtml

namespace dearoreui::ipc {

namespace {

// SEH-safe extraction of the first BindCall argument (a JSON string). This
// function contains no C++ objects with destructors so MSVC permits __try.
// On engine-layout mismatch or a missing argument it returns false and the
// caller falls back to a void result — the client must never crash here.
bool readJsPayload(::cohtml::ArgumentsBinder* binder, char const*& text, std::uint64_t& length) {
    text   = nullptr;
    length = 0;
    __try {
        if (binder == nullptr) {
            return false;
        }
        auto* argument = binder->GetArgument(0);
        if (argument == nullptr) {
            return false;
        }
        argument->ReadAsString(text, length);
        return text != nullptr && length > 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// SEH-safe write of the response JSON back into the binder.
void writeJsResult(::cohtml::ArgumentsBinder* binder, char const* text, bool hasText) {
    __try {
        if (binder == nullptr) {
            return;
        }
        if (hasText && text != nullptr) {
            binder->ResultBegin();
            auto* argument = binder->GetArgument(0);
            if (argument != nullptr) {
                argument->Bind(text);
            }
            binder->ResultEnd();
        } else {
            binder->ResultVoid();
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

// Stage 8: cohtml::IEventHandler implementation that receives JS-side
// engine.call("dearoreui_report", json) invocations. The engine drives this
// through ArgumentsBinder; we extract argument 0 (a JSON string), forward it
// to HostDispatcher via JsPayloadHandler, and write the response JSON back.
// The engine calls (read/write) are isolated in the SEH-safe helpers above so
// a mismatch between the mcmeta header layout and the real engine degrades to
// a diagnostic + null bridge instead of crashing the client (BindCall
// previously corrupted the heap at crash 11:38).
class CoherentJsHandler final : public ::cohtml::IEventHandler {
public:
    explicit CoherentJsHandler(HostDispatcher& dispatcher) : mDispatcher(dispatcher) {}

    void Invoke(::cohtml::ArgumentsBinder* binder) override {
        char const*   text   = nullptr;
        std::uint64_t length = 0;
        if (!readJsPayload(binder, text, length)) {
            writeJsResult(binder, nullptr, false);
            return;
        }

        std::string payload(text, length);
        std::string response = handleJsPayload(mDispatcher, payload, std::chrono::milliseconds{5000});
        if (response.empty()) {
            writeJsResult(binder, nullptr, false);
        } else {
            writeJsResult(binder, response.c_str(), true);
        }
    }

    void const* GetTarget() override { return nullptr; }
    void        SetTarget(void*) override {}

private:
    HostDispatcher& mDispatcher;
};

// Raw SEH-wrapped BindCall invocation. No C++ automatic objects here, so
// __try is permitted (C2712 forbids object unwinding next to __try). The
// handler is passed as an opaque pointer and re-cast inside.
void rawBindCall(void* view, void* handler, void*& bindingOut) {
    bindingOut = nullptr;
    __try {
        bindingOut = static_cast<cohtml::View*>(view)->BindCall(
            "dearoreui_report",
            static_cast<CoherentJsHandler*>(handler)
        );
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        bindingOut = nullptr;
    }
}

// Registration of the "dearoreui_report" native binding. On engine-layout
// mismatch the binding is skipped so the JS->C++ channel degrades to
// diagnostics instead of crashing the client (BindCall previously corrupted
// the heap at crash 11:38).
bool bindNativeCall(void* view, HostDispatcher& dispatcher, void*& handlerOut, void*& bindingOut) {
    auto* handler = new CoherentJsHandler(dispatcher);
    void* binding = nullptr;
    rawBindCall(view, handler, binding);
    if (binding == nullptr) {
        delete handler;
        return false;
    }
    handlerOut = handler;
    bindingOut = binding;
    return true;
}

} // namespace

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
    mRegistry.setOnScriptContextReady(
        [this]() { onScriptContextReady(); }
    );
}

CoherentHostBridge::~CoherentHostBridge() { releaseNativeBinding(); }

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

void CoherentHostBridge::setDispatcher(HostDispatcher& dispatcher) {
    mDispatcher = &dispatcher;
    // Re-register the native binding if a view is already live and its script
    // context is ready (e.g. dispatcher wired up after the page loaded).
    if (isAvailable() && mRegistry.isScriptContextReady()) {
        onScriptContextReady();
    }
}

void CoherentHostBridge::onViewRegistered(void* gamefaceView) {
    static_cast<void>(gamefaceView);
    // The view handle is tracked by the registry. Scripts are NOT flushed
    // here: the page's script context may not exist yet (ExecuteScript would
    // be silently dropped). Flushing happens in onScriptContextReady().
    // The native binding is also bound only once the context is ready.
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

    // Stage 8: register the JS->C++ native binding (once, per view lifetime).
    // BindCall is only legal after the script context exists — the same gate
    // ExecuteScript uses. The binding routes engine.call("dearoreui_report",
    // json) to HostDispatcher via CoherentJsHandler.
    if (mDispatcher != nullptr && mNativeHandler == nullptr) {
        void* handler = nullptr;
        void* binding = nullptr;
        if (bindNativeCall(view, *mDispatcher, handler, binding)) {
            mNativeHandler = handler;
            mNativeBinding = binding;
            diagnostic::recordStage5BridgeState(
                api::ContextId{},
                true,
                "CoherentHostBridge + BindCall(dearoreui_report)"
            );
            diagnostic::recordStage0("js", "event=bindcall_registered\tname=dearoreui_report");
        } else {
            diagnostic::recordStage5HostError(
                api::ContextId{},
                api::RequestId{},
                "dearoreui_report",
                api::ErrorCode::NotSupported,
                "cohtml BindCall registration failed; JS->C++ channel unavailable"
            );
        }
    }
}

void CoherentHostBridge::releaseNativeBinding() {
    if (mNativeHandler == nullptr) {
        return;
    }
    delete static_cast<CoherentJsHandler*>(mNativeHandler);
    mNativeHandler = nullptr;
    mNativeBinding = nullptr;
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
