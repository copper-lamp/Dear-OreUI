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
    // already-mounted container. `chunkIndex` (1-based) paints a numbered
    // marker chip on success so in-game screenshots prove chunk execution.
    [[nodiscard]] std::string generateUiAppendScript(
        api::ContextId id,
        std::string const& containerId,
        std::vector<render::DomNode> const& nodes,
        std::size_t chunkIndex
    ) const;

    diagnostic::DiagnosticLogger& mLogger;
    ipc::IHostBridge&             mBridge;
    std::string                   mWsUrl;
};

} // namespace dearoreui::inject
