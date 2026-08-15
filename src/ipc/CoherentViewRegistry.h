#pragma once

#include <cstddef>
#include <functional>
#include <mutex>

namespace dearoreui::ipc {

// Tracks live Coherent gameface view handles discovered from the
// OreUI::View::initialize hook. The handles are opaque (void*) so this
// registry stays free of mc/cohtml headers and remains unit-testable.
//
// A view may be re-initialized when reused across pages; registerView keeps
// the most recent handle and notifies the registered observer (used by
// CoherentHostBridge to flush deferred scripts).
class CoherentViewRegistry {
public:
    CoherentViewRegistry() = default;

    // Not copyable/movable: the bridge and the hook adapter hold references.
    CoherentViewRegistry(CoherentViewRegistry const&)            = delete;
    CoherentViewRegistry& operator=(CoherentViewRegistry const&) = delete;

    // Called on the game thread when a gameface view becomes available.
    void registerView(void* gamefaceView);

    // Most recently registered non-null view handle, or nullptr.
    [[nodiscard]] void* activeView() const;

    [[nodiscard]] bool hasActiveView() const;

    [[nodiscard]] std::size_t count() const;

    void clear();

    // Single observer invoked on the calling thread after each registerView.
    void setOnViewRegistered(std::function<void(void*)> observer);

private:
    void*                            mActiveView{nullptr};
    std::function<void(void*)>       mOnViewRegistered;
    mutable std::mutex               mMutex;
};

} // namespace dearoreui::ipc
