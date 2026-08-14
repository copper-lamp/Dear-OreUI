#pragma once

#include "registry/ModRecord.h"
#include "transform/DependencyProblem.h"

#include <vector>

namespace dearoreui::transform {

// Resolves the dependency graph of registered mods.
// Input must be the set of ENABLED registered mods.
class DependencyResolver {
public:
    [[nodiscard]] static DependencyResolution resolve(std::vector<registry::ModRecord> const& records);

private:
    enum class RangeOp { Exact, GreaterEqual, Greater, LessEqual, Less, Compatible };

    [[nodiscard]] static bool rangeSyntaxValid(std::string_view range);
    [[nodiscard]] static bool satisfiesRange(std::string_view range, api::Version const& version);
    [[nodiscard]] static bool parseTerm(std::string_view term, RangeOp& op, api::Version& version);
};

} // namespace dearoreui::transform
