#pragma once

#include "api/manifest/Permission.h"
#include "api/types/Id.h"
#include "api/types/Result.h"
#include "ipc/IHostMethod.h"

#include <memory>

namespace dearoreui::api {

class IHostApi {
public:
    virtual ~IHostApi() = default;

    [[nodiscard]] virtual Result<RegistrationHandle>
    registerHostMethod(
        ModId owner,
        PermissionSet const& permissions,
        std::shared_ptr<ipc::IHostMethod> method
    ) = 0;

    [[nodiscard]] virtual Result<void> unregisterHostMethod(RegistrationHandle handle) = 0;
};

} // namespace dearoreui::api
