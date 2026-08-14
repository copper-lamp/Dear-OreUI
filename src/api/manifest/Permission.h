#pragma once

#include <cstddef>
#include <string>
#include <unordered_set>
#include <vector>

namespace dearoreui::api {

enum class Permission {
    ResourceRead,
    ResourceRegister,
    PageObserve,
    UiMount,
    HostReadOnly,
    HostWrite,
    TransformResource,
    TransformBundle,
    DiagnosticRead,
};

[[nodiscard]] constexpr std::string_view permissionName(Permission permission) {
    switch (permission) {
    case Permission::ResourceRead:
        return "resource.read";
    case Permission::ResourceRegister:
        return "resource.register";
    case Permission::PageObserve:
        return "page.observe";
    case Permission::UiMount:
        return "ui.mount";
    case Permission::HostReadOnly:
        return "host.read_only";
    case Permission::HostWrite:
        return "host.write";
    case Permission::TransformResource:
        return "transform.resource";
    case Permission::TransformBundle:
        return "transform.bundle";
    case Permission::DiagnosticRead:
        return "diagnostic.read";
    }
    return "unknown";
}

class PermissionSet {
public:
    PermissionSet() = default;

    explicit PermissionSet(std::vector<Permission> permissions) {
        for (auto permission : permissions) {
            grant(permission);
        }
    }

    void grant(Permission permission) { mPermissions.insert(permission); }

    [[nodiscard]] bool has(Permission permission) const { return mPermissions.find(permission) != mPermissions.end(); }

    [[nodiscard]] std::unordered_set<Permission> const& all() const { return mPermissions; }

    [[nodiscard]] bool operator==(PermissionSet const& other) const { return mPermissions == other.mPermissions; }

    [[nodiscard]] bool operator!=(PermissionSet const& other) const { return !(*this == other); }

private:
    std::unordered_set<Permission> mPermissions;
};

} // namespace dearoreui::api
