#include "api/DearOreUIApi.h"

#include "api/manifest/ManifestValidator.h"
#include "diagnostic/Stage6TransformTelemetry.h"
#include "registry/ModRecord.h"
#include "registry/RegistryEntry.h"

#include <chrono>

namespace dearoreui::api {

DearOreUIApi::DearOreUIApi(
    registry::IModRegistry&       registry,
    ipc::HostMethodRegistry&      hostMethodRegistry,
    capability::ICapabilityQuery& capabilities,
    diagnostic::DiagnosticLogger& logger
)
: mRegistry(registry),
  mHostMethodRegistry(hostMethodRegistry),
  mCapabilities(capabilities),
  mLogger(logger) {}

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

void DearOreUIApi::setReady(bool ready) { mReady.store(ready, std::memory_order_relaxed); }

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
    bool        removed        = mRegistry.unregisterMod(id);
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

Result<RegistrationHandle>
DearOreUIApi::registerHostMethod(
    ModId owner,
    PermissionSet const& permissions,
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

} // namespace dearoreui::api
