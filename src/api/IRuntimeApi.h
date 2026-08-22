#pragma once

#include "api/types/ApiInfo.h"
#include "api/types/Capability.h"
#include "api/types/Compatibility.h"

#include <cstdint>

namespace dearoreui::api {

class IRuntimeApi {
public:
    virtual ~IRuntimeApi() = default;

    [[nodiscard]] virtual ApiInfo             getInfo() const                           = 0;
    [[nodiscard]] virtual CapabilitySet       getCapabilities() const                   = 0;
    [[nodiscard]] virtual SupportLevel        checkSupport(Capability capability) const = 0;
    [[nodiscard]] virtual std::uint32_t       getProtocolVersion() const                = 0;
    [[nodiscard]] virtual bool                isReady() const                           = 0;
    [[nodiscard]] virtual CompatibilityReport checkCompatibility(CompatibilityRequirement const& requirement) const {
        static_cast<void>(requirement);
        return CompatibilityReport{};
    }
};

} // namespace dearoreui::api
