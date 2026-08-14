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

    [[nodiscard]] api::Result<InjectionReport>
    injectUi(api::ContextId id, ui::UiMountPlan const& plan) override;

private:
    [[nodiscard]] std::string generateRuntimeScript(api::ContextId id, resource::IResourceIndex const& index) const;
    [[nodiscard]] std::string generateUiBootstrapScript(api::ContextId id, ui::UiMountPlan const& plan) const;

    diagnostic::DiagnosticLogger& mLogger;
    ipc::IHostBridge&             mBridge;
};

} // namespace dearoreui::inject
