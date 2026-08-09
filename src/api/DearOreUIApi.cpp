#include "api/DearOreUIApi.h"

#include "api/manifest/ManifestValidator.h"
#include "registry/RegistryEntry.h"

#include <chrono>

namespace dearoreui::api {

DearOreUIApi::DearOreUIApi(
    registry::IModRegistry&       registry,
    capability::ICapabilityQuery& capabilities,
    diagnostic::DiagnosticLogger& logger
)
    : mRegistry(registry), mCapabilities(capabilities), mLogger(logger) {}

ApiInfo DearOreUIApi::getInfo() const {
    ApiInfo info;
    info.protocolVersion = getProtocolVersion();
    info.modVersion      = Version{0, 2, 0};
    info.runtimeState    = isReady() ? "ready" : "initializing";
    info.ready           = isReady();
    return info;
}

CapabilitySet DearOreUIApi::getCapabilities() const {
    return mCapabilities.all();
}

SupportLevel DearOreUIApi::checkSupport(Capability capability) const {
    return mCapabilities.query(capability);
}

std::uint32_t DearOreUIApi::getProtocolVersion() const {
    return DearOreUIProtocolVersion;
}

bool DearOreUIApi::isReady() const {
    return mReady.load(std::memory_order_relaxed);
}

void DearOreUIApi::setReady(bool ready) {
    mReady.store(ready, std::memory_order_relaxed);
}

Result<RegistrationHandle> DearOreUIApi::registerResource(
    ModId owner, ResourceManifest const& manifest, std::string payload
) {
    if (!owner.isValid()) {
        return Error{ErrorCode::InvalidArgument, "owner is invalid"};
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
    entry.owner     = owner;
    entry.manifest  = manifest;
    entry.payload   = std::move(payload);

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

Result<RegistrationHandle> DearOreUIApi::registerScript(
    ModId owner, ScriptManifest const& manifest, std::string source
) {
    if (!owner.isValid()) {
        return Error{ErrorCode::InvalidArgument, "owner is invalid"};
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

Result<RegistrationHandle> DearOreUIApi::registerStyleSheet(
    ModId owner, StyleSheetManifest const& manifest, std::string source
) {
    if (!owner.isValid()) {
        return Error{ErrorCode::InvalidArgument, "owner is invalid"};
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

    mLogger.info("registry", "unregistered")
        .withField("handle", std::to_string(handle.value()))
        .emit();
    return Result<void>::success();
}

} // namespace dearoreui::api
