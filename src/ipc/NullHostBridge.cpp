#include "ipc/NullHostBridge.h"

#include "api/types/Error.h"

namespace dearoreui::ipc {

bool NullHostBridge::isAvailable() const { return false; }

api::Result<void> NullHostBridge::sendScript(api::ContextId /*id*/, std::string_view /*script*/) {
    return api::Result<void>::success();
}

api::Result<IpcMessage> NullHostBridge::callHost(
    api::RequestId /*requestId*/,
    api::ContextId /*contextId*/,
    std::string /*method*/,
    std::string /*payload*/,
    std::chrono::milliseconds /*timeout*/
) {
    return api::Error{api::ErrorCode::NotSupported, "host bridge is not available"};
}

void NullHostBridge::cancel(api::RequestId /*requestId*/) {}

void NullHostBridge::invalidateContext(api::ContextId /*id*/) {}

} // namespace dearoreui::ipc
