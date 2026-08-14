#pragma once

#include "diagnostic/DiagnosticLogger.h"
#include "inject/IPageInjector.h"
#include "ipc/IHostBridge.h"

namespace dearoreui::inject {

class RuntimeInjector : public IPageInjector {
public:
    RuntimeInjector(diagnostic::DiagnosticLogger& logger, ipc::IHostBridge& bridge);

    [[nodiscard]] api::Result<InjectionReport>
    inject(api::ContextId id, resource::IResourceIndex const& index) override;

private:
    [[nodiscard]] std::string generateRuntimeScript(api::ContextId id, resource::IResourceIndex const& index) const;

    diagnostic::DiagnosticLogger& mLogger;
    ipc::IHostBridge&             mBridge;
};

} // namespace dearoreui::inject
