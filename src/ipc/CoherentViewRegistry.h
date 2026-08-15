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
// the most recent handle. The script context is NOT usable the moment a view
// is created — cohtml only accepts ExecuteScript after the page's script
// context exists (IViewListener::OnReadyForBindings / OnScriptContextCreated).
// The bridge defers scripts until markScriptContextReady() flips the flag.
class CoherentViewRegistry {
public:
    CoherentViewRegistry() = default;

    // Not copyable/movable: the bridge and the hook adapter hold references.
    CoherentViewRegistry(CoherentViewRegistry const&)            = delete;
    CoherentViewRegistry& operator=(CoherentViewRegistry const&) = delete;

    // Called on the game thread when a gameface view becomes available.
    void registerView(void* gamefaceView);

    // Called when the engine signals the page script context is ready
    // (OreUI::View::OnReadyForBindings). Notifies the script-context observer.
    void markScriptContextReady();

    // Called when the owning OreUI::View is being destroyed (before the
    // engine tears down the cohtml view). Clears the tracked handle so no code
    // can later use a dangling view pointer, and notifies the destroy observer
    // (used by the bridge to unbind native JS->C++ handlers).
    void onViewDestroyed(void* gamefaceView);

    // Called when the engine releases the page's bindings (IViewListener::
    // OnBindingsReleased) — typically when the OreUI page is unloaded while
    // the cohtml view itself is reused, NOT destroyed. Resets the script-context
    // flag (the page context is gone until OnReadyForBindings fires again) and
    // notifies the destroy observer so the bridge unbinds its native handler
    // BEFORE the engine frees the bindings.
    void notifyBindingsReleased();

    // Most recently registered non-null view handle, or nullptr.
    [[nodiscard]] void* activeView() const;

    // Whether the active view's script context is ready to accept scripts.
    [[nodiscard]] bool isScriptContextReady() const;

    [[nodiscard]] bool hasActiveView() const;

    [[nodiscard]] std::size_t count() const;

    void clear();

    // Single observer invoked on the calling thread after each registerView.
    void setOnViewRegistered(std::function<void(void*)> observer);

    // Single observer invoked (on the game thread) when the active view's
    // script context becomes ready. Used by the bridge to flush deferred
    // scripts at the correct moment.
    void setOnScriptContextReady(std::function<void()> observer);

    // Single observer invoked (on the game thread) when the active view is
    // being destroyed. Used by the bridge to release native BindCall handlers
    // before the engine tears the view down.
    void setOnViewDestroyed(std::function<void(void*)> observer);

private:
    void*                            mActiveView{nullptr};
    bool                             mScriptContextReady{false};
    std::function<void(void*)>       mOnViewRegistered;
    std::function<void()>            mOnScriptContextReady;
    std::function<void(void*)>       mOnViewDestroyed;
    mutable std::mutex               mMutex;
};

} // namespace dearoreui::ipc
