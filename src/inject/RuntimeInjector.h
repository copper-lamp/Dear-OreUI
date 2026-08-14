#pragma once

#include "diagnostic/DiagnosticLogger.h"
#include "inject/IPageInjector.h"

namespace dearoreui::inject {

class RuntimeInjector : public IPageInjector {
public:
    explicit RuntimeInjector(diagnostic::DiagnosticLogger& logger);

    [[nodiscard]] api::Result<InjectionReport>
    inject(api::ContextId id, resource::IResourceIndex const& index) override;

private:
    [[nodiscard]] std::string generateRuntimeScript(
        api::ContextId id, resource::IResourceIndex const& index
    ) const;

    diagnostic::DiagnosticLogger& mLogger;
};

} // namespace dearoreui::inject
