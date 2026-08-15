#pragma once

#include "diagnostic/DiagnosticLogger.h"
#include "inject/IPageInjector.h"
#include "ipc/IHostBridge.h"

namespace dearoreui::inject {

class RuntimeInjector : public IPageInjector {
public:
    // wsUrl (ws://127.0.0.1:PORT/dearoreui?token=...) is the Stage 8 JS->C++
    // loopback endpoint. When empty the generated runtime script degrades to
    // the unavailable stub (tests / missing server).
    RuntimeInjector(
        diagnostic::DiagnosticLogger& logger,
        ipc::IHostBridge&             bridge,
        std::string                   wsUrl = {}
    );

    [[nodiscard]] api::Result<InjectionReport>
    inject(api::ContextId id, resource::IResourceIndex const& index) override;

    [[nodiscard]] api::Result<InjectionReport>
    injectUi(api::ContextId id, ui::UiMountPlan const& plan) override;

private:
    [[nodiscard]] std::string generateRuntimeScript(api::ContextId id, resource::IResourceIndex const& index) const;
    [[nodiscard]] std::string generateUiBootstrapScript(api::ContextId id, ui::UiMountPlan const& plan) const;

    diagnostic::DiagnosticLogger& mLogger;
    ipc::IHostBridge&             mBridge;
    std::string                   mWsUrl;
};

} // namespace dearoreui::inject
