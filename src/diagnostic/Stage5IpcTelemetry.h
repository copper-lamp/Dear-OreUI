#pragma once

#include "api/types/Error.h"
#include "api/types/Id.h"

#include <string_view>

namespace dearoreui::diagnostic {

void recordStage5BridgeProbed(
    api::ContextId id,
    bool           executeScriptFound,
    bool           jsToNativeCallbackFound,
    std::string_view summary
);

void recordStage5BridgeState(
    api::ContextId   id,
    bool             available,
    std::string_view bridgeType
);

void recordStage5HostCall(
    api::ContextId   id,
    api::RequestId   requestId,
    std::string_view method,
    std::string_view payload
);

void recordStage5HostResponse(
    api::ContextId   id,
    api::RequestId   requestId,
    std::string_view method,
    bool             hasError
);

void recordStage5HostError(
    api::ContextId   id,
    api::RequestId   requestId,
    std::string_view method,
    api::ErrorCode   code,
    std::string_view message
);

void recordStage5HostCancelled(
    api::ContextId   id,
    api::RequestId   requestId,
    std::string_view method
);

void recordStage5HostInvalidContext(
    api::ContextId   id,
    api::RequestId   requestId,
    std::string_view method
);

// Stage 7.1: real Coherent view capture and script submission events.
void recordStage5ViewInitialized(std::uintptr_t viewPtr, bool executeScriptFound);

void recordStage5ScriptSubmitted(api::ContextId id, std::size_t scriptLength);

void recordStage5ScriptDeferred(api::ContextId id, std::size_t queueLength);

void recordStage5ScriptFailed(api::ContextId id, std::string_view message);

// Script content preview for troubleshooting: phase is "submit"|"defer"|"flush".
void recordStage5ScriptPreview(api::ContextId id, std::string_view phase, std::string_view script);

} // namespace dearoreui::diagnostic
