#pragma once

#include "api/types/ApiInfo.h"
#include "api/types/Capability.h"

#include <cstdint>

namespace dearoreui::api {

class IRuntimeApi {
public:
    virtual ~IRuntimeApi() = default;

    [[nodiscard]] virtual ApiInfo getInfo() const        = 0;
    [[nodiscard]] virtual CapabilitySet getCapabilities() const = 0;
    [[nodiscard]] virtual SupportLevel checkSupport(Capability capability) const = 0;
    [[nodiscard]] virtual std::uint32_t getProtocolVersion() const = 0;
    [[nodiscard]] virtual bool isReady() const           = 0;
};

} // namespace dearoreui::api
