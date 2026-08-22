#include "api/DearOreUIApi.h"

#include "api/manifest/ManifestValidator.h"
#include "component/ComponentRenderer.h"
#include "diagnostic/Stage6TransformTelemetry.h"
#include "diagnostic/Stage7UiTelemetry.h"
#include "page/IPageContextManager.h"
#include "registry/ModRecord.h"
#include "registry/RegistryEntry.h"
#include "resource/ResourceUri.h"

#include <algorithm>
#include <chrono>
#include <type_traits>

namespace dearoreui::api {

DearOreUIApi::DearOreUIApi(
    registry::IModRegistry&       registry,
    ipc::HostMethodRegistry&      hostMethodRegistry,
    capability::ICapabilityQuery& capabilities,
    diagnostic::DiagnosticLogger& logger,
    page::IPageContextManager*    pageManager
)
: mRegistry(registry),
  mHostMethodRegistry(hostMethodRegistry),
  mCapabilities(capabilities),
  mLogger(logger),
  mPageManager(pageManager) {}

ApiInfo DearOreUIApi::getInfo() const {
    ApiInfo info;
    info.protocolVersion = getProtocolVersion();
    info.modVersion      = Version{0, 2, 0};
    info.runtimeState    = isReady() ? "ready" : "initializing";
    info.ready           = isReady();
    return info;
}

CapabilitySet DearOreUIApi::getCapabilities() const { return mCapabilities.all(); }

SupportLevel DearOreUIApi::checkSupport(Capability capability) const { return mCapabilities.query(capability); }

std::uint32_t DearOreUIApi::getProtocolVersion() const { return DearOreUIProtocolVersion; }

bool DearOreUIApi::isReady() const { return mReady.load(std::memory_order_relaxed); }

CompatibilityReport DearOreUIApi::checkCompatibility(CompatibilityRequirement const& requirement) const {
    auto                info = getInfo();
    CompatibilityReport report;
    report.protocolVersion  = info.protocolVersion;
    report.oreuiVersion     = info.modVersion;
    report.minecraftVersion = info.minecraftVersion;
    report.coherentVersion  = info.coherentVersion;
    report.pageScope        = requirement.pageScope;
    report.fingerprint      = requirement.fingerprint;

    if (requirement.protocolVersion != 0 && requirement.protocolVersion != info.protocolVersion) {
        report.status = CompatibilityStatus::Unsupported;
        report.reasons.push_back("protocol version mismatch");
    } else if (!requirement.oreuiVersion.toString().empty() && !info.modVersion.satisfies(requirement.oreuiVersion)) {
        report.status = CompatibilityStatus::Unsupported;
        report.reasons.push_back("OreUI version does not satisfy requirement");
    } else if (!requirement.minecraftVersion.empty() && info.minecraftVersion.empty()) {
        report.status = CompatibilityStatus::Unknown;
        report.warnings.push_back("Minecraft version is not available");
    } else if (!requirement.coherentVersion.empty() && info.coherentVersion.empty()) {
        report.status = CompatibilityStatus::Unknown;
        report.warnings.push_back("Coherent version is not available");
    } else {
        report.status = CompatibilityStatus::Compatible;
    }
    return report;
}

void DearOreUIApi::setReady(bool ready) { mReady.store(ready, std::memory_order_relaxed); }

void DearOreUIApi::setPageManager(page::IPageContextManager* pageManager) { mPageManager = pageManager; }

void DearOreUIApi::notifyPage(PageEvent event, PageContextView const& context) {
    std::vector<PageCallback> callbacks;
    {
        std::lock_guard lock(mPageSubscriptionMutex);
        for (auto const& [handle, subscription] : mPageSubscriptions) {
            static_cast<void>(handle);
            if (subscription.event != event) continue;
            if (!subscription.scopes.empty()
                && std::find(subscription.scopes.begin(), subscription.scopes.end(), context.page.scope)
                       == subscription.scopes.end()) {
                continue;
            }
            callbacks.push_back(subscription.callback);
        }
    }
    for (auto const& callback : callbacks) {
        try {
            callback(context);
        } catch (...) {
            mLogger.error("api", "page_callback_failed").withContext(context.id).emit();
        }
    }
}

Result<SubscriptionHandle>
DearOreUIApi::subscribePage(PageSubscriptionOptions options, PageEvent event, PageCallback callback) {
    if (!options.owner.isValid() || !mRegistry.isModRegistered(options.owner)) {
        return Error{ErrorCode::InvalidArgument, "subscription owner mod is not registered"};
    }
    if (!callback) {
        return Error{ErrorCode::InvalidArgument, "page callback is empty"};
    }
    auto handle = SubscriptionHandle{mNextSubscription++};
    {
        std::lock_guard lock(mPageSubscriptionMutex);
        mPageSubscriptions.emplace(
            handle,
            PageSubscription{options.owner, std::move(options.scopes), event, std::move(callback)}
        );
    }
    return handle;
}

Result<void> DearOreUIApi::unsubscribePage(SubscriptionHandle handle) {
    std::lock_guard lock(mPageSubscriptionMutex);
    // Unsubscribe is intentionally idempotent: cleanup paths may race with
    // explicit Mod shutdown without turning a successful cleanup into an error.
    mPageSubscriptions.erase(handle);
    return Result<void>::success();
}

Result<PageContextView> DearOreUIApi::getPageContext(ContextId id) const {
    if (mPageManager == nullptr) return Error{ErrorCode::InvalidState, "page manager unavailable"};
    auto context = mPageManager->find(id);
    if (!context) return Error{ErrorCode::InvalidContext, "page context not found"};
    return PageContextView{id, context->page};
}

Result<RegistrationHandle>
DearOreUIApi::registerResource(ModId owner, ResourceManifest const& manifest, std::string payload) {
    if (!owner.isValid()) {
        return Error{ErrorCode::InvalidArgument, "owner is invalid"};
    }
    if (!mRegistry.isModRegistered(owner)) {
        return Error{ErrorCode::InvalidArgument, "owner mod is not registered"};
    }
    if (manifest.modNamespace != owner.value()) {
        return Error{ErrorCode::InvalidArgument, "manifest namespace does not match owner"};
    }

    auto validation = ManifestValidator::validate(manifest);
    if (validation.isErr()) {
        mLogger.warning("manifest", "validation_failed")
            .withMod(owner)
            .withError(validation.error().code)
            .withMessage(validation.error().message)
            .emit();
        return validation.error();
    }

    if (mRegistry.hasConflict(manifest)) {
        return Error{ErrorCode::ResourceConflict, "resource path conflict"};
    }

    registry::ResourceEntry entry;
    entry.owner    = owner;
    entry.manifest = manifest;
    entry.payload  = std::move(payload);

    auto result = mRegistry.insert(std::move(entry));
    if (result.isErr()) {
        mLogger.warning("registry", "resource_registration_failed")
            .withMod(owner)
            .withError(result.error().code)
            .withMessage(result.error().message)
            .emit();
        return result.error();
    }

    mLogger.info("registry", "resource_registered")
        .withMod(owner)
        .withField("handle", std::to_string(result.value().value()))
        .withField("namespace", manifest.modNamespace)
        .withField("path", manifest.path)
        .emit();
    return result.value();
}

Result<RegistrationHandle>
DearOreUIApi::registerScript(ModId owner, ScriptManifest const& manifest, std::string source) {
    if (!owner.isValid()) {
        return Error{ErrorCode::InvalidArgument, "owner is invalid"};
    }
    if (!mRegistry.isModRegistered(owner)) {
        return Error{ErrorCode::InvalidArgument, "owner mod is not registered"};
    }
    if (manifest.modNamespace != owner.value()) {
        return Error{ErrorCode::InvalidArgument, "manifest namespace does not match owner"};
    }

    auto validation = ManifestValidator::validate(manifest);
    if (validation.isErr()) {
        mLogger.warning("manifest", "validation_failed")
            .withMod(owner)
            .withError(validation.error().code)
            .withMessage(validation.error().message)
            .emit();
        return validation.error();
    }

    api::ResourceManifest conflictProbe;
    conflictProbe.modNamespace = manifest.modNamespace;
    conflictProbe.path         = manifest.path;
    if (mRegistry.hasConflict(conflictProbe)) {
        return Error{ErrorCode::ResourceConflict, "script path conflict"};
    }

    registry::ScriptEntry entry;
    entry.owner    = owner;
    entry.manifest = manifest;
    entry.source   = std::move(source);

    auto result = mRegistry.insert(std::move(entry));
    if (result.isErr()) {
        mLogger.warning("registry", "script_registration_failed")
            .withMod(owner)
            .withError(result.error().code)
            .withMessage(result.error().message)
            .emit();
        return result.error();
    }

    mLogger.info("registry", "script_registered")
        .withMod(owner)
        .withField("handle", std::to_string(result.value().value()))
        .withField("namespace", manifest.modNamespace)
        .withField("path", manifest.path)
        .emit();
    return result.value();
}

Result<RegistrationHandle>
DearOreUIApi::registerStyleSheet(ModId owner, StyleSheetManifest const& manifest, std::string source) {
    if (!owner.isValid()) {
        return Error{ErrorCode::InvalidArgument, "owner is invalid"};
    }
    if (!mRegistry.isModRegistered(owner)) {
        return Error{ErrorCode::InvalidArgument, "owner mod is not registered"};
    }
    if (manifest.modNamespace != owner.value()) {
        return Error{ErrorCode::InvalidArgument, "manifest namespace does not match owner"};
    }

    auto validation = ManifestValidator::validate(manifest);
    if (validation.isErr()) {
        mLogger.warning("manifest", "validation_failed")
            .withMod(owner)
            .withError(validation.error().code)
            .withMessage(validation.error().message)
            .emit();
        return validation.error();
    }

    api::ResourceManifest conflictProbe;
    conflictProbe.modNamespace = manifest.modNamespace;
    conflictProbe.path         = manifest.path;
    if (mRegistry.hasConflict(conflictProbe)) {
        return Error{ErrorCode::ResourceConflict, "stylesheet path conflict"};
    }

    registry::StyleSheetEntry entry;
    entry.owner    = owner;
    entry.manifest = manifest;
    entry.source   = std::move(source);

    auto result = mRegistry.insert(std::move(entry));
    if (result.isErr()) {
        mLogger.warning("registry", "stylesheet_registration_failed")
            .withMod(owner)
            .withError(result.error().code)
            .withMessage(result.error().message)
            .emit();
        return result.error();
    }

    mLogger.info("registry", "stylesheet_registered")
        .withMod(owner)
        .withField("handle", std::to_string(result.value().value()))
        .withField("namespace", manifest.modNamespace)
        .withField("path", manifest.path)
        .emit();
    return result.value();
}

Result<void> DearOreUIApi::unregister(RegistrationHandle handle) {
    if (!handle.isValid()) {
        return Error{ErrorCode::InvalidArgument, "handle is invalid"};
    }

    bool removed = mRegistry.remove(handle);
    if (!removed) {
        return Error{ErrorCode::NotFound, "registration handle not found"};
    }

    mLogger.info("registry", "unregistered").withField("handle", std::to_string(handle.value())).emit();
    return Result<void>::success();
}

Result<ResourceInfo> DearOreUIApi::describeResource(ModId requester, std::string_view uri) const {
    if (!requester.isValid() || !mRegistry.isModRegistered(requester)) return Error{ErrorCode::PermissionDenied, "requester mod is not registered"};
    auto requesterRecord = mRegistry.findMod(requester);
    if (!requesterRecord || std::find(requesterRecord->manifest.permissions.begin(), requesterRecord->manifest.permissions.end(), Permission::ResourceRead) == requesterRecord->manifest.permissions.end()) {
        return Error{ErrorCode::PermissionDenied, "resource.read permission is required"};
    }
    auto parsed = resource::ResourceUri::parse(uri);
    if (parsed.isErr()) return parsed.error();
    if (parsed.value().modNamespace != requester.value()) return Error{ErrorCode::PermissionDenied, "cross-namespace resource access denied"};
    for (auto const& entry : mRegistry.listEntries()) {
        ResourceInfo info;
        bool match = false;
        std::visit([&](auto const& item) {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, registry::ResourceEntry>) {
                info = ResourceInfo{resource::ResourceUri{resource::ResourceUriScheme::Resource, item.manifest.modNamespace, item.manifest.path}.toString(), std::string{resourceKindName(item.manifest.kind)}, item.payload.size(), item.manifest.pageScopes.empty() ? PageScope::Any : item.manifest.pageScopes.front()}; match = info.uri == uri;
            } else if constexpr (std::is_same_v<T, registry::ScriptEntry>) {
                info = ResourceInfo{resource::ResourceUri{resource::ResourceUriScheme::Script, item.manifest.modNamespace, item.manifest.path}.toString(), "text/javascript", item.source.size(), item.manifest.pageScopes.empty() ? PageScope::Any : item.manifest.pageScopes.front()}; match = info.uri == uri;
            } else if constexpr (std::is_same_v<T, registry::StyleSheetEntry>) {
                info = ResourceInfo{resource::ResourceUri{resource::ResourceUriScheme::Style, item.manifest.modNamespace, item.manifest.path}.toString(), "text/css", item.source.size(), item.manifest.pageScopes.empty() ? PageScope::Any : item.manifest.pageScopes.front()}; match = info.uri == uri;
            }
        }, entry);
        if (match) return info;
    }
    return Error{ErrorCode::ResourceNotFound, "resource not found"};
}

