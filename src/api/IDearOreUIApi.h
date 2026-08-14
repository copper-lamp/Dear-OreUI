#pragma once

#include "api/IHostApi.h"
#include "api/IResourceApi.h"
#include "api/IRuntimeApi.h"

namespace dearoreui::api {

class IDearOreUIApi : public IRuntimeApi, public IResourceApi, public IHostApi {
public:
    ~IDearOreUIApi() override = default;
};

} // namespace dearoreui::api
