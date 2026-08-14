#pragma once

#include "api/types/Result.h"
#include "inject/InjectionReport.h"
#include "resource/IResourceIndex.h"

namespace dearoreui::inject {

class IPageInjector {
public:
    virtual ~IPageInjector() = default;

    [[nodiscard]] virtual api::Result<InjectionReport>
    inject(api::ContextId id, resource::IResourceIndex const& index) = 0;
};

} // namespace dearoreui::inject
