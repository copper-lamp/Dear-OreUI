#pragma once

#include <cstddef>
#include <mutex>
#include <unordered_map>

namespace dearoreui::hook {

// cohtml::View vtable slot indices (declaration order == MSVC layout; slot 60
// ExecuteScript was machine-code-fingerprint verified in Stage 7.1).
inline constexpr std::size_t kViewSlotRegisterForEvent    = 52;
inline constexpr std::size_t kViewSlotUnregisterFromEvent = 53;
inline constexpr std::size_t kViewSlotBindCall            = 54;
inline constexpr std::size_t kViewSlotUnbindCall          = 55;
inline constexpr std::size_t kViewSlotTriggerEvent        = 75;

// Original vtable slot pointers kept per installed view.
struct BindingProbeSlots {
    void* registerForEvent{nullptr};
    void* unregisterFromEvent{nullptr};
    void* bindCall{nullptr};
    void* unbindCall{nullptr};
    void* triggerEvent{nullptr};
};

// Read-only observer for how the GAME registers its native JS<->C++ handlers
// on the cohtml view (event names, timing, handler layout). The thunks log a
// bindprobe diagnostic event, then call the original slot unchanged.
//
// Installed from Stage7ViewInitializeHook BEFORE origin() runs so that
// registrations made inside OreUI::View::initialize are captured too. This is
// the "observe the game" step: it tells us when / how the game registers
// facet:request handlers without crashing, so the mod can mimic the pattern.
class BindingProbe {
public:
    // Replaces the binding slots of the view's vtable (idempotent per view).
    static void install(void* gamefaceView);

    // Restores the vtable slots and drops bookkeeping. Only safe while the
    // view memory is still alive.
    static void uninstall(void* gamefaceView);

    // Restores every installed vtable and clears bookkeeping (mod disable).
    static void clearAll();

    // Called by the thunks: returns the saved original slots for the view.
    static BindingProbeSlots* find(void* view);

private:
    static std::mutex                                   sMutex;
    static std::unordered_map<void*, BindingProbeSlots> sSlots;
};

} // namespace dearoreui::hook
