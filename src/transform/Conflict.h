#pragma once

#include "api/types/Id.h"

#include <string>
#include <vector>

namespace dearoreui::transform {

enum class ConflictKind {
    ResourceOwnership,
    DeclaredConflict,
    OrderCycle,
};

struct Conflict {
    ConflictKind             kind{ConflictKind::ResourceOwnership};
    std::string              description;
    std::vector<api::ModId>  involvedMods;
    std::vector<std::string> involvedPaths;
};

} // namespace dearoreui::transform
