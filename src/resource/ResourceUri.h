#pragma once

#include "api/types/Result.h"

#include <string>

namespace dearoreui::resource {

enum class ResourceUriScheme {
    Resource,
    Script,
    Style,
    Unknown,
};

struct ResourceUri {
    ResourceUriScheme scheme{ResourceUriScheme::Unknown};
    std::string       modNamespace;
    std::string       path;

    [[nodiscard]] static api::Result<ResourceUri> parse(std::string_view uri);
    [[nodiscard]] std::string                     toString() const;
    [[nodiscard]] bool                            operator==(ResourceUri const& other) const;
    [[nodiscard]] bool                            operator!=(ResourceUri const& other) const;
};

} // namespace dearoreui::resource
