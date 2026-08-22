#pragma once

// DearOreUI external integration bridge (pure C ABI, no C++ runtime features).
//
// Use from any external LeviLamina mod to obtain the DearOreUI API instance:
//
//   #include "bridge/DearOreUIBridge.h"
//   auto result = DearOreUI_QueryApi(1); // 1 == DearOreUI protocol version
//   if (result.status == DearOreUIBridge_Ok) {
//       auto* api = static_cast<dearoreui::api::IDearOreUIApi*>(result.api);
//   }
//
// The returned api pointer is owned by DearOreUI; callers must never delete it.
// All query methods are thread-safe; mutation methods must be called on the
// game main thread.

#include <stdint.h>

#ifdef _WIN32
#ifdef DEAROREUI_BUILDING
#define DEAROREUI_API __declspec(dllexport)
#else
#define DEAROREUI_API __declspec(dllimport)
#endif
#else
#define DEAROREUI_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum DearOreUIBridgeStatus {
    DearOreUIBridge_Ok             = 0, // api is available and protocol matches
    DearOreUIBridge_ModNotLoaded   = 1, // DearOreUI is not loaded / bridge not set
    DearOreUIBridge_NotReady       = 2, // DearOreUI loaded but runtime not enabled
    DearOreUIBridge_VersionMismatch = 3 // requested protocol differs from current
} DearOreUIBridgeStatus;

typedef struct DearOreUIBridgeResult {
    uint32_t status;          // DearOreUIBridgeStatus
    uint32_t protocolVersion; // current DearOreUI protocol version
    void*    api;             // IDearOreUIApi* when status == DearOreUIBridge_Ok
} DearOreUIBridgeResult;

// Queries the DearOreUI API instance. `requestedProtocolVersion` may be 0 to
// accept any currently active protocol. The result carries the current
// protocolVersion so callers can negotiate.
DEAROREUI_API DearOreUIBridgeResult DearOreUI_QueryApi(uint32_t requestedProtocolVersion);

#ifdef __cplusplus
} // extern "C"
#endif