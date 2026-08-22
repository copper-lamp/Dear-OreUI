#pragma once

#include "api/types/Error.h"
#include "api/types/Id.h"
#include "api/types/Result.h"
#include "ui/OverlaySpec.h"

namespace dearoreui::ui {

class IMountHost {
public:
    virtual ~IMountHost() = default;

    [[nodiscard]] virtual api::Result<void> createContainer(api::ContextId contextId, OverlaySpec const& spec) = 0;

    [[nodiscard]] virtual api::Result<void> removeContainer(api::ContextId contextId, std::string_view containerId) = 0;

    [[nodiscard]] virtual bool isAvailable() const = 0;
};

} // namespace dearoreui::ui
