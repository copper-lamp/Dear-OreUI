#pragma once

#include "api/types/Page.h"
#include "registry/RegistryEntry.h"
#include "resource/ResourceUri.h"
#include "source/PageSourceSnapshot.h"

#include <optional>
#include <string>
#include <vector>

namespace dearoreui::resource {

struct ResourceLocation {
    std::string uri;
    std::string contentType;
    std::size_t size{0};
    api::PageScope scope{api::PageScope::Any};

    [[nodiscard]] bool operator==(ResourceLocation const& other) const {
        return uri == other.uri && contentType == other.contentType && size == other.size
            && scope == other.scope;
    }

    [[nodiscard]] bool operator!=(ResourceLocation const& other) const { return !(*this == other); }
};

class IResourceIndex {
public:
    virtual ~IResourceIndex() = default;

    virtual void registerSnapshot(source::PageSourceSnapshot const& snapshot) = 0;
    virtual void registerModResource(registry::ResourceEntry const& entry)    = 0;
    virtual void registerModScript(registry::ScriptEntry const& entry)        = 0;
    virtual void registerModStyleSheet(registry::StyleSheetEntry const& entry) = 0;

    [[nodiscard]] virtual std::optional<ResourceLocation> resolve(std::string_view uri) const = 0;
    [[nodiscard]] virtual std::vector<ResourceLocation> listForPage(api::PageScope scope) const = 0;
};

} // namespace dearoreui::resource
