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

// Stage 8-A: native OreUI facet JS->C++ channel telemetry.
// reason: "ok" | "registry_null" | "already_registered"
void recordStage5FacetRegistered(std::string_view facetName, std::string_view reason, std::uintptr_t registryPtr);

void recordStage5FacetActivated(std::string_view facetName, bool hasParams, std::size_t payloadSize);

void recordStage5FacetPayload(api::RequestId requestId, api::ContextId contextId, std::string_view method);

void recordStage5FacetResponse(api::RequestId requestId, std::size_t responseSize);

void recordStage5FacetError(api::RequestId requestId, api::ErrorCode code, std::string_view message);

} // namespace dearoreui::diagnostic
