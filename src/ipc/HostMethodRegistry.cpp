#include "ipc/HostMethodRegistry.h"

#include "api/types/Error.h"

namespace dearoreui::ipc {

api::Result<api::RegistrationHandle> HostMethodRegistry::registerMethod(
    const api::ModId&                   owner,
    api::PermissionSet const&    permissions,
    const std::shared_ptr<IHostMethod>& method
) {
    if (!owner.isValid() || method == nullptr) {
        return api::Error{api::ErrorCode::InvalidArgument, "invalid owner or method"};
    }

    auto required = method->requiredPermission();
    if (!permissions.has(required)) {
        return api::Error{
            api::ErrorCode::HostPermissionDenied,
            std::string{"mod lacks permission: "} + std::string{api::permissionName(required)}
        };
    }

    std::lock_guard lock(mMutex);

    auto name = method->name();
    if (mByName.find(name) != mByName.end()) {
        return api::Error{api::ErrorCode::AlreadyExists, "host method already registered"};
    }

    auto            handle = nextHandle();
    HostMethodEntry entry;
    entry.handle      = handle;
    entry.owner       = owner;
    entry.permissions = permissions;
    entry.method      = method;

    mEntries.emplace(handle, std::move(entry));
    mByName.emplace(std::move(name), handle);
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

std::shared_ptr<IHostMethod> HostMethodRegistry::find(std::string_view name) const {
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
