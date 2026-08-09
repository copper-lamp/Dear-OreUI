#pragma once

#include "api/types/Id.h"

#include <string>
#include <vector>

namespace dearoreui::api {

enum class ErrorCode {
    None,
    InvalidArgument,
    InvalidFormat,
    InvalidState,
    NotFound,
    AlreadyExists,
    NotSupported,
    VersionMismatch,
    DependencyMissing,
    DependencyCycle,
    ResourceConflict,
    ResourceNotFound,
    InvalidManifest,
    NamespaceConflict,
    PermissionDenied,
    InjectionFailed,
    HostMethodNotFound,
    HostPermissionDenied,
    HostCallTimeout,
    HostCallCancelled,
    InvalidContext,
    InternalError,
};

struct Error {
    ErrorCode   code{ErrorCode::None};
    std::string message;
    std::string category;
    std::vector<std::string> details;

    [[nodiscard]] bool isOk() const { return code == ErrorCode::None && message.empty(); }

    [[nodiscard]] static Error ok() { return Error{}; }
};

} // namespace dearoreui::api
