#include "facet/FacetRegistry.h"

#include "api/types/Error.h"

namespace dearoreui::facet {

api::Result<void> FacetRegistry::registerProvider(std::shared_ptr<IFacetProvider> provider) {
    if (!provider) {
        return api::Error{api::ErrorCode::InvalidArgument, "provider is null"};
    }
    auto name = provider->facetName();
    if (name.empty()) {
        return api::Error{api::ErrorCode::InvalidArgument, "facet name is empty"};
    }

    std::lock_guard lock(mMutex);
    if (mProviders.find(name) != mProviders.end()) {
        return api::Error{api::ErrorCode::AlreadyExists, "facet already registered"};
    }
    mProviders.emplace(std::move(name), std::move(provider));
    return api::Result<void>::success();
}

bool FacetRegistry::unregister(std::string_view name) {
    std::lock_guard lock(mMutex);
    return mProviders.erase(std::string{name}) > 0;
}

api::Result<std::string> FacetRegistry::handle(std::string_view name, std::string_view args) const {
    std::shared_ptr<IFacetProvider> provider;
    {
        std::lock_guard lock(mMutex);
        auto            iterator = mProviders.find(std::string{name});
        if (iterator == mProviders.end()) {
            return api::Error{api::ErrorCode::HostMethodNotFound, "facet not found"};
        }
        provider = iterator->second;
    }
    return provider->handle(args);
}

bool FacetRegistry::has(std::string_view name) const {
    std::lock_guard lock(mMutex);
    return mProviders.find(std::string{name}) != mProviders.end();
}

void FacetRegistry::clear() {
    std::lock_guard lock(mMutex);
    mProviders.clear();
}

} // namespace dearoreui::facet
