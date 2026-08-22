#include "api/IDearOreUIApi.h"
#include "api/IHostMethod.h"
#include "api/types/ComponentSpec.h"
#include "api/types/HostMethodManifest.h"
#include "api/types/Page.h"

#include <memory>
#include <string>

namespace dearoreui::examples {

class BatchMethod final : public api::IHostMethod {
public:
    [[nodiscard]] std::string name() const override { return "example.batch.initialize"; }
    [[nodiscard]] api::Permission requiredPermission() const override { return api::Permission::HostReadOnly; }
    [[nodiscard]] api::Result<std::string> execute(api::ContextId, std::string_view args) override {
        return std::string{"{\"accepted\":true,\"request\":"} + std::string{args} + "}";
    }
};

// This function is intentionally written against Public API only. A host Mod
// supplies IDearOreUIApi obtained from Runtime::api() or its ABI adapter.
api::Result<void> registerExternalMod(api::IDearOreUIApi& oreui) {
    api::ModManifest manifest;
    manifest.id = api::ModId{"example.external_mod"};
    manifest.modNamespace = "example.external_mod";
    manifest.displayName = "External Mod Example";
    manifest.modVersion = api::Version{1, 0, 0};
    manifest.permissions = {api::Permission::HostReadOnly, api::Permission::PageObserve, api::Permission::UiMount};

    auto mod = oreui.registerMod(manifest);
    if (mod.isErr()) return mod.error();

    // Declarative component through the Public ComponentSpec value object.
    api::ComponentSpec badge;
    badge.kind    = api::ComponentKind::Badge;
    badge.label   = "ready";
    badge.variant = "primary";

    api::UiManifest ui;
    ui.modNamespace = manifest.modNamespace;
    ui.id           = "status_badge";
    ui.pageScopes   = {api::PageScope::InGame};
    ui.fingerprint  = "example.status_badge.1";
    auto uiHandle = oreui.registerComponent(manifest.id, ui, badge);
    if (uiHandle.isErr()) return uiHandle.error();

    api::HostMethodManifest hostManifest;
    hostManifest.name = "example.batch.initialize";
    hostManifest.permissions.grant(api::Permission::HostReadOnly);
    hostManifest.pageScopes = {api::PageScope::InGame};
    hostManifest.maxRequestBytes = 32 * 1024;
    hostManifest.maxResponseBytes = 64 * 1024;

    auto hostHandle = oreui.registerHostMethod(
        manifest.id, hostManifest, std::make_shared<BatchMethod>()
    );
    if (hostHandle.isErr()) return hostHandle.error();

    api::PageSubscriptionOptions options;
    options.owner = manifest.id;
    options.scopes = {api::PageScope::InGame};
    auto subscription = oreui.subscribePage(
        std::move(options), api::PageEvent::Ready,
        [&oreui, owner = manifest.id](api::PageContextView const& page) {
            // One View: this is the only JS→C++ business dispatch.
            // Batch all initialization commands into this single request.
            api::EventPublishOptions event;
            event.owner = owner;
            event.context = page.id;
            event.name = "example.state.ready";
            event.payload = "{\"initialized\":true}";
            static_cast<void>(oreui.publishEvent(std::move(event)));
        }
    );
    if (subscription.isErr()) return subscription.error();

    return api::Result<void>::success();
}

} // namespace dearoreui::examples