#pragma once

#include "ipc/IHostBridge.h"

namespace dearoreui::ipc {

class NullHostBridge : public IHostBridge {
public:
    [[nodiscard]] bool isAvailable() const override;

    [[nodiscard]] api::Result<void> sendScript(api::ContextId id, std::string_view script) override;

    [[nodiscard]] api::Result<IpcMessage> callHost(
        api::RequestId            requestId,
        api::ContextId            contextId,
        std::string               method,
        std::string               payload,
        std::chrono::milliseconds timeout
    ) override;

    void setHostDispatcher(HostDispatcher& dispatcher) override;

    void cancel(api::RequestId requestId) override;
    void invalidateContext(api::ContextId id) override;
};

} // namespace dearoreui::ipc
