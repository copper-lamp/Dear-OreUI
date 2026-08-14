#pragma once

#include "api/IRuntimeApi.h"
#include "facet/IFacetProvider.h"

namespace dearoreui::facet {

class RuntimeInfoFacet : public IFacetProvider {
public:
    explicit RuntimeInfoFacet(api::IRuntimeApi const& runtime);

    [[nodiscard]] std::string              facetName() const override { return "runtime.info"; }
    [[nodiscard]] api::Result<std::string> handle(std::string_view args) override;

private:
    api::IRuntimeApi const& mRuntime;
};

} // namespace dearoreui::facet
