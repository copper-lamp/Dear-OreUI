#pragma once

#include "hook/IPageHookCallback.h"
#include "capability/ICapabilityQuery.h"
#include "diagnostic/DiagnosticLogger.h"

namespace dearoreui::hook {

class OreUIHookAdapter {
public:
    OreUIHookAdapter(
        IPageHookCallback& callback,
        capability::ICapabilityQuery& capabilities,
        diagnostic::DiagnosticLogger& logger
    );
    ~OreUIHookAdapter();

    [[nodiscard]] bool install();
    [[nodiscard]] bool uninstall();

    [[nodiscard]] bool isInstalled() const;

private:
    IPageHookCallback&            mCallback;
    capability::ICapabilityQuery& mCapabilities;
    diagnostic::DiagnosticLogger& mLogger;
};

} // namespace dearoreui::hook
