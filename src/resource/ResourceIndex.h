#pragma once

#include "resource/IResourceIndex.h"

#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace dearoreui::resource {

class ResourceIndex : public IResourceIndex {
public:
    ResourceIndex() = default;

    void registerSnapshot(source::PageSourceSnapshot const& snapshot) override;
    void registerModResource(registry::ResourceEntry const& entry) override;
    void registerModScript(registry::ScriptEntry const& entry) override;
    void registerModStyleSheet(registry::StyleSheetEntry const& entry) override;

    [[nodiscard]] std::optional<ResourceLocation> resolve(std::string_view uri) const override;
    [[nodiscard]] std::vector<ResourceLocation>   listForPage(api::PageScope scope) const override;

private:
    [[nodiscard]] static bool        scopeMatches(api::PageScope target, std::vector<api::PageScope> const& scopes);
    [[nodiscard]] static std::string contentTypeFor(api::ResourceKind kind);

    mutable std::mutex                                mMutex;
    std::unordered_map<std::string, ResourceLocation> mLocations;
    std::unordered_set<std::string>                   mOriginalPaths;
};

} // namespace dearoreui::resource
