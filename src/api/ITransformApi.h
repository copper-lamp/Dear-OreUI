#pragma once

#include "api/types/Transform.h"

namespace dearoreui::api {

class ITransformApi {
public:
    virtual ~ITransformApi() = default;
    [[nodiscard]] virtual Result<TransformReport> previewTransform(TransformRequest request) const = 0;
};

} // namespace dearoreui::api
