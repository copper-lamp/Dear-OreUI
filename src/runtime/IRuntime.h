#pragma once

#include "api/IDearOreUIApi.h"
#include "capability/ICapabilityQuery.h"
#include "diagnostic/DiagnosticLogger.h"
#include "page/IPageContextManager.h"

namespace dearoreui::runtime {

class IRuntime {
public:
    virtual ~IRuntime() = default;

    [[nodiscard]] virtual bool initialize() = 0;
    [[nodiscard]] virtual bool enable()     = 0;
    [[nodiscard]] virtual bool disable()    = 0;

    [[nodiscard]] virtual diagnostic::DiagnosticLogger& diagnostics() = 0;
    [[nodiscard]] virtual capability::ICapabilityQuery& capabilities() = 0;
    [[nodiscard]] virtual api::IDearOreUIApi* api() = 0;
    [[nodiscard]] virtual page::IPageContextManager* pageManager() = 0;
};

} // namespace dearoreui::runtime
