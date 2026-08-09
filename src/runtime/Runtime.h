#pragma once

#include "api/DearOreUIApi.h"
#include "capability/StaticCapabilityQuery.h"
#include "registry/ModRegistry.h"
#include "runtime/IRuntime.h"
#include "runtime/RuntimeConfig.h"

#include <memory>

namespace dearoreui::runtime {

class Runtime : public IRuntime {
public:
    explicit Runtime(RuntimeConfig config);

    [[nodiscard]] bool initialize() override;
    [[nodiscard]] bool enable() override;
    [[nodiscard]] bool disable() override;

    [[nodiscard]] diagnostic::DiagnosticLogger& diagnostics() override;
    [[nodiscard]] capability::ICapabilityQuery& capabilities() override;
    [[nodiscard]] api::IDearOreUIApi* api() override;

private:
    RuntimeConfig                     mConfig;
    capability::StaticCapabilityQuery mCapabilities;
    std::unique_ptr<registry::ModRegistry> mRegistry;
    std::unique_ptr<api::DearOreUIApi>     mApi;
    bool                              mInitialized{false};
    bool                              mEnabled{false};
};

} // namespace dearoreui::runtime
