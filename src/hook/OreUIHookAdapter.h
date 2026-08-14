#pragma once

#include "capability/ICapabilityQuery.h"
#include "diagnostic/DiagnosticLogger.h"
#include "hook/IPageHookCallback.h"

#include <filesystem>

namespace dearoreui::hook {

class OreUIHookAdapter {
public:
    OreUIHookAdapter(
        IPageHookCallback&            callback,
        capability::ICapabilityQuery& capabilities,
        diagnostic::DiagnosticLogger& logger,
        std::filesystem::path         dataDirectory = {}
    );
    ~OreUIHookAdapter();

    [[nodiscard]] bool install();
    [[nodiscard]] bool uninstall();

    [[nodiscard]] bool isInstalled() const;

private:
    IPageHookCallback&            mCallback;
    capability::ICapabilityQuery& mCapabilities;
    diagnostic::DiagnosticLogger& mLogger;
    std::filesystem::path         mDataDirectory;
};

} // namespace dearoreui::hook
