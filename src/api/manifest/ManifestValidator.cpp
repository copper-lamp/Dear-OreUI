#include "api/manifest/ManifestValidator.h"

#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace dearoreui::api {

namespace {

bool allOf(std::string_view text, bool (*predicate)(char)) {
    return std::all_of(text.begin(), text.end(), [predicate](char c) { return predicate(c); });
}

bool isIdChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '-' || c == '_'; }

bool isNamespaceChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '-' || c == '_'; }

bool isPathControlChar(char c) { return c == '\\' || c == ':'; }

} // namespace

// ModId is the same string as the mod namespace (owner == modNamespace across
// every registration), so the id charset follows the namespace rules: dotted
// names are allowed but must not start/end with '.' or contain '..'.
bool ManifestValidator::isValidId(std::string_view id) {
    if (id.empty() || id.size() > 64) {
        return false;
    }
    if (!allOf(id, isIdChar)) {
        return false;
    }
    if (id.front() == '.' || id.back() == '.') {
        return false;
    }
    if (id.find("..") != std::string_view::npos) {
        return false;
    }
    return true;
}

bool ManifestValidator::isValidNamespace(std::string_view ns) {
    if (ns.empty() || ns.size() > 128) {
        return false;
    }
    if (!allOf(ns, isNamespaceChar)) {
        return false;
    }
    if (ns.front() == '.' || ns.back() == '.') {
        return false;
    }
    if (ns.find("..") != std::string_view::npos) {
        return false;
    }
    return true;
}

bool ManifestValidator::isValidPath(std::string_view path) {
    if (path.empty() || path.size() > 512) {
        return false;
    }
    if (path.front() == '/' || path.front() == '\\') {
        return false;
    }
    if (path.find("..") != std::string_view::npos) {
        return false;
    }
    if (std::any_of(path.begin(), path.end(), isPathControlChar)) {
        return false;
    }
    return true;
}

Result<void> ManifestValidator::validate(ModManifest const& manifest) {
    if (manifest.manifestVersion != 1) {
        return Error{ErrorCode::InvalidManifest, "manifest_version must be 1"};
    }

    if (!manifest.id.isValid()) {
        return Error{ErrorCode::InvalidManifest, "id is required"};
    }
    if (!isValidId(manifest.id.value())) {
        return Error{ErrorCode::InvalidManifest, "id contains invalid characters"};
    }

    if (manifest.modNamespace.empty()) {
        return Error{ErrorCode::InvalidManifest, "namespace is required"};
    }
    if (!isValidNamespace(manifest.modNamespace)) {
        return Error{ErrorCode::InvalidArgument, "namespace format is invalid"};
    }

    if (!manifest.modVersion.toString().empty() && manifest.modVersion == Version{}) {
        // A default-constructed Version is treated as unset here.
        // If the caller parsed an invalid version, parse() would have already failed.
    }

    if (manifest.apiVersion != DearOreUIProtocolVersion) {
        return Error{
            ErrorCode::VersionMismatch,
            "api_version mismatch: expected " + std::to_string(DearOreUIProtocolVersion) + ", got "
                + std::to_string(manifest.apiVersion)
        };
    }

    for (auto permission : manifest.permissions) {
        switch (permission) {
        case Permission::ResourceRead:
        case Permission::ResourceRegister:
        case Permission::PageObserve:
        case Permission::UiMount:
        case Permission::HostReadOnly:
        case Permission::HostWrite:
        case Permission::TransformResource:
        case Permission::TransformBundle:
        case Permission::DiagnosticRead:
            continue;
        }
        return Error{ErrorCode::PermissionDenied, "permission is not recognized"};
    }

    std::unordered_set<std::string> resourcePaths;
    for (auto const& resource : manifest.resources) {
        auto result = validate(resource);
        if (result.isErr()) {
            return result.error();
        }
        if (resource.modNamespace != manifest.modNamespace) {
            return Error{ErrorCode::InvalidManifest, "resource namespace does not match mod namespace"};
        }
        if (!resourcePaths.insert(resource.path).second) {
            return Error{ErrorCode::AlreadyExists, "duplicate resource path: " + resource.path};
        }
    }

    std::unordered_set<std::string> scriptPaths;
    for (auto const& script : manifest.scripts) {
        auto result = validate(script);
        if (result.isErr()) {
            return result.error();
        }
        if (script.modNamespace != manifest.modNamespace) {
            return Error{ErrorCode::InvalidManifest, "script namespace does not match mod namespace"};
        }
        if (!scriptPaths.insert(script.path).second) {
            return Error{ErrorCode::AlreadyExists, "duplicate script path: " + script.path};
        }
    }

    std::unordered_set<std::string> stylePaths;
    for (auto const& style : manifest.styles) {
        auto result = validate(style);
        if (result.isErr()) {
            return result.error();
        }
        if (style.modNamespace != manifest.modNamespace) {
            return Error{ErrorCode::InvalidManifest, "stylesheet namespace does not match mod namespace"};
        }
        if (!stylePaths.insert(style.path).second) {
            return Error{ErrorCode::AlreadyExists, "duplicate stylesheet path: " + style.path};
        }
    }

    for (auto const& dependency : manifest.dependencies) {
        if (dependency.modNamespace.empty()) {
            return Error{ErrorCode::InvalidManifest, "dependency namespace is required"};
        }
        if (dependency.versionRange.empty()) {
            return Error{ErrorCode::InvalidFormat, "dependency version_range is required"};
        }
    }

    return Result<void>::success();
}