Result<ResourceBytes> DearOreUIApi::readResource(ModId requester, std::string_view uri, ResourceReadOptions options) const {
    auto info = describeResource(requester, uri);
    if (info.isErr()) return info.error();
    if (options.maxBytes == 0 || options.maxBytes > 1024 * 1024) return Error{ErrorCode::InvalidArgument, "resource read limit is invalid"};
    for (auto const& entry : mRegistry.listEntries()) {
        ResourceBytes bytes;
        bool match = false;
        std::visit([&](auto const& item) {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, registry::ResourceEntry>) { bytes.data = item.payload; match = info.value().uri == uri; }
            else if constexpr (std::is_same_v<T, registry::ScriptEntry>) { bytes.data = item.source; match = info.value().uri == uri; }
            else if constexpr (std::is_same_v<T, registry::StyleSheetEntry>) { bytes.data = item.source; match = info.value().uri == uri; }
        }, entry);
        if (match) { if (bytes.data.size() > options.maxBytes) return Error{ErrorCode::InvalidArgument, "resource exceeds requested limit"}; bytes.info = info.value(); return bytes; }
    }
    return Error{ErrorCode::ResourceNotFound, "resource not found"};
}

Result<ModId> DearOreUIApi::registerMod(ModManifest const& manifest) {
    auto validation = ManifestValidator::validate(manifest);
    if (validation.isErr()) {
        mLogger.warning("manifest", "mod_validation_failed")
            .withMod(manifest.id)
            .withError(validation.error().code)
            .withMessage(validation.error().message)
            .emit();
        return validation.error();
    }

    registry::ModRecord record;
    record.manifest = manifest;

    auto result = mRegistry.registerMod(std::move(record));
    if (result.isErr()) {
        mLogger.warning("registry", "mod_registration_failed")
            .withMod(manifest.id)
            .withError(result.error().code)
            .withMessage(result.error().message)
            .emit();
        return result.error();
    }

    diagnostic::recordStage6ModRegistered(result.value(), manifest.modNamespace, manifest.dependencies.size());
    return result.value();
}

