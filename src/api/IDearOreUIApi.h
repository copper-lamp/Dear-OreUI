#pragma once

#include "api/IHostApi.h"
#include "api/IDiagnosticApi.h"
#include "api/IEventApi.h"
#include "api/ITransformApi.h"
#include "api/IModApi.h"
#include "api/IPageApi.h"
#include "api/IResourceApi.h"
#include "api/IRuntimeApi.h"
#include "api/IUiApi.h"

namespace dearoreui::api {

class IDearOreUIApi
    : public IRuntimeApi,
      public IResourceApi,
      public IHostApi,
      public IDiagnosticApi,
      public IEventApi,
      public ITransformApi,
      public IModApi,
      public IPageApi,
      public IUiApi {
public:
    ~IDearOreUIApi() override = default;
};

} // namespace dearoreui::api



