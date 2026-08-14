#pragma once

#include "capability/ICapabilityQuery.h"
#include "diagnostic/DiagnosticLogger.h"
#include "ipc/IHostBridge.h"

namespace dearoreui::ipc {

// Stage 5 stub for the real Coherent JS execution bridge.
// It is gated by Capability::HostBridge: while the capability is not
// Experimental/Supported the bridge reports itself as unavailable and
// degrades to no-op behavior, preserving stage 4 page loading.
class CoherentHostBridge : public IHostBridge {
public:
    CoherentHostBridge(
        capability::ICapabilityQuery& capabilities,
        diagnostic::DiagnosticLogger& logger
    );

    [[nodiscard]] bool isAvailable() const override;

    [[nodiscard]] api::Result<void>
    sendScript(api::ContextId id, std::string_view script) override;

    [[nodiscard]] api::Result<IpcMessage> callHost(
        api::RequestId            requestId,
        api::ContextId            contextId,
        std::string               method,
        std::string               payload,
        std::chrono::milliseconds timeout
    ) override;

    void cancel(api::RequestId requestId) override;
    void invalidateContext(api::ContextId id) override;

private:
    capability::ICapabilityQuery& mCapabilities;
    diagnostic::DiagnosticLogger& mLogger;
};

} // namespace dearoreui::ipc
