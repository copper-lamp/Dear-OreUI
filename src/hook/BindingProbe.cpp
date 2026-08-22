#include "hook/BindingProbe.h"

#include "diagnostic/DiagnosticLogger.h"

#include <windows.h>

#include <chrono>
#include <cstring>
#include <string>
#include <vector>

namespace dearoreui::hook {

std::mutex                                   BindingProbe::sMutex;
std::unordered_map<void*, BindingProbeSlots> BindingProbe::sSlots;

using RegisterForEventFn    = void*(__fastcall*)(void*, char const*, void*);
using UnregisterFromEventFn = void(__fastcall*)(void*, void*);
using TriggerEventFn        = void(__fastcall*)(void*, char const*);

namespace {

[[nodiscard]] std::uint64_t nowMs() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count()
    );
}

// Reads the first three vtable BindingProbeSlots of a handler object. C-style + SEH only
// (no C++ objects with destructors) so MSVC allows __try with /EHa.
struct HandlerLayout {
    std::uintptr_t vtable{0};
    std::uintptr_t slot0{0};
    std::uintptr_t slot1{0};
    std::uintptr_t slot2{0};
    bool           valid{false};
};

HandlerLayout readHandlerLayout(void* handler) {
    HandlerLayout layout;
    if (handler == nullptr) {
        return layout;
    }
    __try {
        void** const vtable = *reinterpret_cast<void***>(handler);
        layout.vtable       = reinterpret_cast<std::uintptr_t>(vtable);
        layout.slot0        = reinterpret_cast<std::uintptr_t>(vtable[0]);
        layout.slot1        = reinterpret_cast<std::uintptr_t>(vtable[1]);
        layout.slot2        = reinterpret_cast<std::uintptr_t>(vtable[2]);
        layout.valid        = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        layout.valid = false;
    }
    return layout;
}

void logRegister(char const* op, void* view, char const* name, void* handler, void* ret) {
    auto layout = readHandlerLayout(handler);
    diagnostic::globalLogger()
        .info("bindprobe", op)
        .withField("view", std::to_string(reinterpret_cast<std::uintptr_t>(view)))
        .withField("tid", std::to_string(GetCurrentThreadId()))
        .withField("ms", std::to_string(nowMs()))
        .withField("name", name != nullptr ? std::string{name} : std::string{"?"})
        .withField("handler", std::to_string(reinterpret_cast<std::uintptr_t>(handler)))
        .withField("h_valid", layout.valid ? "true" : "false")
        .withField("h_vtable", std::to_string(layout.vtable))
        .withField("h_slot0", std::to_string(layout.slot0))
        .withField("h_slot1", std::to_string(layout.slot1))
        .withField("h_slot2", std::to_string(layout.slot2))
        .withField("ret", std::to_string(reinterpret_cast<std::uintptr_t>(ret)))
        .emit();
}

void logHandleOp(char const* op, void* view, void* handle) {
    diagnostic::globalLogger()
        .info("bindprobe", op)
        .withField("view", std::to_string(reinterpret_cast<std::uintptr_t>(view)))
        .withField("tid", std::to_string(GetCurrentThreadId()))
        .withField("ms", std::to_string(nowMs()))
        .withField("handle", std::to_string(reinterpret_cast<std::uintptr_t>(handle)))
        .emit();
}

void logTrigger(void* view, char const* name) {
    diagnostic::globalLogger()
        .info("bindprobe", "trigger_event")
        .withField("view", std::to_string(reinterpret_cast<std::uintptr_t>(view)))
        .withField("tid", std::to_string(GetCurrentThreadId()))
        .withField("ms", std::to_string(nowMs()))
        .withField("name", name != nullptr ? std::string{name} : std::string{"?"})
        .emit();
}

} // namespace

// ---------------------------------------------------------------------------
// Thunks (x64 __fastcall; first arg is `this` = cohtml::View*)
// ---------------------------------------------------------------------------

void* __fastcall thunkRegisterForEvent(void* self, char const* name, void* handler) {
    auto* original = BindingProbe::find(self);
    void* ret      = nullptr;
    if (original != nullptr && original->registerForEvent != nullptr) {
        ret = reinterpret_cast<RegisterForEventFn>(original->registerForEvent)(self, name, handler);
    }
    logRegister("register_event", self, name, handler, ret);
    return ret;
}

void __fastcall thunkUnregisterFromEvent(void* self, void* handle) {
    auto* original = BindingProbe::find(self);
    if (original != nullptr && original->unregisterFromEvent != nullptr) {
        reinterpret_cast<UnregisterFromEventFn>(original->unregisterFromEvent)(self, handle);
    }
    logHandleOp("unregister_event", self, handle);
}

