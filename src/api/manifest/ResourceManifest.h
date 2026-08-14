#pragma once

#include "api/manifest/Dependency.h"
#include "api/manifest/Permission.h"
#include "api/types/Page.h"
#include "api/types/Version.h"

#include <optional>
#include <string>
#include <vector>

namespace dearoreui::api {

enum class ResourceKind {
    JavaScript,
    StyleSheet,
    Html,
    Json,
    Texture,
    Font,
    Audio,
    Binary,
};

[[nodiscard]] constexpr std::string_view resourceKindName(ResourceKind kind) {
    switch (kind) {
    case ResourceKind::JavaScript:
        return "javascript";
    case ResourceKind::StyleSheet:
        return "stylesheet";
    case ResourceKind::Html:
        return "html";
    case ResourceKind::Json:
        return "json";
    case ResourceKind::Texture:
        return "texture";
    case ResourceKind::Font:
        return "font";
    case ResourceKind::Audio:
        return "audio";
    case ResourceKind::Binary:
        return "binary";
    }
    return "unknown";
}

enum class ResourceSource {
    Inline,
    ModResource,
};

struct VersionConstraint {
    std::string minimum;
    std::string maximum;
};

struct ResourceManifest {
    std::string                      modNamespace;
    std::string                      path;
    ResourceKind                     kind{ResourceKind::Binary};
    ResourceSource                   source{ResourceSource::Inline};
    std::optional<VersionConstraint> versionConstraint;
    std::vector<PageScope>           pageScopes;
    std::vector<Dependency>          dependencies;
    std::vector<std::string>         conflicts;
    PermissionSet                    permissions;
    std::string                      fingerprint; // optional content fingerprint

    [[nodiscard]] bool operator==(ResourceManifest const& other) const {
        return modNamespace == other.modNamespace && path == other.path && kind == other.kind && source == other.source
            && fingerprint == other.fingerprint;
    }

    [[nodiscard]] bool operator!=(ResourceManifest const& other) const { return !(*this == other); }
};

} // namespace dearoreui::api
