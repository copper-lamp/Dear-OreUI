#pragma once

#include "api/types/Error.h"
#include "api/types/Result.h"
#include "facet/IFacetProvider.h"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace dearoreui::facet {

class FacetRegistry {
public:
    FacetRegistry() = default;

    [[nodiscard]] api::Result<void> registerProvider(std::shared_ptr<IFacetProvider> provider);

    [[nodiscard]] bool unregister(std::string_view name);

    [[nodiscard]] api::Result<std::string> handle(std::string_view name, std::string_view args) const;

    [[nodiscard]] bool has(std::string_view name) const;
    void               clear();

private:
    mutable std::mutex                                               mMutex;
    std::unordered_map<std::string, std::shared_ptr<IFacetProvider>> mProviders;
};

} // namespace dearoreui::facet