Result<void> ManifestValidator::validate(ResourceManifest const& manifest) {
    return validateCommonResource("resource", manifest.modNamespace, manifest.path);
}

Result<void> ManifestValidator::validate(ScriptManifest const& manifest) {
    return validateCommonResource("script", manifest.modNamespace, manifest.path);
}

Result<void> ManifestValidator::validate(StyleSheetManifest const& manifest) {
    return validateCommonResource("stylesheet", manifest.modNamespace, manifest.path);
}

Result<void> ManifestValidator::validate(UiManifest const& manifest) {
    if (manifest.modNamespace.empty()) {
        return Error{ErrorCode::InvalidManifest, "ui namespace is required"};
    }
    if (!isValidNamespace(manifest.modNamespace)) {
        return Error{ErrorCode::InvalidArgument, "ui namespace is invalid"};
    }
    if (manifest.id.empty()) {
        return Error{ErrorCode::InvalidManifest, "ui id is required"};
    }
    if (!isValidId(manifest.id)) {
        return Error{ErrorCode::InvalidArgument, "ui id contains invalid characters"};
    }
    if (!manifest.containerId.empty() && !isValidId(manifest.containerId)) {
        return Error{ErrorCode::InvalidArgument, "ui container_id contains invalid characters"};
    }
    if (manifest.fingerprint.empty()) {
        return Error{ErrorCode::InvalidManifest, "ui fingerprint is required"};
    }

    std::unordered_set<std::string> scriptPaths;
    for (auto const& path : manifest.scripts) {
        if (!isValidPath(path)) {
            return Error{ErrorCode::InvalidArgument, "ui script path is invalid: " + path};
        }
        if (!scriptPaths.insert(path).second) {
            return Error{ErrorCode::AlreadyExists, "duplicate ui script path: " + path};
        }
    }

    std::unordered_set<std::string> stylePaths;
    for (auto const& path : manifest.styles) {
        if (!isValidPath(path)) {
            return Error{ErrorCode::InvalidArgument, "ui style path is invalid: " + path};
        }
        if (!stylePaths.insert(path).second) {
            return Error{ErrorCode::AlreadyExists, "duplicate ui style path: " + path};
        }
    }

    for (auto const& conflict : manifest.conflicts) {
        if (conflict.empty()) {
            return Error{ErrorCode::InvalidManifest, "ui conflict declaration cannot be empty"};
        }
    }

    return Result<void>::success();
}

Result<void> ManifestValidator::validateCommonResource(
    std::string_view   typeName,
    std::string const& modNamespace,
    std::string const& path
) {
    if (modNamespace.empty()) {
        return Error{ErrorCode::InvalidManifest, std::string{typeName} + " namespace is required"};
    }
    if (!isValidNamespace(modNamespace)) {
        return Error{ErrorCode::InvalidArgument, std::string{typeName} + " namespace is invalid"};
    }
    if (path.empty()) {
        return Error{ErrorCode::InvalidManifest, std::string{typeName} + " path is required"};
    }
    if (!isValidPath(path)) {
        return Error{
            ErrorCode::InvalidArgument,
            std::string{typeName} + " path contains illegal characters or traversal: " + path
        };
    }
    return Result<void>::success();
}

} // namespace dearoreui::api
