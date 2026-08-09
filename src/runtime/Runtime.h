#pragma once

#include "capability/StaticCapabilityQuery.h"
#include "runtime/IRuntime.h"
#include "runtime/RuntimeConfig.h"

namespace dearoreui::runtime {

class Runtime : public IRuntime {
public:
    explicit Runtime(RuntimeConfig config);

    [[nodiscard]] bool initialize() override;
    [[nodiscard]] bool enable() override;
    [[nodiscard]] bool disable() override;

    [[nodiscard]] diagnostic::DiagnosticLogger& diagnostics() override;
    [[nodiscard]] capability::ICapabilityQuery& capabilities() override;

private:
    RuntimeConfig                     mConfig;
    capability::StaticCapabilityQuery mCapabilities;
    bool                              mInitialized{false};
    bool                              mEnabled{false};
};

} // namespace dearoreui::runtime
