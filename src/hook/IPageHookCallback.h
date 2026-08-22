#pragma once

#include "api/types/Id.h"
#include "api/types/Page.h"

#include <optional>
#include <string_view>

namespace dearoreui::hook {

class IPageHookCallback {
public:
    virtual ~IPageHookCallback() = default;

    [[nodiscard]] virtual api::ContextId
                 onPageCreated(std::string_view url, std::optional<api::RouterLocationSnapshot> location) = 0;
    virtual void onPageDestroyed(api::ContextId id)                                                       = 0;

    /// Client main-loop frame callback (ClientInstance::update hook). Drives
    /// the UI frame service: periodic C++->JS pushes run here on the game
    /// thread, the only reliable cadence on this client.
    virtual void onClientFrame() = 0;
};

} // namespace dearoreui::hook
