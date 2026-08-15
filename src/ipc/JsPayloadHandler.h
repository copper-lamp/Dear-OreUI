#pragma once

#include "ipc/HostDispatcher.h"
#include "ipc/IpcMessage.h"

#include <string>
#include <string_view>

namespace dearoreui::ipc {

// Cohtml-free JS->C++ payload handler (Stage 8).
//
// The native BindCall handler extracts the first JS argument (a JSON string)
// and hands it to handleJsPayload. Keeping this function cohtml-free makes the
// request/response wire protocol unit-testable without the engine headers.
//
// Wire protocol (see IpcMessage::serializeIpcMessage):
//   in:  {"type":"request","id":N,"ctx":N,"method":"...","payload":"..."}
//   out: {"type":"response","id":N,"ctx":N,"method":"","payload":"...","error":N}
//
// Payloads that are not valid IpcMessages are treated as plain diagnostics
// reports (fire-and-forget, like the Stage 7.1 dearoreui_report probe) and
// yield an empty response string.

// Executes one JS-side request against the dispatcher and returns the
// serialized response JSON. Returns an empty string when the payload is a
// non-protocol diagnostic report (or on hard parse failure the response is a
// serialized error message so the JS side always gets a definitive answer).
[[nodiscard]] std::string handleJsPayload(
    HostDispatcher&                  dispatcher,
    std::string_view                 payload,
    std::chrono::milliseconds        timeout
);

} // namespace dearoreui::ipc
