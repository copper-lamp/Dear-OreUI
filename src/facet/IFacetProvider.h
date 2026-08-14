#pragma once

#include "api/types/Error.h"
#include "api/types/Result.h"

#include <string>
#include <string_view>

namespace dearoreui::facet {

class IFacetProvider {
public:
    virtual ~IFacetProvider() = default;

    [[nodiscard]] virtual std::string              facetName() const             = 0;
    [[nodiscard]] virtual api::Result<std::string> handle(std::string_view args) = 0;
};

} // namespace dearoreui::facet
