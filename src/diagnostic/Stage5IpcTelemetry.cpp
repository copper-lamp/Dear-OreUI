#include "diagnostic/Stage5IpcTelemetry.h"

#include "diagnostic/DiagnosticLogger.h"

#include <algorithm>

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

void recordStage5ViewInitialized(std::uintptr_t viewPtr, bool executeScriptFound) {
    globalLogger()
        .info("stage5", "view_initialized")
        .withField("view_ptr", std::to_string(viewPtr))
        .withField("execute_script_found", boolString(executeScriptFound))
        .emit();
}

void recordStage5ScriptSubmitted(api::ContextId id, std::size_t scriptLength) {
    globalLogger()
        .info("stage5", "script_submitted")
        .withContext(id)
        .withField("script_length", std::to_string(scriptLength))
        .emit();
}

void recordStage5ScriptDeferred(api::ContextId id, std::size_t queueLength) {
    globalLogger()
        .info("stage5", "script_deferred")
        .withContext(id)
        .withField("queue_length", std::to_string(queueLength))
        .emit();
}

void recordStage5ScriptFailed(api::ContextId id, std::string_view message) {
    globalLogger()
        .warning("stage5", "script_failed")
        .withContext(id)
        .withMessage(std::string{message})
        .emit();
}

void recordStage5ScriptPreview(api::ContextId id, std::string_view phase, std::string_view script) {
    auto const previewLength = std::min<std::size_t>(script.size(), 200);
    globalLogger()
        .info("stage5", "script_preview")
        .withContext(id)
        .withField("phase", std::string{phase})
        .withField("script_length", std::to_string(script.size()))
        .withField("preview", std::string{script.substr(0, previewLength)})
        .emit();
}

void recordStage5FacetRegistered(std::string_view facetName, std::string_view reason, std::uintptr_t registryPtr) {
    globalLogger()
        .info("facet", "registered")
        .withField("facet_name", std::string{facetName})
        .withField("reason", std::string{reason})
        .withField("registry_ptr", std::to_string(registryPtr))
        .emit();
}

void recordStage5FacetActivated(std::string_view facetName, bool hasParams, std::size_t payloadSize) {
    globalLogger()
        .info("facet", "activated")
        .withField("facet_name", std::string{facetName})
        .withField("has_params", boolString(hasParams))
        .withField("payload_size", std::to_string(payloadSize))
        .emit();
}

void recordStage5FacetPayload(api::RequestId requestId, api::ContextId contextId, std::string_view method) {
    globalLogger()
        .info("facet", "payload")
        .withContext(contextId)
        .withField("request_id", std::to_string(requestId.value()))
        .withField("method", std::string{method})
        .emit();
}

void recordStage5FacetResponse(api::RequestId requestId, std::size_t responseSize) {
    globalLogger()
        .info("facet", "response")
        .withField("request_id", std::to_string(requestId.value()))
        .withField("response_size", std::to_string(responseSize))
        .emit();
}

void recordStage5FacetError(api::RequestId requestId, api::ErrorCode code, std::string_view message) {
    globalLogger()
        .warning("facet", "error")
        .withError(code)
        .withField("request_id", std::to_string(requestId.value()))
        .withMessage(std::string{message})
        .emit();
}

} // namespace dearoreui::diagnostic