void* __fastcall thunkBindCall(void* self, char const* name, void* handler) {
    auto* original = BindingProbe::find(self);
    void* ret      = nullptr;
    if (original != nullptr && original->bindCall != nullptr) {
        ret = reinterpret_cast<RegisterForEventFn>(original->bindCall)(self, name, handler);
    }
    logRegister("bind_call", self, name, handler, ret);
    return ret;
}

void __fastcall thunkUnbindCall(void* self, void* handle) {
    auto* original = BindingProbe::find(self);
    if (original != nullptr && original->unbindCall != nullptr) {
        reinterpret_cast<UnregisterFromEventFn>(original->unbindCall)(self, handle);
    }
    logHandleOp("unbind_call", self, handle);
}

void __fastcall thunkTriggerEvent(void* self, char const* name) {
    auto* original = BindingProbe::find(self);
    if (original != nullptr && original->triggerEvent != nullptr) {
        reinterpret_cast<TriggerEventFn>(original->triggerEvent)(self, name);
    }
    logTrigger(self, name);
}

// ---------------------------------------------------------------------------
// BindingProbe
// ---------------------------------------------------------------------------

BindingProbeSlots* BindingProbe::find(void* view) {
    std::lock_guard lock{sMutex};
    auto            it = sSlots.find(view);
    return it != sSlots.end() ? &it->second : nullptr;
}

void BindingProbe::install(void* gamefaceView) {
    if (gamefaceView == nullptr) {
        return;
    }
    auto** vtable = *reinterpret_cast<void***>(gamefaceView);
    if (vtable == nullptr) {
        return;
    }

    std::lock_guard lock{sMutex};
    auto [it, inserted]                  = sSlots.try_emplace(gamefaceView);
    BindingProbeSlots& BindingProbeSlots = it->second;
    if (!inserted && BindingProbeSlots.registerForEvent != nullptr) {
        return; // already installed for this view
    }

    BindingProbeSlots.registerForEvent    = vtable[kViewSlotRegisterForEvent];
    BindingProbeSlots.unregisterFromEvent = vtable[kViewSlotUnregisterFromEvent];
    BindingProbeSlots.bindCall            = vtable[kViewSlotBindCall];
    BindingProbeSlots.unbindCall          = vtable[kViewSlotUnbindCall];
    BindingProbeSlots.triggerEvent        = vtable[kViewSlotTriggerEvent];

    DWORD oldProtect = 0;
    if (VirtualProtect(vtable, 76 * sizeof(void*), PAGE_READWRITE, &oldProtect)) {
        vtable[kViewSlotRegisterForEvent]    = reinterpret_cast<void*>(&thunkRegisterForEvent);
        vtable[kViewSlotUnregisterFromEvent] = reinterpret_cast<void*>(&thunkUnregisterFromEvent);
        vtable[kViewSlotBindCall]            = reinterpret_cast<void*>(&thunkBindCall);
        vtable[kViewSlotUnbindCall]          = reinterpret_cast<void*>(&thunkUnbindCall);
        vtable[kViewSlotTriggerEvent]        = reinterpret_cast<void*>(&thunkTriggerEvent);
        static_cast<void>(VirtualProtect(vtable, 76 * sizeof(void*), oldProtect, &oldProtect));
    }

    diagnostic::globalLogger()
        .info("bindprobe", "installed")
        .withField("view", std::to_string(reinterpret_cast<std::uintptr_t>(gamefaceView)))
        .withField("vtable", std::to_string(reinterpret_cast<std::uintptr_t>(vtable)))
        .emit();
}

void BindingProbe::uninstall(void* gamefaceView) {
    if (gamefaceView == nullptr) {
        return;
    }
    std::lock_guard lock{sMutex};
    auto            it = sSlots.find(gamefaceView);
    if (it == sSlots.end()) {
        return;
    }
    auto** vtable = *reinterpret_cast<void***>(gamefaceView);
    if (vtable != nullptr) {
        DWORD oldProtect = 0;
        if (VirtualProtect(vtable, 76 * sizeof(void*), PAGE_READWRITE, &oldProtect)) {
            vtable[kViewSlotRegisterForEvent]    = it->second.registerForEvent;
            vtable[kViewSlotUnregisterFromEvent] = it->second.unregisterFromEvent;
            vtable[kViewSlotBindCall]            = it->second.bindCall;
            vtable[kViewSlotUnbindCall]          = it->second.unbindCall;
            vtable[kViewSlotTriggerEvent]        = it->second.triggerEvent;
            static_cast<void>(VirtualProtect(vtable, 76 * sizeof(void*), oldProtect, &oldProtect));
        }
    }
    sSlots.erase(it);
}

void BindingProbe::clearAll() {
    std::vector<void*> views;
    {
        std::lock_guard lock{sMutex};
        views.reserve(sSlots.size());
        for (auto const& [view, BindingProbeSlots] : sSlots) {
            views.push_back(view);
        }
    }
    for (void* view : views) {
        uninstall(view);
    }
}

} // namespace dearoreui::hook
