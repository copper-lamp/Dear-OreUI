#pragma once

#include "api/IHostApi.h"
#include "api/IModApi.h"
#include "api/IResourceApi.h"
#include "api/IRuntimeApi.h"
#include "api/IUiApi.h"

namespace dearoreui::api {

class IDearOreUIApi
    : public IRuntimeApi,
      public IResourceApi,
      public IHostApi,
      public IModApi,
      public IUiApi {
public:
    ~IDearOreUIApi() override = default;
};

} // namespace dearoreui::api
