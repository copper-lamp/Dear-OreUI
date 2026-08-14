#pragma once

#include "registry/IModRegistry.h"
#include "ui/UiMountPlan.h"

#include <unordered_map>
#include <unordered_set>

namespace dearoreui::ui {

class UiPlanner {
public:
    explicit UiPlanner(registry::IModRegistry& registry);

    [[nodiscard]] UiMountPlan plan(api::ContextId contextId, api::PageScope scope) const;

private:
    [[nodiscard]] bool scopeMatches(api::PageScope target, std::vector<api::PageScope> const& scopes) const;

    [[nodiscard]] static std::vector<api::ModId> buildModOrder(registry::IModRegistry& registry);

    registry::IModRegistry& mRegistry;
};

} // namespace dearoreui::ui
