#pragma once

#include "api/manifest/Permission.h"
#include "api/types/HostMethodManifest.h"
#include "api/types/Id.h"
#include "api/types/Result.h"
#include "ipc/IHostMethod.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace dearoreui::ipc {

struct HostMethodEntry {
    api::RegistrationHandle      handle;
    api::ModId                   owner;
    api::PermissionSet           permissions;
    api::HostMethodManifest      manifest;
    std::shared_ptr<IHostMethod> method;
};

class HostMethodRegistry {
public:
    HostMethodRegistry() = default;

    [[nodiscard]] api::Result<api::RegistrationHandle> registerMethod(
        const api::ModId&                   owner,
        api::PermissionSet const&           permissions,
        const std::shared_ptr<IHostMethod>& method
    );

    [[nodiscard]] api::Result<api::RegistrationHandle> registerMethod(
        const api::ModId&                   owner,
        api::HostMethodManifest             manifest,
        const std::shared_ptr<IHostMethod>& method
    );

    [[nodiscard]] bool unregister(api::RegistrationHandle handle);

    [[nodiscard]] std::shared_ptr<IHostMethod>   find(std::string_view name) const;
    [[nodiscard]] std::optional<HostMethodEntry> findEntry(api::RegistrationHandle handle) const;
    [[nodiscard]] std::size_t                    size() const;
    void                                         clear();

private:
    [[nodiscard]] api::RegistrationHandle nextHandle();

    mutable std::mutex                                           mMutex;
    std::unordered_map<api::RegistrationHandle, HostMethodEntry> mEntries;
    std::unordered_map<std::string, api::RegistrationHandle>     mByName;
    std::atomic<std::uint64_t>                                   mNextHandle{1};
};

} // namespace dearoreui::ipc
