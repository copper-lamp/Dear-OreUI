#pragma once

#include "ipc/CoherentViewRegistry.h"
#include "ipc/IHostBridge.h"

#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

namespace dearoreui::ipc {

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
class CoherentHostBridge : public IHostBridge {
public:
    CoherentHostBridge(
        CoherentViewRegistry& registry,
        ScriptExecutor        executor = defaultCoherentExecutor
    );

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

    // Called by CoherentViewRegistry when a new view is registered; the handle
    // is tracked by the registry itself, so nothing is flushed here (the page
    // script context may not exist yet).
    void onViewRegistered(void* gamefaceView);

    // Called by CoherentViewRegistry when the page signals its script context
    // is ready (OnReadyForBindings); flushes the deferred script queue into
    // the active view via ExecuteScript.
    void onScriptContextReady();

private:
    [[nodiscard]] api::Result<void> submit(void* gamefaceView, api::ContextId id, std::string const& script);

    CoherentViewRegistry&         mRegistry;
    ScriptExecutor                mExecutor;

    struct PendingScript {
        api::ContextId contextId;
        std::string    script;
    };
    std::vector<PendingScript> mPending;
    mutable std::mutex         mQueueMutex;
    static constexpr std::size_t kMaxPending = 8;
};

} // namespace dearoreui::ipc
