#pragma once

#include "api/types/Result.h"
#include "inject/InjectionReport.h"
#include "resource/IResourceIndex.h"
#include "ui/UiMountPlan.h"

namespace dearoreui::inject {

class IPageInjector {
public:
    virtual ~IPageInjector() = default;

    [[nodiscard]] virtual api::Result<InjectionReport>
    inject(api::ContextId id, resource::IResourceIndex const& index) = 0;

    [[nodiscard]] virtual api::Result<InjectionReport>
    injectUi(api::ContextId id, ui::UiMountPlan const& plan) = 0;
};

} // namespace dearoreui::inject
