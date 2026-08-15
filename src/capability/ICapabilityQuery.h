#pragma once

#include "api/types/Capability.h"

namespace dearoreui::capability {

class ICapabilityQuery {
public:
    virtual ~ICapabilityQuery() = default;

    [[nodiscard]] virtual api::SupportLevel  query(api::Capability capability) const = 0;
    [[nodiscard]] virtual api::CapabilitySet all() const                             = 0;

    // Optional runtime upgrade/downgrade. Default no-op for read-only queries.
    virtual void setLevel(api::Capability, api::SupportLevel, std::string) {}
};

} // namespace dearoreui::capability
