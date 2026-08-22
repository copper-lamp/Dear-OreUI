#pragma once

#include "api/IDearOreUIApi.h"

namespace dearoreui::bridge {

// Internal (non-public) registration points. Called by the mod entry after
// Runtime::enable() succeeds / before disable() so external Mods can query the
// ready API instance through the pure C bridge (DearOreUIBridge.h).
void setApi(api::IDearOreUIApi* api);
void clearApi();

} // namespace dearoreui::bridge