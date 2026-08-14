#pragma once

#include "api/manifest/Permission.h"
#include "api/types/Error.h"
#include "api/types/Id.h"
#include "api/types/Result.h"

#include <string>
#include <string_view>

namespace dearoreui::ipc {

class IHostMethod {
public:
    virtual ~IHostMethod() = default;

    [[nodiscard]] virtual std::string     name() const               = 0;
    [[nodiscard]] virtual api::Permission requiredPermission() const = 0;

    [[nodiscard]] virtual api::Result<std::string> execute(api::ContextId contextId, std::string_view args) = 0;
};

} // namespace dearoreui::ipc
