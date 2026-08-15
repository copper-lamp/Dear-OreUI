#include "ipc/JsPayloadHandler.h"

#include "diagnostic/Stage7UiTelemetry.h"

#include <map>

namespace dearoreui::ipc {

namespace {

[[nodiscard]] std::string serializeErrorResponse(
    api::RequestId   id,
    api::ContextId   contextId,
    api::ErrorCode   code,
    std::string_view message
) {
    IpcMessage response;
    response.type      = IpcMessageType::Response;
    response.id        = id;
    response.contextId = contextId;
    response.error     = code;
    response.payload   = std::string{message};
    return serializeIpcMessage(response);
}

} // namespace

std::string handleJsPayload(
    HostDispatcher&           dispatcher,
    std::string_view          payload,
    std::chrono::milliseconds timeout
) {
    // Heuristic: payloads that look like a protocol message (start with '{')
    // are parsed strictly; anything else is the Stage 7.1 plain diagnostics
    // report ("runtime_executed:...", "dbg:...") and fire-and-forgets.
    bool const looksLikeProtocol = !payload.empty() && payload.front() == '{';

    auto parsed = parseIpcMessage(payload);
    if (parsed.isErr()) {
        if (!looksLikeProtocol) {
            diagnostic::recordStage7JsReport(payload);
            return {};
        }
        return serializeErrorResponse(
            api::RequestId{},
            api::ContextId{},
            api::ErrorCode::InvalidFormat,
            "invalid ipc message: " + parsed.error().message
        );
    }

    auto const& request = parsed.value();
    if (request.type != IpcMessageType::Request) {
        return serializeErrorResponse(
            request.id,
            request.contextId,
            api::ErrorCode::InvalidArgument,
            "js payload must be an ipc request"
        );
    }

    auto result = dispatcher.dispatch(request, timeout);
    if (result.isOk()) {
        return serializeIpcMessage(result.value());
    }
    return serializeErrorResponse(
        request.id,
        request.contextId,
        result.error().code,
        result.error().message
    );
}

} // namespace dearoreui::ipc
