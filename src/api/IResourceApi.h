#pragma once

#include "api/manifest/ResourceManifest.h"
#include "api/manifest/ScriptManifest.h"
#include "api/manifest/StyleSheetManifest.h"
#include "api/types/Id.h"
#include "api/types/Result.h"
#include "api/types/ResourceRead.h"

#include <string>
#include <string_view>

namespace dearoreui::api {

class IResourceApi {
public:
    virtual ~IResourceApi() = default;

    [[nodiscard]] virtual Result<RegistrationHandle>
    registerResource(ModId owner, ResourceManifest const& manifest, std::string payload) = 0;

    [[nodiscard]] virtual Result<RegistrationHandle>
    registerScript(ModId owner, ScriptManifest const& manifest, std::string source) = 0;

    [[nodiscard]] virtual Result<RegistrationHandle>
    registerStyleSheet(ModId owner, StyleSheetManifest const& manifest, std::string source) = 0;

    [[nodiscard]] virtual Result<void> unregister(RegistrationHandle handle) = 0;

    [[nodiscard]] virtual Result<ResourceInfo> describeResource(ModId requester, std::string_view uri) const = 0;
    [[nodiscard]] virtual Result<ResourceBytes> readResource(ModId requester, std::string_view uri, ResourceReadOptions options) const = 0;
};

} // namespace dearoreui::api
