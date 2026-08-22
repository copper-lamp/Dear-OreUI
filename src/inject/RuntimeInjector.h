#pragma once

#include "diagnostic/DiagnosticLogger.h"
#include "inject/IPageInjector.h"
#include "ipc/IHostBridge.h"

namespace dearoreui::inject {

class RuntimeInjector : public IPageInjector {
public:
    // JS->C++ uses the game's native Facet protocol. The C++ bridge remains
    // responsible for ExecuteScript and response delivery.
    RuntimeInjector(
        diagnostic::DiagnosticLogger& logger,
        ipc::IHostBridge&             bridge
    );

    [[nodiscard]] api::Result<InjectionReport>
    inject(api::ContextId id, resource::IResourceIndex const& index) override;

    [[nodiscard]] api::Result<InjectionReport>
    injectUi(api::ContextId id, ui::UiMountPlan const& plan) override;

private:
    [[nodiscard]] std::string generateRuntimeScript(api::ContextId id, resource::IResourceIndex const& index) const;
    // Generates a self-contained bootstrap script for ONE UI item (machinery +
    // spec). Injecting per-UI keeps each ExecuteScript small (cohtml silently
    // drops large ones).
    [[nodiscard]] std::string generateUiBootstrapScript(api::ContextId id, ui::UiMountItem const& item) const;
    // Body-only follow-up for UIs after the first: reuses the machinery already
    // injected by the bootstrap script so each additional UI stays small.
    // `bodyOverride` (optional) replaces the spec's body for chunked injection.
    [[nodiscard]] std::string generateUiBodyScript(
        api::ContextId id,
        ui::UiMountItem const& item,
        std::vector<render::DomNode> const* bodyOverride = nullptr
    ) const;
    // Append-only script for chunked UIs: appends a node forest into an
    // already-mounted container (target captured by mount as
    // container.__dearOreUiRoot).
    [[nodiscard]] std::string generateUiAppendScript(
        api::ContextId id,
        std::string const& containerId,
        std::vector<render::DomNode> const& nodes
    ) const;

    diagnostic::DiagnosticLogger& mLogger;
    ipc::IHostBridge&             mBridge;
};

} // namespace dearoreui::inject
