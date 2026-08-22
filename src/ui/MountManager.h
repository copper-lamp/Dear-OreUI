#pragma once

#include "api/types/Id.h"
#include "ui/IMountHost.h"
#include "ui/MountHandle.h"
#include "ui/UiMountPlan.h"

#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace dearoreui::ui {

class MountManager {
public:
    explicit MountManager(IMountHost& host);

    [[nodiscard]] api::Result<UiMountPlan> mountPage(api::ContextId contextId, UiMountPlan plan);
    [[nodiscard]] api::Result<void>        unmountPage(api::ContextId contextId);

    [[nodiscard]] bool        hasMounted(api::ContextId contextId, api::RegistrationHandle handle) const;
    [[nodiscard]] std::size_t activeMountCount(api::ContextId contextId) const;
    [[nodiscard]] std::size_t totalActiveCount() const;

private:
    IMountHost&                                                  mHost;
    mutable std::mutex                                           mMutex;
    std::unordered_map<api::ContextId, std::vector<MountHandle>> mActiveMounts;
};

} // namespace dearoreui::ui
