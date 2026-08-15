#pragma once

#include "api/types/Error.h"
#include "api/types/Id.h"
#include "api/types/Result.h"
#include "ipc/IpcMessage.h"

#include <chrono>
#include <string_view>

namespace dearoreui::ipc {

class HostDispatcher;

class IHostBridge {
public:
    virtual ~IHostBridge() = default;

    [[nodiscard]] virtual bool isAvailable() const = 0;

    [[nodiscard]] virtual api::Result<void> sendScript(api::ContextId id, std::string_view script) = 0;

    [[nodiscard]] virtual api::Result<IpcMessage> callHost(
        api::RequestId            requestId,
        api::ContextId            contextId,
        std::string               method,
        std::string               payload,
        std::chrono::milliseconds timeout
    ) = 0;

    // Wired by the runtime after the host dispatcher is created. The native
    // JS->C++ event handler needs it to route engine.trigger payloads.
    virtual void setHostDispatcher(HostDispatcher& dispatcher) = 0;

    virtual void cancel(api::RequestId requestId)     = 0;
    virtual void invalidateContext(api::ContextId id) = 0;
};

} // namespace dearoreui::ipc
