#pragma once

#include "api/types/Error.h"
#include "api/types/Id.h"
#include "api/types/Result.h"

#include <string>
#include <string_view>

namespace dearoreui::ipc {

enum class IpcMessageType {
    Request,
    Response,
    Cancel,
    Notify,
};

struct IpcMessage {
    IpcMessageType type{IpcMessageType::Request};
    api::RequestId id;
    api::ContextId contextId;
    std::string    method;
    std::string    payload;
    api::ErrorCode error{api::ErrorCode::None};
};

[[nodiscard]] std::string             serializeIpcMessage(IpcMessage const& message);
[[nodiscard]] api::Result<IpcMessage> parseIpcMessage(std::string_view json);

[[nodiscard]] std::string_view            ipcMessageTypeName(IpcMessageType type);
[[nodiscard]] api::Result<IpcMessageType> parseIpcMessageType(std::string_view name);

} // namespace dearoreui::ipc