Result<void> DearOreUIApi::unregisterMod(ModId id) {
    if (!id.isValid()) {
        return Error{ErrorCode::InvalidArgument, "mod id is invalid"};
    }

    auto mod = mRegistry.findMod(id);
    if (!mod.has_value()) {
        return Error{ErrorCode::NotFound, "mod is not registered"};
    }

    std::size_t removedEntries = mRegistry.findByOwner(id).size();
    {
        std::lock_guard lock(mPageSubscriptionMutex);
        for (auto it = mPageSubscriptions.begin(); it != mPageSubscriptions.end();) {
            it = it->second.owner == id ? mPageSubscriptions.erase(it) : std::next(it);
        }
    }
    bool removed = mRegistry.unregisterMod(id);
    if (!removed) {
        return Error{ErrorCode::NotFound, "mod is not registered"};
    }

    mLogger.info("registry", "mod_unregistered")
        .withMod(id)
        .withField("removed_entry_count", std::to_string(removedEntries))
        .emit();
    diagnostic::recordStage6ModUnregistered(id, removedEntries);
    return Result<void>::success();
}

bool DearOreUIApi::isModRegistered(ModId id) const { return mRegistry.isModRegistered(id); }

bool DearOreUIApi::setModEnabled(ModId id, bool enabled) { return mRegistry.setModEnabled(id, enabled); }

