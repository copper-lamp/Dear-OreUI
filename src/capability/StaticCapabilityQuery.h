#pragma once

#include "capability/ICapabilityQuery.h"

namespace dearoreui::capability {

class StaticCapabilityQuery : public ICapabilityQuery {
public:
    StaticCapabilityQuery();

    [[nodiscard]] api::SupportLevel  query(api::Capability capability) const override;
    [[nodiscard]] api::CapabilitySet all() const override;

    // Runtime upgrade/downgrade of a capability (e.g. HostBridge becomes
    // Experimental once a real Coherent view is captured).
    void setLevel(api::Capability capability, api::SupportLevel level, std::string note = {});

private:
    api::CapabilitySet mCapabilities;
};

} // namespace dearoreui::capability
