#pragma once

#include "api/types/Id.h"
#include "api/types/Result.h"

#include <cstddef>
#include <functional>

namespace dearoreui::api {

struct FrameSubscriptionOptions {
    ModId owner;
};

/// Client-main-loop frame service. Subscribers are invoked once per client
/// frame (ClientInstance::update hook, game thread) while the runtime is
/// enabled. This is the only reliable periodic cadence on the real client:
/// world tick emitters are unavailable during enable and the LL coroutine
/// executor does not run. Used to drive periodic C++->JS event pushes.
class IFrameApi {
public:
    using FrameCallback = std::function<void()>;

    virtual ~IFrameApi() = default;

    /// Registers a per-frame callback. The owning mod must be registered.
    virtual Result<SubscriptionHandle> subscribeFrame(FrameSubscriptionOptions options, FrameCallback callback) = 0;

    /// Unregisters a frame subscription (idempotent).
    virtual Result<void> unsubscribeFrame(SubscriptionHandle handle) = 0;

    /// Drives one client frame into all subscribers. Internal: called by the
    /// runtime from the ClientInstance::update hook while a page is alive.
    virtual void frameTick() = 0;
};

} // namespace dearoreui::api