bool DearOreUIApi::isModEnabled(ModId id) const { return mRegistry.isModEnabled(id); }

Result<RegistrationHandle> DearOreUIApi::registerHostMethod(
    ModId                             owner,
    PermissionSet const&              permissions,
    std::shared_ptr<ipc::IHostMethod> method
) {
    if (!owner.isValid()) {
        return Error{ErrorCode::InvalidArgument, "owner is invalid"};
    }
    if (!method) {
        return Error{ErrorCode::InvalidArgument, "method is null"};
    }

    auto result = mHostMethodRegistry.registerMethod(owner, permissions, method);
    if (result.isErr()) {
        mLogger.warning("host", "method_registration_failed")
            .withMod(owner)
            .withError(result.error().code)
            .withMessage(result.error().message)
            .emit();
        return result.error();
    }

    mLogger.info("host", "method_registered")
        .withMod(owner)
        .withField("handle", std::to_string(result.value().value()))
        .withField("method", method->name())
        .emit();
    return result.value();
}

Result<RegistrationHandle>
DearOreUIApi::registerHostMethod(ModId owner, HostMethodManifest manifest, std::shared_ptr<ipc::IHostMethod> method) {
    if (!owner.isValid() || !mRegistry.isModRegistered(owner)) {
        return Error{ErrorCode::InvalidArgument, "owner mod is not registered"};
    }
    auto result = mHostMethodRegistry.registerMethod(owner, std::move(manifest), method);
    if (result.isErr()) {
        mLogger.warning("host", "method_registration_failed")
            .withMod(owner)
            .withError(result.error().code)
            .withMessage(result.error().message)
            .emit();
    }
    return result;
}

