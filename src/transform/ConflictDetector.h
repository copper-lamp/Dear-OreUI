#pragma once

#include "transform/ChangePlan.h"
#include "transform/Conflict.h"

#include <vector>

namespace dearoreui::transform {

class ConflictDetector {
public:
    // Scans pending operations and marks conflicting ones BlockedConflict.
    // Returns the detected conflicts. Must be called with a deterministic ordering
    // already applied to `operations`.
    [[nodiscard]] static std::vector<Conflict> detect(std::vector<ChangeOperation>& operations);
};

} // namespace dearoreui::transform
