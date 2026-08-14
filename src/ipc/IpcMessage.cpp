#include "ipc/IpcMessage.h"

#include "api/manifest/JsonManifestParser.h"

#include <map>

namespace dearoreui::ipc {

namespace {

std::string_view typeName(IpcMessageType type) {
    switch (type) {
    case IpcMessageType::Request:
        return "request";
    case IpcMessageType::Response:
        return "response";
    case IpcMessageType::Cancel:
        return "cancel";
    case IpcMessageType::Notify:
        return "notify";
    }
    return "unknown";
}

api::Result<std::uint64_t> readUint64(api::JsonValue const& value, std::string_view field) {
    auto const* node = value.find(field);
    if (node == nullptr || !node->isNumber()) {
        return api::Error{api::ErrorCode::InvalidFormat, std::string{"missing or invalid "} + std::string{field}};
    }
    return static_cast<std::uint64_t>(node->asNumber());
}

api::Result<std::string> readString(api::JsonValue const& value, std::string_view field, bool required) {
    auto const* node = value.find(field);
    if (node == nullptr) {
        if (required) {
            return api::Error{api::ErrorCode::InvalidFormat, std::string{"missing "} + std::string{field}};
        }
        return std::string{};
    }
    if (!node->isString()) {
        return api::Error{api::ErrorCode::InvalidFormat, std::string{"invalid "} + std::string{field}};
    }
    return node->asString();
}

} // namespace

std::string_view ipcMessageTypeName(IpcMessageType type) { return typeName(type); }

api::Result<IpcMessageType> parseIpcMessageType(std::string_view name) {
    if (name == "request") return IpcMessageType::Request;
    if (name == "response") return IpcMessageType::Response;
    if (name == "cancel") return IpcMessageType::Cancel;
    if (name == "notify") return IpcMessageType::Notify;
    return api::Error{api::ErrorCode::InvalidFormat, "unknown ipc message type"};
}

std::string serializeIpcMessage(IpcMessage const& message) {
    std::map<std::string, api::JsonValue> object;
    object.emplace("type", api::JsonValue{std::string{typeName(message.type)}});
    object.emplace("id", api::JsonValue{static_cast<double>(message.id.value())});
    object.emplace("ctx", api::JsonValue{static_cast<double>(message.contextId.value())});
    object.emplace("method", api::JsonValue{message.method});
    object.emplace("payload", api::JsonValue{message.payload});
    object.emplace("error", api::JsonValue{static_cast<double>(static_cast<int>(message.error))});
    return api::serializeJson(api::JsonValue{std::move(object)});
}

api::Result<IpcMessage> parseIpcMessage(std::string_view json) {
    auto parsed = api::parseJson(json);
    if (parsed.isErr()) {
        return parsed.error();
    }

    auto const& root = parsed.value();
    if (!root.isObject()) {
        return api::Error{api::ErrorCode::InvalidFormat, "ipc message must be an object"};
    }

    auto typeString = readString(root, "type", true);
    if (typeString.isErr()) return typeString.error();
    auto type = parseIpcMessageType(typeString.value());
    if (type.isErr()) return type.error();

    auto id = readUint64(root, "id");
    if (id.isErr()) return id.error();
    auto ctx = readUint64(root, "ctx");
    if (ctx.isErr()) return ctx.error();

    auto method = readString(root, "method", false);
    if (method.isErr()) return method.error();
    auto payload = readString(root, "payload", false);
    if (payload.isErr()) return payload.error();

    IpcMessage message;
    message.type      = type.value();
    message.id        = api::RequestId{id.value()};
    message.contextId = api::ContextId{ctx.value()};
    message.method    = std::move(method.value());
    message.payload   = std::move(payload.value());

    auto const* errorNode = root.find("error");
    if (errorNode != nullptr && errorNode->isNumber()) {
        message.error = static_cast<api::ErrorCode>(static_cast<int>(errorNode->asNumber()));
    }

    return message;
}

} // namespace dearoreui::ipc
