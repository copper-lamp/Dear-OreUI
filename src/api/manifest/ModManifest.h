#pragma once

#include "api/manifest/Dependency.h"
#include "api/manifest/Permission.h"
#include "api/manifest/ResourceManifest.h"
#include "api/manifest/ScriptManifest.h"
#include "api/manifest/StyleSheetManifest.h"
#include "api/types/Id.h"
#include "api/types/Result.h"
#include "api/types/Version.h"

#include <cstdint>
#include <string>
#include <vector>

namespace dearoreui::api {

struct ModManifest {
    std::uint32_t                   manifestVersion{1};
    ModId                           id;
    std::string                     modNamespace;
    std::string                     displayName;
    Version                         modVersion;
    std::uint32_t                   apiVersion{1};
    std::vector<Dependency>         dependencies;
    std::vector<Permission>         permissions;
    std::vector<ResourceManifest>   resources;
    std::vector<ScriptManifest>     scripts;
    std::vector<StyleSheetManifest> styles;

    [[nodiscard]] static Result<ModManifest> fromJson(std::string_view json);
};

} // namespace dearoreui::api