Result<void> DearOreUIApi::unregisterHostMethod(RegistrationHandle handle) {
    if (!handle.isValid()) {
        return Error{ErrorCode::InvalidArgument, "handle is invalid"};
    }

    bool removed = mHostMethodRegistry.unregister(handle);
    if (!removed) {
        return Error{ErrorCode::NotFound, "host method handle not found"};
    }

    mLogger.info("host", "method_unregistered").withField("handle", std::to_string(handle.value())).emit();
    return Result<void>::success();
}

namespace {

[[nodiscard]] dearoreui::api::Result<dearoreui::api::RegistrationHandle> registerUiImpl(
    dearoreui::api::ModId                    owner,
    dearoreui::api::UiManifest               manifest,
    std::string                              htmlBody,
    dearoreui::api::UiKind                   expectedKind,
    dearoreui::registry::IModRegistry&       registry,
    dearoreui::diagnostic::DiagnosticLogger& logger,
    std::vector<dearoreui::render::DomNode>  domNodes = {}
) {
    using namespace dearoreui::api;

    if (!owner.isValid()) {
        return Error{ErrorCode::InvalidArgument, "owner is invalid"};
    }
    if (!registry.isModRegistered(owner)) {
        return Error{ErrorCode::InvalidArgument, "owner mod is not registered"};
    }
    if (manifest.modNamespace != owner.value()) {
        return Error{ErrorCode::InvalidArgument, "manifest namespace does not match owner"};
    }

    manifest.kind = expectedKind;

    auto validation = ManifestValidator::validate(manifest);
    if (validation.isErr()) {
        logger.warning("manifest", "ui_validation_failed")
            .withMod(owner)
            .withError(validation.error().code)
            .withMessage(validation.error().message)
            .emit();
        return validation.error();
    }

    auto const modNamespace = manifest.modNamespace;
    auto const uiId         = manifest.id;
    auto const kind         = manifest.kind;

    dearoreui::registry::UiEntry entry;
    entry.owner    = owner;
    entry.manifest = std::move(manifest);
    entry.htmlBody = std::move(htmlBody);
    entry.domNodes = std::move(domNodes);

    auto result = registry.insert(std::move(entry));
    if (result.isErr()) {
        logger.warning("registry", "ui_registration_failed")
            .withMod(owner)
            .withError(result.error().code)
            .withMessage(result.error().message)
            .emit();
        return result.error();
    }

    logger.info("ui", "registered")
        .withMod(owner)
        .withField("handle", std::to_string(result.value().value()))
        .withField("namespace", modNamespace)
        .withField("ui_id", uiId)
        .withField("kind", std::string(uiKindName(kind)))
        .emit();
    dearoreui::diagnostic::recordStage7UiRegistered(owner, modNamespace, uiId, kind);
    return result.value();
}

} // namespace

