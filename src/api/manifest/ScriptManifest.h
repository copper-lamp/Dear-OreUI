#pragma once

#include "api/manifest/Dependency.h"
#include "api/manifest/Permission.h"
#include "api/types/Page.h"
#include "api/types/Version.h"

#include <optional>
#include <string>
#include <vector>

namespace dearoreui::api {

struct ScriptManifest {
    std::string                      modNamespace;
    std::string                      path;
    std::optional<VersionConstraint> versionConstraint;
    std::vector<PageScope>           pageScopes;
    std::vector<Dependency>          dependencies;
    std::vector<std::string>         conflicts;
    PermissionSet                    permissions;
    std::string                      fingerprint;
    std::string                      source;

    [[nodiscard]] bool operator==(ScriptManifest const& other) const {
        return modNamespace == other.modNamespace && path == other.path && fingerprint == other.fingerprint
            && source == other.source;
    }

    [[nodiscard]] bool operator!=(ScriptManifest const& other) const { return !(*this == other); }
};

} // namespace dearoreui::api
