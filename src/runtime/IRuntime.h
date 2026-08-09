#pragma once

#include "capability/ICapabilityQuery.h"
#include "diagnostic/DiagnosticLogger.h"

namespace dearoreui::runtime {

class IRuntime {
public:
    virtual ~IRuntime() = default;

    [[nodiscard]] virtual bool initialize() = 0;
    [[nodiscard]] virtual bool enable()     = 0;
    [[nodiscard]] virtual bool disable()    = 0;

    [[nodiscard]] virtual diagnostic::DiagnosticLogger& diagnostics() = 0;
    [[nodiscard]] virtual capability::ICapabilityQuery& capabilities() = 0;
};

} // namespace dearoreui::runtime
