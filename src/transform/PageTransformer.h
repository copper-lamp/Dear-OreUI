#pragma once

#include "source/PageSourceSnapshot.h"
#include "transform/ChangePlan.h"
#include "transform/TransformedPage.h"

namespace dearoreui::transform {

// Materializes a ChangePlan into the final ordered set of entries (TransformedPage)
// and produces the ChangeReport consumed by diagnostics and the injector.
class PageTransformer {
public:
    PageTransformer() = default;

    [[nodiscard]] TransformedPage transform(ChangePlan const& plan, source::PageSourceSnapshot const& snapshot) const;
};

} // namespace dearoreui::transform
