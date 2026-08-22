#include "facet/FacetHostMethod.h"

namespace dearoreui::facet {

FacetHostMethod::FacetHostMethod(std::shared_ptr<IFacetProvider> provider, api::Permission requiredPermission)
: mProvider(std::move(provider)),
  mRequiredPermission(requiredPermission) {}

std::string FacetHostMethod::name() const { return mProvider ? mProvider->facetName() : std::string{}; }

api::Permission FacetHostMethod::requiredPermission() const { return mRequiredPermission; }

api::Result<std::string> FacetHostMethod::execute(api::ContextId /*contextId*/, std::string_view args) {
    if (!mProvider) {
        return api::Error{api::ErrorCode::InternalError, "facet provider is null"};
    }
    return mProvider->handle(args);
}

} // namespace dearoreui::facet