Result<RegistrationHandle>
DearOreUIApi::registerOverlay(ModId owner, UiManifest const& manifest, std::string htmlBody) {
    return registerUiImpl(owner, manifest, std::move(htmlBody), UiKind::Overlay, mRegistry, mLogger);
}

Result<RegistrationHandle> DearOreUIApi::registerPanel(ModId owner, UiManifest const& manifest, std::string htmlBody) {
    return registerUiImpl(owner, manifest, std::move(htmlBody), UiKind::Panel, mRegistry, mLogger);
}

Result<RegistrationHandle> DearOreUIApi::registerButton(ModId owner, UiManifest const& manifest, std::string htmlBody) {
    return registerUiImpl(owner, manifest, std::move(htmlBody), UiKind::Button, mRegistry, mLogger);
}

Result<RegistrationHandle> DearOreUIApi::registerPage(ModId owner, UiManifest const& manifest, std::string htmlBody) {
    return registerUiImpl(owner, manifest, std::move(htmlBody), UiKind::Page, mRegistry, mLogger);
}

Result<RegistrationHandle>
DearOreUIApi::registerComponent(ModId owner, UiManifest const& manifest, component::ComponentSpec const& spec) {
    // Stage 8: render the declarative component into an htmlBody through the
    // component renderer, then reuse the standard overlay registration path.
    // The htmlBody is later parsed back into a DomNode forest by the universal
    // CSSOM renderer at injection time.
    //
    // M8.1.2: also keep the rendered DomNode forest (with per-state cssText
    // stateStyles) so injection can skip the lossy htmlBody round-trip.
    auto        nodes    = component::renderComponent(spec);
    std::string htmlBody = component::renderComponentToHtml(spec);

    mLogger.info("ui", "component_registered")
        .withMod(owner)
        .withField("component", std::string(component::componentKindName(spec.kind)))
        .withField("html_body_length", std::to_string(htmlBody.size()))
        .withField("dom_node_count", std::to_string(nodes.size()))
        .emit();

    return registerUiImpl(owner, manifest, std::move(htmlBody), UiKind::Overlay, mRegistry, mLogger, std::move(nodes));
}

Result<void> DearOreUIApi::unregisterUi(RegistrationHandle handle) {
    if (!handle.isValid()) {
        return Error{ErrorCode::InvalidArgument, "handle is invalid"};
    }

    auto found = mRegistry.find(handle);
    if (!found.has_value() || !std::holds_alternative<registry::UiEntry>(found.value())) {
        return Error{ErrorCode::NotFound, "ui registration handle not found"};
    }

    auto const& entry   = std::get<registry::UiEntry>(found.value());
    bool        removed = mRegistry.remove(handle);
    if (!removed) {
        return Error{ErrorCode::NotFound, "ui registration handle not found"};
    }

    mLogger.info("ui", "unregistered")
        .withField("handle", std::to_string(handle.value()))
        .withField("namespace", entry.manifest.modNamespace)
        .withField("ui_id", entry.manifest.id)
        .emit();
    diagnostic::recordStage7UiUnregistered(handle, entry.manifest.modNamespace, entry.manifest.id);
    return Result<void>::success();
}

} // namespace dearoreui::api
