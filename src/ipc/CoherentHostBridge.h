#pragma once

#include "ipc/CoherentViewRegistry.h"
#include "ipc/IHostBridge.h"

#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

namespace dearoreui::ipc {

class HostDispatcher;

// Low-level JS submitter. The default implementation casts the opaque handle
// to cohtml::View and calls ExecuteScript; tests inject a fake.
using ScriptExecutor = void (*)(void* gamefaceView, std::string const& script);

// Real implementation: casts the handle to cohtml::View and calls
// ExecuteScript. Defined in CoherentHostBridge.cpp (needs cohtml headers).
void defaultCoherentExecutor(void* gamefaceView, std::string const& script);

// Real Coherent JS execution bridge (Stage 7.1).
//
// Availability is driven by CoherentViewRegistry: while no gameface view has
// been captured the bridge reports itself as unavailable and defers scripts
// into a bounded queue, preserving stage 4 page loading exactly like
// NullHostBridge. Once a view is registered the deferred scripts are flushed
// and subsequent sendScript calls execute immediately.
//
// Stage 8 note: the JS->C++ channel previously used cohtml BindCall /
// RegisterForEvent, but BOTH crashed the client when registered at the
// OnReadyForBindings moment. BindingProbe observation showed the game registers
// its facet handlers EARLIER (after page creation, before OnReadyForBindings)
// and never unregisters. The bridge now mimics that: the handler is registered
// as soon as a view is captured (view_initialize), not when bindings become
// ready. If the timing hypothesis is right, returning to the menu no longer
// crashes and engine.trigger("dearoreui_report", json) reaches handleJsPayload.
class CoherentHostBridge : public IHostBridge {
public:
    CoherentHostBridge(
        CoherentViewRegistry& registry,
        ScriptExecutor        executor = defaultCoherentExecutor
    );
    ~CoherentHostBridge() override;

    [[nodiscard]] bool isAvailable() const override;

    [[nodiscard]] api::Result<void> sendScript(api::ContextId id, std::string_view script) override;

    [[nodiscard]] api::Result<IpcMessage> callHost(
        api::RequestId            requestId,
        api::ContextId            contextId,
        std::string               method,
        std::string               payload,
        std::chrono::milliseconds timeout
    ) override;

    void cancel(api::RequestId requestId) override;
    void invalidateContext(api::ContextId id) override;

    // Wired by Runtime::enable() after the dispatcher is created. The native
    // JS handler needs it to route engine.trigger("dearoreui_report", json).
    void setHostDispatcher(HostDispatcher& dispatcher) override;

    // Called by CoherentViewRegistry when the page signals its script context
    // is ready (OnReadyForBindings); flushes the deferred script queue into
    // the active view via ExecuteScript.
    void onScriptContextReady();

private:
    // Called by CoherentViewRegistry when a view is first captured. Registers
    // the JS->C++ event handler EARLY (view_initialize), mimicking the game's
    // facet handler timing. Like the game, the handler is never unregistered.
    void onViewRegistered(void* gamefaceView);

    [[nodiscard]] api::Result<void> submit(void* gamefaceView, api::ContextId id, std::string const& script);

    CoherentViewRegistry& mRegistry;
    ScriptExecutor        mExecutor;
    HostDispatcher*       mDispatcher{nullptr};
    void*                 mNativeHandler{nullptr}; // CoherentJsHandler* (opaque, cohtml-free header)
    void*                 mBindingHandle{nullptr}; // handle returned by RegisterForEvent

    struct PendingScript {
        api::ContextId contextId;
        std::string    script;
    };
    std::vector<PendingScript> mPending;
    mutable std::mutex         mQueueMutex;
    static constexpr std::size_t kMaxPending = 8;
};

} // namespace dearoreui::ipc
