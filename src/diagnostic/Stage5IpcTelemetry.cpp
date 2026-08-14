#include "diagnostic/Stage5IpcTelemetry.h"

#include "diagnostic/DiagnosticLogger.h"

namespace dearoreui::diagnostic {

namespace {

[[nodiscard]] std::string boolString(bool value) { return value ? "true" : "false"; }

} // namespace

void recordStage5BridgeProbed(
    api::ContextId   id,
    bool             executeScriptFound,
    bool             jsToNativeCallbackFound,
    std::string_view summary
) {
    globalLogger()
        .info("stage5", "bridge_probe")
        .withContext(id)
        .withField("execute_script_found", boolString(executeScriptFound))
        .withField("js_to_native_callback_found", boolString(jsToNativeCallbackFound))
        .withField("summary", std::string{summary})
        .emit();
}

void recordStage5BridgeState(
    api::ContextId   id,
    bool             available,
    std::string_view bridgeType
) {
    globalLogger()
        .info("stage5", "bridge_state")
        .withContext(id)
        .withField("available", boolString(available))
        .withField("bridge_type", std::string{bridgeType})
        .emit();
}

void recordStage5HostCall(
    api::ContextId   id,
    api::RequestId   requestId,
    std::string_view method,
    std::string_view payload
) {
    globalLogger()
        .info("stage5", "host_call")
        .withContext(id)
        .withField("request_id", std::to_string(requestId.value()))
        .withField("method", std::string{method})
        .withField("payload_length", std::to_string(payload.size()))
        .emit();
}

void recordStage5HostResponse(
    api::ContextId   id,
    api::RequestId   requestId,
    std::string_view method,
    bool             hasError
) {
    globalLogger()
        .info("stage5", "host_response")
        .withContext(id)
        .withField("request_id", std::to_string(requestId.value()))
        .withField("method", std::string{method})
        .withField("has_error", boolString(hasError))
        .emit();
}

void recordStage5HostError(
    api::ContextId   id,
    api::RequestId   requestId,
    std::string_view method,
    api::ErrorCode   code,
    std::string_view message
) {
    globalLogger()
        .warning("stage5", "host_error")
        .withContext(id)
        .withError(code)
        .withField("request_id", std::to_string(requestId.value()))
        .withField("method", std::string{method})
        .withMessage(std::string{message})
        .emit();
}

void recordStage5HostCancelled(
    api::ContextId   id,
    api::RequestId   requestId,
    std::string_view method
) {
    globalLogger()
        .info("stage5", "host_cancelled")
        .withContext(id)
        .withField("request_id", std::to_string(requestId.value()))
        .withField("method", std::string{method})
        .emit();
}

void recordStage5HostInvalidContext(
    api::ContextId   id,
    api::RequestId   requestId,
    std::string_view method
) {
    globalLogger()
        .info("stage5", "host_invalid_context")
        .withContext(id)
        .withError(api::ErrorCode::InvalidContext)
        .withField("request_id", std::to_string(requestId.value()))
        .withField("method", std::string{method})
        .emit();
}

} // namespace dearoreui::diagnostic
