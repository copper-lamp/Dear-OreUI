#pragma once

#include "diagnostic/DiagnosticEvent.h"

namespace dearoreui::diagnostic {

class IDiagnosticSink {
public:
    virtual ~IDiagnosticSink() = default;

    virtual void consume(DiagnosticEvent const& event) = 0;

    virtual void flush() {}
};

} // namespace dearoreui::diagnostic
