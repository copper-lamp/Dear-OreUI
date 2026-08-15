#pragma once

#include "api/manifest/UiManifest.h"
#include "api/types/Id.h"
#include "api/types/Result.h"
#include "component/ComponentSpec.h"

#include <string>

namespace dearoreui::api {

class IUiApi {
public:
    virtual ~IUiApi() = default;

    [[nodiscard]] virtual Result<RegistrationHandle>
    registerOverlay(ModId owner, UiManifest const& manifest, std::string htmlBody) = 0;

    [[nodiscard]] virtual Result<RegistrationHandle>
    registerPanel(ModId owner, UiManifest const& manifest, std::string htmlBody) = 0;

    [[nodiscard]] virtual Result<RegistrationHandle>
    registerButton(ModId owner, UiManifest const& manifest, std::string htmlBody) = 0;

    [[nodiscard]] virtual Result<RegistrationHandle>
    registerPage(ModId owner, UiManifest const& manifest, std::string htmlBody) = 0;

    // Stage 8: register a declarative component. The component is rendered by
    // ComponentRenderer into an htmlBody and flows through the same
    // register/plan/mount/inject pipeline as raw htmlBody overlays.
    [[nodiscard]] virtual Result<RegistrationHandle>
    registerComponent(ModId owner, UiManifest const& manifest, component::ComponentSpec const& spec) = 0;

    [[nodiscard]] virtual Result<void> unregisterUi(RegistrationHandle handle) = 0;
};

} // namespace dearoreui::api
