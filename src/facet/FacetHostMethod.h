#pragma once

#include "api/manifest/Permission.h"
#include "facet/IFacetProvider.h"
#include "ipc/IHostMethod.h"

#include <memory>

namespace dearoreui::facet {

class FacetHostMethod : public ipc::IHostMethod {
public:
    explicit FacetHostMethod(std::shared_ptr<IFacetProvider> provider, api::Permission requiredPermission);

    [[nodiscard]] std::string              name() const override;
    [[nodiscard]] api::Permission          requiredPermission() const override;
    [[nodiscard]] api::Result<std::string> execute(api::ContextId contextId, std::string_view args) override;

private:
    std::shared_ptr<IFacetProvider> mProvider;
    api::Permission                 mRequiredPermission;
};

} // namespace dearoreui::facet
