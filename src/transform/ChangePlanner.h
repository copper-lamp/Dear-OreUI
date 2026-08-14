#pragma once

#include "registry/IModRegistry.h"
#include "transform/ChangePlan.h"
#include "transform/DependencyProblem.h"

namespace dearoreui::transform {

class ChangePlanner {
public:
    explicit ChangePlanner(registry::IModRegistry& registry);

    // Builds the change plan for one page: filter applicable entries, resolve
    // dependencies, detect conflicts, and apply deterministic ordering.
    [[nodiscard]] ChangePlan plan(api::ContextId contextId, api::PageScope scope) const;

private:
    [[nodiscard]] static ChangeOperation         toOperation(registry::RegistryEntry const& entry);
    [[nodiscard]] static bool                    scopeMatches(api::PageScope target, std::vector<api::PageScope> const& scopes);
    [[nodiscard]] static DependencyProblem::Kind problemKindFor(
        api::ModId                             owner,
        std::vector<DependencyProblem> const& problems
    );

    registry::IModRegistry& mRegistry;
};

} // namespace dearoreui::transform
