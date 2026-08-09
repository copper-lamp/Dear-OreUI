#pragma once

#include "capability/ICapabilityQuery.h"

namespace dearoreui::capability {

class StaticCapabilityQuery : public ICapabilityQuery {
public:
    StaticCapabilityQuery();

    [[nodiscard]] api::SupportLevel query(api::Capability capability) const override;
    [[nodiscard]] api::CapabilitySet all() const override;

private:
    api::CapabilitySet mCapabilities;
};

} // namespace dearoreui::capability
