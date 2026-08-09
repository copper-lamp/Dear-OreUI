#pragma once

#include "api/IRuntimeApi.h"
#include "api/IResourceApi.h"

namespace dearoreui::api {

class IDearOreUIApi : public IRuntimeApi, public IResourceApi {
public:
    ~IDearOreUIApi() override = default;
};

} // namespace dearoreui::api
