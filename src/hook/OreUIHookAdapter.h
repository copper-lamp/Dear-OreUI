#pragma once

#include "capability/ICapabilityQuery.h"
#include "diagnostic/DiagnosticLogger.h"
#include "hook/IPageHookCallback.h"
#include "hook/Stage5CoherentProbe.h"
#include "ipc/CoherentViewRegistry.h"

#include <filesystem>
#include <functional>

namespace dearoreui::hook {

class OreUIHookAdapter {
public:
    OreUIHookAdapter(
        IPageHookCallback&            callback,
        capability::ICapabilityQuery& capabilities,
        ipc::CoherentViewRegistry&    viewRegistry,
        diagnostic::DiagnosticLogger& logger,
        std::filesystem::path         dataDirectory = {}
    );
    ~OreUIHookAdapter();

    [[nodiscard]] bool install();
    [[nodiscard]] bool uninstall();

    [[nodiscard]] bool isInstalled() const;

    // Stage 8-A: invoked by the createFacetRegistry hook for every fresh
    // IFacetRegistry (void* = OreUI::IFacetRegistry*). Runtime wires this to
    // OreUIFacetBridge so the "dearoreui" facet can be registered.
    void setOnFacetRegistryCreated(std::function<void(void*)> callback);

private:
    IPageHookCallback&            mCallback;
    capability::ICapabilityQuery& mCapabilities;
    ipc::CoherentViewRegistry&    mViewRegistry;
    diagnostic::DiagnosticLogger& mLogger;
    std::filesystem::path         mDataDirectory;
    Stage5CoherentProbe           mProbe;
};

} // namespace dearoreui::hook
