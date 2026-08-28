#include "ipc/HostMethodRegistry.h"

#include "api/types/Error.h"

#include <chrono>

namespace dearoreui::ipc {

api::Result<api::RegistrationHandle> HostMethodRegistry::registerMethod(
    const api::ModId&                   owner,
    api::PermissionSet const&           permissions,
    const std::shared_ptr<api::IHostMethod>& method
) {
    api::HostMethodManifest manifest;
    manifest.name        = method != nullptr ? method->name() : std::string{};
    manifest.permissions = permissions;
    return registerMethod(owner, std::move(manifest), method);
}

api::Result<api::RegistrationHandle> HostMethodRegistry::registerMethod(
    const api::ModId&                   owner,
    api::HostMethodManifest             manifest,
    const std::shared_ptr<api::IHostMethod>& method
) {
    if (!owner.isValid() || method == nullptr || manifest.name.empty()) {
        return api::Error{api::ErrorCode::InvalidArgument, "invalid owner, manifest or method"};
    }
    if (manifest.name != method->name()) {
        return api::Error{api::ErrorCode::InvalidArgument, "manifest name does not match method"};
    }
    if (manifest.name.find('.') == std::string::npos) {
        return api::Error{api::ErrorCode::NamespaceConflict, "host method must be namespace qualified"};
    }
    if (manifest.timeout.count() <= 0 || manifest.timeout > std::chrono::seconds{30} || manifest.maxRequestBytes == 0
        || manifest.maxRequestBytes > 1024 * 1024 || manifest.maxResponseBytes == 0
        || manifest.maxResponseBytes > 4 * 1024 * 1024) {
        return api::Error{api::ErrorCode::InvalidArgument, "host method limits are outside production bounds"};
    }

    auto required = method->requiredPermission();
    if (!manifest.permissions.has(required)) {
        return api::Error{
            api::ErrorCode::HostPermissionDenied,
            std::string{"mod lacks permission: "} + std::string{api::permissionName(required)}
        };
    }

    std::lock_guard lock(mMutex);
    if (mByName.find(manifest.name) != mByName.end()) {
        return api::Error{api::ErrorCode::AlreadyExists, "host method already registered"};
    }

    auto            handle = nextHandle();
    HostMethodEntry entry{handle, owner, manifest.permissions, std::move(manifest), method};
    mEntries.emplace(handle, std::move(entry));
    mByName.emplace(method->name(), handle);
    return handle;
}

bool HostMethodRegistry::unregister(api::RegistrationHandle handle) {
    std::lock_guard lock(mMutex);
    auto            iterator = mEntries.find(handle);
    if (iterator == mEntries.end()) {
        return false;
    }
    mByName.erase(iterator->second.method->name());
    mEntries.erase(iterator);
    return true;
}

std::shared_ptr<api::IHostMethod> HostMethodRegistry::find(std::string_view name) const {
    std::lock_guard lock(mMutex);
    auto            iterator = mByName.find(std::string{name});
    if (iterator == mByName.end()) {
        return nullptr;
    }
    auto entryIterator = mEntries.find(iterator->second);
    if (entryIterator == mEntries.end()) {
        return nullptr;
    }
    return entryIterator->second.method;
}

std::optional<HostMethodEntry> HostMethodRegistry::findEntry(api::RegistrationHandle handle) const {
    std::lock_guard lock(mMutex);
    auto            iterator = mEntries.find(handle);
    if (iterator == mEntries.end()) {
        return std::nullopt;
    }
    return iterator->second;
}

std::optional<HostMethodEntry> HostMethodRegistry::findByName(std::string_view name) const {
    std::lock_guard lock(mMutex);
    auto byName = mByName.find(std::string{name});
    if (byName == mByName.end()) return std::nullopt;
    auto entry = mEntries.find(byName->second);
    if (entry == mEntries.end()) return std::nullopt;
    return entry->second;
}

std::size_t HostMethodRegistry::size() const {
    std::lock_guard lock(mMutex);
    return mEntries.size();
}

void HostMethodRegistry::clear() {
    std::lock_guard lock(mMutex);
    mEntries.clear();
    mByName.clear();
}

api::RegistrationHandle HostMethodRegistry::nextHandle() { return api::RegistrationHandle{mNextHandle.fetch_add(1)}; }

} // namespace dearoreui::ipc
