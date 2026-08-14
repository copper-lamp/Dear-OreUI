#pragma once

#include "api/types/Id.h"
#include "registry/RegistryEntry.h"
#include "transform/ChangePlan.h"

#include <vector>

namespace dearoreui::transform {

// The final, ordered set of changes ready to be registered into the resource index.
struct TransformedPage {
    api::ContextId                           contextId;
    std::vector<registry::ScriptEntry>       scripts;
    std::vector<registry::StyleSheetEntry>   styles;
    std::vector<registry::ResourceEntry>     resources;
    std::vector<registry::UiEntry>           uiEntries;
    ChangeReport                             report;
};

} // namespace dearoreui::transform
