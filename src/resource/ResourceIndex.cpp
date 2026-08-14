#include "resource/ResourceIndex.h"

#include "api/manifest/ResourceManifest.h"
#include "api/manifest/ScriptManifest.h"
#include "api/manifest/StyleSheetManifest.h"

#include <algorithm>

namespace dearoreui::resource {

namespace {

[[nodiscard]] std::string toUri(
    ResourceUriScheme scheme, std::string const& modNamespace, std::string const& path
) {
    ResourceUri uri;
    uri.scheme       = scheme;
    uri.modNamespace = modNamespace;
    uri.path         = path;
    return uri.toString();
}

} // namespace

void ResourceIndex::registerSnapshot(source::PageSourceSnapshot const& snapshot) {
    std::lock_guard lock{mMutex};
    for (auto const& [path, content] : snapshot.textResources) {
        mOriginalPaths.insert(path);
        ResourceLocation location;
        location.uri         = toUri(ResourceUriScheme::Resource, "vanilla", path);
        location.contentType = contentTypeFor(
            path.ends_with(".css")  ? api::ResourceKind::StyleSheet
            : path.ends_with(".js") ? api::ResourceKind::JavaScript
            : path.ends_with(".html") || path.ends_with(".htm") ? api::ResourceKind::Html
            : path.ends_with(".json") ? api::ResourceKind::Json
                                      : api::ResourceKind::Binary
        );
        location.size        = content.size();
        location.scope       = api::PageScope::Any;
        mLocations.emplace(location.uri, std::move(location));
    }
    for (auto const& [path, content] : snapshot.binaryResources) {
        mOriginalPaths.insert(path);
        ResourceLocation location;
        location.uri         = toUri(ResourceUriScheme::Resource, "vanilla", path);
        location.contentType = contentTypeFor(api::ResourceKind::Binary);
        location.size        = content.size();
        location.scope       = api::PageScope::Any;
        mLocations.emplace(location.uri, std::move(location));
    }
}

void ResourceIndex::registerModResource(registry::ResourceEntry const& entry) {
    ResourceLocation location;
    location.uri         = toUri(ResourceUriScheme::Resource, entry.manifest.modNamespace, entry.manifest.path);
    location.contentType = contentTypeFor(entry.manifest.kind);
    location.size        = entry.payload.size();
    location.scope       = entry.manifest.pageScopes.empty() ? api::PageScope::Any
                                                             : entry.manifest.pageScopes.front();

    std::lock_guard lock{mMutex};
    mLocations.emplace(location.uri, std::move(location));
}

void ResourceIndex::registerModScript(registry::ScriptEntry const& entry) {
    ResourceLocation location;
    location.uri         = toUri(ResourceUriScheme::Script, entry.manifest.modNamespace, entry.manifest.path);
    location.contentType = "text/javascript";
    location.size        = entry.source.size();
    location.scope       = entry.manifest.pageScopes.empty() ? api::PageScope::Any
                                                             : entry.manifest.pageScopes.front();

    std::lock_guard lock{mMutex};
    mLocations.emplace(location.uri, std::move(location));
}

void ResourceIndex::registerModStyleSheet(registry::StyleSheetEntry const& entry) {
    ResourceLocation location;
    location.uri         = toUri(ResourceUriScheme::Style, entry.manifest.modNamespace, entry.manifest.path);
    location.contentType = "text/css";
    location.size        = entry.source.size();
    location.scope       = entry.manifest.pageScopes.empty() ? api::PageScope::Any
                                                             : entry.manifest.pageScopes.front();

    std::lock_guard lock{mMutex};
    mLocations.emplace(location.uri, std::move(location));
}

std::optional<ResourceLocation> ResourceIndex::resolve(std::string_view uri) const {
    std::lock_guard lock{mMutex};
    auto iterator = mLocations.find(std::string(uri));
    if (iterator == mLocations.end()) {
        return std::nullopt;
    }
    return iterator->second;
}

std::vector<ResourceLocation> ResourceIndex::listForPage(api::PageScope scope) const {
    std::lock_guard lock{mMutex};
    std::vector<ResourceLocation> result;
    for (auto const& [uri, location] : mLocations) {
        static_cast<void>(uri);
        if (scopeMatches(scope, {location.scope})) {
            result.push_back(location);
        }
    }
    return result;
}

bool ResourceIndex::scopeMatches(
    api::PageScope target, std::vector<api::PageScope> const& scopes
) {
    if (scopes.empty()) return true;
    for (auto scope : scopes) {
        if (scope == api::PageScope::Any || target == api::PageScope::Any || scope == target) {
            return true;
        }
    }
    return false;
}

std::string ResourceIndex::contentTypeFor(api::ResourceKind kind) {
    switch (kind) {
    case api::ResourceKind::Html:
        return "text/html";
    case api::ResourceKind::StyleSheet:
        return "text/css";
    case api::ResourceKind::JavaScript:
    case api::ResourceKind::Json:
        return "application/javascript";
    case api::ResourceKind::Texture:
        return "image/png";
    case api::ResourceKind::Font:
        return "font/woff2";
    case api::ResourceKind::Audio:
        return "audio/ogg";
    default:
        return "application/octet-stream";
    }
}

} // namespace dearoreui::resource
