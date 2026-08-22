#pragma once

#include "api/IDearOreUIApi.h"

#include "capability/ICapabilityQuery.h"
#include "diagnostic/DiagnosticLogger.h"
#include "ipc/HostMethodRegistry.h"
#include "registry/IModRegistry.h"

#include <atomic>
#include <mutex>
#include <unordered_map>

namespace dearoreui::page {
class IPageContextManager;
}

namespace dearoreui::api {

class DearOreUIApi : public IDearOreUIApi {
public:
    DearOreUIApi(
        registry::IModRegistry&       registry,
        ipc::HostMethodRegistry&      hostMethodRegistry,
        capability::ICapabilityQuery& capabilities,
        diagnostic::DiagnosticLogger& logger,
        page::IPageContextManager& pageManager
    );

    [[nodiscard]] ApiInfo       getInfo() const override;
    [[nodiscard]] CapabilitySet getCapabilities() const override;
    [[nodiscard]] SupportLevel  checkSupport(Capability capability) const override;
    [[nodiscard]] std::uint32_t getProtocolVersion() const override;
    [[nodiscard]] bool          isReady() const override;

    [[nodiscard]] Result<RegistrationHandle>
    registerResource(ModId owner, ResourceManifest const& manifest, std::string payload) override;

    [[nodiscard]] Result<RegistrationHandle>
    registerScript(ModId owner, ScriptManifest const& manifest, std::string source) override;

    [[nodiscard]] Result<RegistrationHandle>
    registerStyleSheet(ModId owner, StyleSheetManifest const& manifest, std::string source) override;

    [[nodiscard]] Result<void> unregister(RegistrationHandle handle) override;

    [[nodiscard]] Result<ModId> registerMod(ModManifest const& manifest) override;
    [[nodiscard]] Result<void>  unregisterMod(ModId id) override;
    [[nodiscard]] bool          isModRegistered(ModId id) const override;
    [[nodiscard]] bool          setModEnabled(ModId id, bool enabled) override;
    [[nodiscard]] bool          isModEnabled(ModId id) const override;

    [[nodiscard]] Result<SubscriptionHandle> subscribePage(
        PageEvent event,
        PageCallback callback
    ) override;

    [[nodiscard]] Result<void> unsubscribePage(SubscriptionHandle handle) override;

    [[nodiscard]] Result<PageContextView> getPageContext(ContextId id) const override;

    [[nodiscard]] Result<RegistrationHandle>
    registerHostMethod(
        ModId owner,
        PermissionSet const& permissions,
        std::shared_ptr<ipc::IHostMethod> method
    ) override;

    [[nodiscard]] Result<void> unregisterHostMethod(RegistrationHandle handle) override;

    [[nodiscard]] Result<RegistrationHandle>
    registerOverlay(ModId owner, UiManifest const& manifest, std::string htmlBody) override;

    [[nodiscard]] Result<RegistrationHandle>
    registerPanel(ModId owner, UiManifest const& manifest, std::string htmlBody) override;

    [[nodiscard]] Result<RegistrationHandle>
    registerButton(ModId owner, UiManifest const& manifest, std::string htmlBody) override;

    [[nodiscard]] Result<RegistrationHandle>
    registerPage(ModId owner, UiManifest const& manifest, std::string htmlBody) override;

    [[nodiscard]] Result<RegistrationHandle>
    registerComponent(ModId owner, UiManifest const& manifest, component::ComponentSpec const& spec) override;

    [[nodiscard]] Result<void> unregisterUi(RegistrationHandle handle) override;

    void setReady(bool ready);


private:
    registry::IModRegistry&       mRegistry;
    ipc::HostMethodRegistry&      mHostMethodRegistry;
    capability::ICapabilityQuery& mCapabilities;
    diagnostic::DiagnosticLogger& mLogger;
    page::IPageContextManager&    mPageManager;

    mutable std::mutex mPageSubscriptionMutex;
    std::uint64_t      mNextSubscription{1};
    struct PageSubscription {
        PageEvent    event;
        PageCallback callback;
    };
    std::unordered_map<SubscriptionHandle, PageSubscription> mPageSubscriptions;
    std::atomic<bool> mReady{false};
};

} // namespace dearoreui::api
