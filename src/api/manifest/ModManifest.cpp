#include "api/manifest/ModManifest.h"
#include "api/manifest/JsonManifestParser.h"

#include <algorithm>

namespace dearoreui::api {

namespace {

Result<std::string> requireString(JsonValue const* value, std::string_view field) {
    if (value == nullptr) {
        return Error{ErrorCode::InvalidManifest, std::string{"Missing field: "} + std::string{field}};
    }
    if (!value->isString()) {
        return Error{ErrorCode::InvalidManifest, std::string{"Field must be a string: "} + std::string{field}};
    }
    return value->asString();
}

Result<std::uint32_t> requireUint(JsonValue const* value, std::string_view field) {
    if (value == nullptr) {
        return Error{ErrorCode::InvalidManifest, std::string{"Missing field: "} + std::string{field}};
    }
    if (!value->isNumber()) {
        return Error{ErrorCode::InvalidManifest, std::string{"Field must be a number: "} + std::string{field}};
    }
    auto number = value->asNumber();
    if (number < 0.0 || number != std::floor(number)) {
        return Error{ErrorCode::InvalidManifest, std::string{"Field must be an integer: "} + std::string{field}};
    }
    return static_cast<std::uint32_t>(number);
}

Result<std::uint32_t> optionalUint(JsonValue const* value, std::uint32_t defaultValue) {
    if (value == nullptr) {
        return defaultValue;
    }
    if (!value->isNumber()) {
        return Error{ErrorCode::InvalidManifest, "Field must be a number"};
    }
    auto number = value->asNumber();
    if (number < 0.0 || number != std::floor(number)) {
        return Error{ErrorCode::InvalidManifest, "Field must be an integer"};
    }
    return static_cast<std::uint32_t>(number);
}

Result<std::string> optionalString(JsonValue const* value, std::string defaultValue = {}) {
    if (value == nullptr) {
        return defaultValue;
    }
    if (!value->isString()) {
        return Error{ErrorCode::InvalidManifest, "Field must be a string"};
    }
    return value->asString();
}

Result<std::vector<std::string>> parseStringArray(JsonValue const* value, std::string_view field) {
    if (value == nullptr) {
        return std::vector<std::string>{};
    }
    if (!value->isArray()) {
        return Error{ErrorCode::InvalidManifest, std::string{"Field must be an array: "} + std::string{field}};
    }
    std::vector<std::string> result;
    for (auto const& item : value->asArray()) {
        if (!item.isString()) {
            return Error{ErrorCode::InvalidManifest, "Array item must be a string"};
        }
        result.push_back(item.asString());
    }
    return result;
}

Result<Permission> parsePermission(std::string_view name) {
    if (name == "resource.read") return Permission::ResourceRead;
    if (name == "resource.register") return Permission::ResourceRegister;
    if (name == "page.observe") return Permission::PageObserve;
    if (name == "ui.mount") return Permission::UiMount;
    if (name == "host.read_only") return Permission::HostReadOnly;
    if (name == "host.write") return Permission::HostWrite;
    if (name == "transform.resource") return Permission::TransformResource;
    if (name == "transform.bundle") return Permission::TransformBundle;
    if (name == "diagnostic.read") return Permission::DiagnosticRead;
    return Error{ErrorCode::PermissionDenied, std::string{"Unknown permission: "} + std::string{name}};
}

Result<std::vector<Permission>> parsePermissions(JsonValue const* value) {
    if (value == nullptr) {
        return std::vector<Permission>{};
    }
    if (!value->isArray()) {
        return Error{ErrorCode::InvalidManifest, "permissions must be an array"};
    }
    std::vector<Permission> result;
    for (auto const& item : value->asArray()) {
        if (!item.isString()) {
            return Error{ErrorCode::InvalidManifest, "permission must be a string"};
        }
        auto permission = parsePermission(item.asString());
        if (permission.isErr()) {
            return permission.error();
        }
        result.push_back(permission.value());
    }
    return result;
}

Result<VersionConstraint> parseVersionConstraint(JsonValue const* value) {
    if (value == nullptr) {
        return VersionConstraint{};
    }
    if (!value->isObject()) {
        return Error{ErrorCode::InvalidFormat, "version_constraint must be an object"};
    }
    VersionConstraint constraint;
    if (auto minValue = value->find("minimum")) {
        if (!minValue->isString()) {
            return Error{ErrorCode::InvalidFormat, "version_constraint.minimum must be a string"};
        }
        constraint.minimum = minValue->asString();
    }
    if (auto maxValue = value->find("maximum")) {
        if (!maxValue->isString()) {
            return Error{ErrorCode::InvalidFormat, "version_constraint.maximum must be a string"};
        }
        constraint.maximum = maxValue->asString();
    }
    return constraint;
}

Result<std::vector<PageScope>> parsePageScopes(JsonValue const* value) {
    if (value == nullptr) {
        return std::vector<PageScope>{};
    }
    if (!value->isArray()) {
        return Error{ErrorCode::InvalidFormat, "page_scope must be an array"};
    }
    std::vector<PageScope> result;
    for (auto const& item : value->asArray()) {
        if (!item.isString()) {
            return Error{ErrorCode::InvalidFormat, "page_scope item must be a string"};
        }
        auto const& name = item.asString();
        if (name == "any") result.push_back(PageScope::Any);
        else if (name == "main_menu") result.push_back(PageScope::MainMenu);
        else if (name == "play_screen") result.push_back(PageScope::PlayScreen);
        else if (name == "settings") result.push_back(PageScope::Settings);
        else if (name == "pause") result.push_back(PageScope::Pause);
        else if (name == "in_game") result.push_back(PageScope::InGame);
        else if (name == "custom") result.push_back(PageScope::Custom);
        else return Error{ErrorCode::InvalidFormat, "Unknown page_scope: " + name};
    }
    return result;
}

Result<std::vector<Dependency>> parseDependencies(JsonValue const* value) {
    if (value == nullptr) {
        return std::vector<Dependency>{};
    }
    if (!value->isArray()) {
        return Error{ErrorCode::InvalidFormat, "dependencies must be an array"};
    }
    std::vector<Dependency> result;
    for (auto const& item : value->asArray()) {
        if (!item.isObject()) {
            return Error{ErrorCode::InvalidFormat, "dependency must be an object"};
        }
        Dependency dependency;
        auto nsResult = requireString(item.find("namespace"), "dependency.namespace");
        if (nsResult.isErr()) return nsResult.error();
        dependency.modNamespace = std::move(nsResult.value());

        auto rangeResult = requireString(item.find("version_range"), "dependency.version_range");
        if (rangeResult.isErr()) return rangeResult.error();
        dependency.versionRange = std::move(rangeResult.value());

        if (auto optValue = item.find("optional")) {
            if (!optValue->isBoolean()) {
                return Error{ErrorCode::InvalidFormat, "dependency.optional must be a boolean"};
            }
            dependency.optional = optValue->asBoolean();
        }
        result.push_back(std::move(dependency));
    }
    return result;
}

Result<ResourceKind> parseResourceKind(std::string_view name) {
    if (name == "javascript") return ResourceKind::JavaScript;
    if (name == "stylesheet") return ResourceKind::StyleSheet;
    if (name == "html") return ResourceKind::Html;
    if (name == "json") return ResourceKind::Json;
    if (name == "texture") return ResourceKind::Texture;
    if (name == "font") return ResourceKind::Font;
    if (name == "audio") return ResourceKind::Audio;
    if (name == "binary") return ResourceKind::Binary;
    return Error{ErrorCode::InvalidFormat, std::string{"Unknown resource kind: "} + std::string{name}};
}

Result<ResourceSource> parseResourceSource(std::string_view name) {
    if (name == "inline") return ResourceSource::Inline;
    if (name == "mod_resource") return ResourceSource::ModResource;
    return Error{ErrorCode::InvalidFormat, std::string{"Unknown resource source: "} + std::string{name}};
}

Result<ResourceManifest> parseResourceManifest(JsonValue const& value, std::string_view defaultNamespace) {
    if (!value.isObject()) {
        return Error{ErrorCode::InvalidManifest, "resource must be an object"};
    }
    ResourceManifest manifest;

    auto nsResult = optionalString(value.find("namespace"), std::string{defaultNamespace});
    if (nsResult.isErr()) return nsResult.error();
    manifest.modNamespace = std::move(nsResult.value());

    auto pathResult = requireString(value.find("path"), "resource.path");
    if (pathResult.isErr()) return pathResult.error();
    manifest.path = std::move(pathResult.value());

    if (auto kindValue = value.find("kind")) {
        if (!kindValue->isString()) return Error{ErrorCode::InvalidFormat, "resource.kind must be a string"};
        auto kind = parseResourceKind(kindValue->asString());
        if (kind.isErr()) return kind.error();
        manifest.kind = kind.value();
    }

    if (auto sourceValue = value.find("source")) {
        if (!sourceValue->isString()) return Error{ErrorCode::InvalidFormat, "resource.source must be a string"};
        auto source = parseResourceSource(sourceValue->asString());
        if (source.isErr()) return source.error();
        manifest.source = source.value();
    }

    auto versionResult = parseVersionConstraint(value.find("version_constraint"));
    if (versionResult.isErr()) return versionResult.error();
    manifest.versionConstraint = std::move(versionResult.value());

    auto scopesResult = parsePageScopes(value.find("page_scope"));
    if (scopesResult.isErr()) return scopesResult.error();
    manifest.pageScopes = std::move(scopesResult.value());

    auto depsResult = parseDependencies(value.find("dependencies"));
    if (depsResult.isErr()) return depsResult.error();
    manifest.dependencies = std::move(depsResult.value());

    auto conflictsResult = parseStringArray(value.find("conflicts"), "conflicts");
    if (conflictsResult.isErr()) return conflictsResult.error();
    manifest.conflicts = std::move(conflictsResult.value());

    auto permissionsResult = parsePermissions(value.find("permissions"));
    if (permissionsResult.isErr()) return permissionsResult.error();
    manifest.permissions = PermissionSet{std::move(permissionsResult.value())};

    auto fingerprintResult = optionalString(value.find("fingerprint"));
    if (fingerprintResult.isErr()) return fingerprintResult.error();
    manifest.fingerprint = std::move(fingerprintResult.value());

    return manifest;
}

Result<ScriptManifest> parseScriptManifest(JsonValue const& value, std::string_view defaultNamespace) {
    if (!value.isObject()) {
        return Error{ErrorCode::InvalidManifest, "script must be an object"};
    }
    ScriptManifest manifest;

    auto nsResult = optionalString(value.find("namespace"), std::string{defaultNamespace});
    if (nsResult.isErr()) return nsResult.error();
    manifest.modNamespace = std::move(nsResult.value());

    auto pathResult = requireString(value.find("path"), "script.path");
    if (pathResult.isErr()) return pathResult.error();
    manifest.path = std::move(pathResult.value());

    auto versionResult = parseVersionConstraint(value.find("version_constraint"));
    if (versionResult.isErr()) return versionResult.error();
    manifest.versionConstraint = std::move(versionResult.value());

    auto scopesResult = parsePageScopes(value.find("page_scope"));
    if (scopesResult.isErr()) return scopesResult.error();
    manifest.pageScopes = std::move(scopesResult.value());

    auto depsResult = parseDependencies(value.find("dependencies"));
    if (depsResult.isErr()) return depsResult.error();
    manifest.dependencies = std::move(depsResult.value());

    auto conflictsResult = parseStringArray(value.find("conflicts"), "conflicts");
    if (conflictsResult.isErr()) return conflictsResult.error();
    manifest.conflicts = std::move(conflictsResult.value());

    auto permissionsResult = parsePermissions(value.find("permissions"));
    if (permissionsResult.isErr()) return permissionsResult.error();
    manifest.permissions = PermissionSet{std::move(permissionsResult.value())};

    auto fingerprintResult = optionalString(value.find("fingerprint"));
    if (fingerprintResult.isErr()) return fingerprintResult.error();
    manifest.fingerprint = std::move(fingerprintResult.value());

    auto sourceResult = requireString(value.find("source"), "script.source");
    if (sourceResult.isErr()) return sourceResult.error();
    manifest.source = std::move(sourceResult.value());

    return manifest;
}

Result<StyleSheetManifest> parseStyleSheetManifest(JsonValue const& value, std::string_view defaultNamespace) {
    if (!value.isObject()) {
        return Error{ErrorCode::InvalidManifest, "stylesheet must be an object"};
    }
    StyleSheetManifest manifest;

    auto nsResult = optionalString(value.find("namespace"), std::string{defaultNamespace});
    if (nsResult.isErr()) return nsResult.error();
    manifest.modNamespace = std::move(nsResult.value());

    auto pathResult = requireString(value.find("path"), "stylesheet.path");
    if (pathResult.isErr()) return pathResult.error();
    manifest.path = std::move(pathResult.value());

    auto versionResult = parseVersionConstraint(value.find("version_constraint"));
    if (versionResult.isErr()) return versionResult.error();
    manifest.versionConstraint = std::move(versionResult.value());

    auto scopesResult = parsePageScopes(value.find("page_scope"));
    if (scopesResult.isErr()) return scopesResult.error();
    manifest.pageScopes = std::move(scopesResult.value());

    auto depsResult = parseDependencies(value.find("dependencies"));
    if (depsResult.isErr()) return depsResult.error();
    manifest.dependencies = std::move(depsResult.value());

    auto conflictsResult = parseStringArray(value.find("conflicts"), "conflicts");
    if (conflictsResult.isErr()) return conflictsResult.error();
    manifest.conflicts = std::move(conflictsResult.value());

    auto permissionsResult = parsePermissions(value.find("permissions"));
    if (permissionsResult.isErr()) return permissionsResult.error();
    manifest.permissions = PermissionSet{std::move(permissionsResult.value())};

    auto fingerprintResult = optionalString(value.find("fingerprint"));
    if (fingerprintResult.isErr()) return fingerprintResult.error();
    manifest.fingerprint = std::move(fingerprintResult.value());

    auto sourceResult = requireString(value.find("source"), "stylesheet.source");
    if (sourceResult.isErr()) return sourceResult.error();
    manifest.source = std::move(sourceResult.value());

    return manifest;
}

} // namespace

Result<ModManifest> ModManifest::fromJson(std::string_view json) {
    auto parsed = parseJson(json);
    if (parsed.isErr()) {
        return parsed.error();
    }

    auto const& root = parsed.value();
    if (!root.isObject()) {
        return Error{ErrorCode::InvalidManifest, "Manifest root must be an object"};
    }

    ModManifest manifest;

    auto versionResult = optionalUint(root.find("manifest_version"), 1);
    if (versionResult.isErr()) return versionResult.error();
    manifest.manifestVersion = versionResult.value();

    auto nsResult = requireString(root.find("namespace"), "namespace");
    if (nsResult.isErr()) return nsResult.error();
    manifest.modNamespace = std::move(nsResult.value());

    auto idResult = requireString(root.find("id"), "id");
    if (idResult.isErr()) return idResult.error();
    manifest.id = ModId{std::move(idResult.value())};

    auto displayResult = optionalString(root.find("display_name"));
    if (displayResult.isErr()) return displayResult.error();
    manifest.displayName = std::move(displayResult.value());

    auto modVersionResult = requireString(root.find("mod_version"), "mod_version");
    if (modVersionResult.isErr()) return modVersionResult.error();
    auto versionParsed = Version::parse(modVersionResult.value());
    if (versionParsed.isErr()) {
        return Error{ErrorCode::InvalidManifest, "mod_version is invalid: " + modVersionResult.value()};
    }
    manifest.modVersion = versionParsed.value();

    auto apiVersionResult = optionalUint(root.find("api_version"), 1);
    if (apiVersionResult.isErr()) return apiVersionResult.error();
    manifest.apiVersion = apiVersionResult.value();

    auto depsResult = parseDependencies(root.find("dependencies"));
    if (depsResult.isErr()) return depsResult.error();
    manifest.dependencies = std::move(depsResult.value());

    auto permissionsResult = parsePermissions(root.find("permissions"));
    if (permissionsResult.isErr()) return permissionsResult.error();
    manifest.permissions = std::move(permissionsResult.value());

    if (auto resourcesValue = root.find("resources")) {
        if (!resourcesValue->isArray()) {
            return Error{ErrorCode::InvalidManifest, "resources must be an array"};
        }
        for (auto const& item : resourcesValue->asArray()) {
            auto resource = parseResourceManifest(item, manifest.modNamespace);
            if (resource.isErr()) return resource.error();
            manifest.resources.push_back(std::move(resource.value()));
        }
    }

    if (auto scriptsValue = root.find("scripts")) {
        if (!scriptsValue->isArray()) {
            return Error{ErrorCode::InvalidManifest, "scripts must be an array"};
        }
        for (auto const& item : scriptsValue->asArray()) {
            auto script = parseScriptManifest(item, manifest.modNamespace);
            if (script.isErr()) return script.error();
            manifest.scripts.push_back(std::move(script.value()));
        }
    }

    if (auto stylesValue = root.find("styles")) {
        if (!stylesValue->isArray()) {
            return Error{ErrorCode::InvalidManifest, "styles must be an array"};
        }
        for (auto const& item : stylesValue->asArray()) {
            auto style = parseStyleSheetManifest(item, manifest.modNamespace);
            if (style.isErr()) return style.error();
            manifest.styles.push_back(std::move(style.value()));
        }
    }

    return manifest;
}

} // namespace dearoreui::api
