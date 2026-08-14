#include "ipc/CoherentHostBridge.h"

#include "api/types/Capability.h"
#include "api/types/Error.h"
#include "diagnostic/Stage5IpcTelemetry.h"

namespace dearoreui::ipc {

CoherentHostBridge::CoherentHostBridge(
    capability::ICapabilityQuery& capabilities,
    diagnostic::DiagnosticLogger& logger
)
: mCapabilities(capabilities), mLogger(logger) {}

bool CoherentHostBridge::isAvailable() const {
    auto level = mCapabilities.query(api::Capability::HostBridge);
    return level == api::SupportLevel::Experimental || level == api::SupportLevel::Supported;
}

api::Result<void>
CoherentHostBridge::sendScript(api::ContextId id, std::string_view /*script*/) {
    if (!isAvailable()) {
        return api::Result<void>::success();
    }

    diagnostic::recordStage5BridgeState(id, true, "CoherentHostBridge");
    // Real script submission via Coherent View::ExecuteScript is implemented
    // after the entry point is verified in telemetry.
    return api::Result<void>::success();
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
    static_cast<void>(id);
}

} // namespace dearoreui::ipc
