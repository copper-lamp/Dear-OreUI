#pragma once

#include "api/types/Id.h"

#include <string>
#include <vector>

namespace dearoreui::transform {

struct DependencyProblem {
    enum class Kind { Missing, VersionMismatch, Cycle, InvalidRange };

    Kind                    kind{Kind::Missing};
    api::ModId              dependant;
    std::string             dependencyNamespace;
    std::string             versionRange;
    std::vector<api::ModId> cyclePath;
    std::string             message;
};

struct DependencyResolution {
    std::vector<api::ModId>        order;
    std::vector<DependencyProblem> problems;
};

} // namespace dearoreui::transform
