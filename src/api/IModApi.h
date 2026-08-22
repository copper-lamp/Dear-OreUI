#pragma once

#include "api/manifest/ModManifest.h"
#include "api/types/Id.h"
#include "api/types/Result.h"

namespace dearoreui::api {

class IModApi {
public:
    virtual ~IModApi() = default;

    // Mod-level registration and lifecycle. The mod must be registered before its
    // resources, scripts and stylesheets can be registered.
    [[nodiscard]] virtual Result<ModId> registerMod(ModManifest const& manifest) = 0;
    [[nodiscard]] virtual Result<void>  unregisterMod(ModId id)                  = 0;
    [[nodiscard]] virtual bool          isModRegistered(ModId id) const          = 0;
    [[nodiscard]] virtual bool          setModEnabled(ModId id, bool enabled)    = 0;
    [[nodiscard]] virtual bool          isModEnabled(ModId id) const             = 0;
};

} // namespace dearoreui::api
