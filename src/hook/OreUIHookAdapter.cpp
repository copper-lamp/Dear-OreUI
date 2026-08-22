#include "hook/OreUIHookAdapter.h"

#include "diagnostic/Stage0TelemetryCompat.h"
#include "hook/BindingProbe.h"
#include "poc/Stage1NavigationPoc.h"

#include "ll/api/memory/Hook.h"

#include <windows.h>

#include "mc/client/game/ClientInstance.h"
#include "mc/client/gui/ScreenTechStackSelector.h"
#include "mc/client/gui/TechStack.h"
#include "mc/client/gui/oreui/SceneProvider.h"
#include "mc/client/gui/oreui/binding/FacetRegistryFactory.h"
#include "mc/client/gui/oreui/input/ViewInputHandler.h"
#include "mc/client/gui/oreui/interface/IFacetRegistry.h"
#include "mc/client/gui/oreui/routing/Router.h"
#include "mc/client/gui/oreui/views/View.h"
#include "mc/client/gui/oreui/views/ViewRenderer.h"

#include <atomic>
#include <cctype>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace dearoreui::hook {

namespace {

struct AdapterState {
    bool techStack{};
    bool scene{};
    bool routeChange{};
    bool routePush{};
    bool routeReplace{};
    bool routeBack{};
    bool routerDestructorThunk{};
    bool clientUpdate{};
    bool viewInitialize{};
    bool onReadyForBindings{};
    bool viewDestructor{};
    bool bindingsReleased{};
    bool viewRemoveScene{};
    bool viewUnload{};
    bool facetRegistryFactory{}; // Stage 8-A: createFacetRegistry hook (optional)

    std::mutex                                         mutex;
    std::unordered_map<OreUI::Router*, api::ContextId> routerContexts;
    std::unordered_set<std::string>                    observed;
    IPageHookCallback*                                 callback{};
    Stage5CoherentProbe*                               probe{};
    capability::ICapabilityQuery*                      capabilities{};
    ipc::CoherentViewRegistry*                         viewRegistry{};
    // Stage 8-A: invoked by the createFacetRegistry hook for every fresh
    // IFacetRegistry so the "dearoreui" facet can be registered (JS->C++
    // channel). Wired by Runtime to OreUIFacetBridge.
    std::function<void(void*)> onFacetRegistryCreated{};
};

AdapterState& state() {
    static AdapterState value;
    return value;
}

// ---------------------------------------------------------------------------
// Stage 7.1 JS feedback via OutputDebugString.
// Coherent's log handler and JS console messages are routed through
// OutputDebugStringA/W on Windows. Hooking these kernel32 exports is safe (no
// unverified vtable ABI) and lets us see whether the injected bootstrap script
// actually ran inside the engine: its console.log lines carry the
// "[DearOreUI]" prefix and its engine.call('dearoreui_report', ...) probe
// produces a "no such method" message when no binding is registered.
// ---------------------------------------------------------------------------

bool containsDearOreUi(std::string_view text) {
    if (text.size() < 9) return false;
    std::string lower;
    lower.reserve(text.size());
    for (char c : text) {
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return lower.find("dearoreui") != std::string::npos;
}

void recordOdsLine(std::string_view line) {
    std::string sanitized{line};
    while (!sanitized.empty() && (sanitized.back() == '\n' || sanitized.back() == '\r')) {
        sanitized.pop_back();
    }
    diagnostic::recordStage0("js", "event=ods\tmsg=" + sanitized);
}

void dumpViewVtable(void* gamefaceView);

LL_AUTO_STATIC_HOOK(
    Stage7OutputDebugStringAHook,
    ll::memory::HookPriority::Low,
    &::OutputDebugStringA,
    void,
    const char* lpOutputString
) {
    if (lpOutputString != nullptr && containsDearOreUi(lpOutputString)) {
        recordOdsLine(lpOutputString);
    }
    origin(lpOutputString);
}

LL_AUTO_STATIC_HOOK(
    Stage7OutputDebugStringWHook,
    ll::memory::HookPriority::Low,
    &::OutputDebugStringW,
    void,
    const wchar_t* lpOutputString
) {
    if (lpOutputString != nullptr) {
        int const length = ::WideCharToMultiByte(CP_UTF8, 0, lpOutputString, -1, nullptr, 0, nullptr, nullptr);
        if (length > 0) {
            std::string utf8(static_cast<std::size_t>(length), '\0');
            ::WideCharToMultiByte(CP_UTF8, 0, lpOutputString, -1, utf8.data(), length, nullptr, nullptr);
            if (containsDearOreUi(utf8)) {
                recordOdsLine(utf8);
            }
        }
    }
    origin(lpOutputString);
}

char const* techStackName(ui::TechStack stack) {
    switch (stack) {
    case ui::TechStack::JsonUI:
        return "JsonUI";
    case ui::TechStack::OreUI:
        return "OreUI";
    default:
        return "Unknown";
    }
}

std::string locationFields(std::string_view prefix, std::optional<OreUI::RouterLocation> const& location) {
    if (!location) return std::string(prefix) + "=none";
    return std::string(prefix) + "_path=" + location->getPath() + "\t" + std::string(prefix)
         + "_query=" + location->getQuery() + "\t" + std::string(prefix) + "_fragment=" + location->getFragment();
}

std::optional<api::RouterLocationSnapshot> makeLocationSnapshot(std::optional<OreUI::RouterLocation> const& location) {
    if (!location) return std::nullopt;
    return api::RouterLocationSnapshot{
        location->getPath(),
        location->getQuery(),
        location->getFragment(),
    };
}

void recordOnce(std::string event, std::string fields) {
    auto key = event + "\t" + fields;
    if (!state().observed.insert(key).second) return;
    diagnostic::recordStage0(event, fields);
}

void notifyPageCreated(OreUI::Router& router, std::string_view url) {
    auto&           adapterState = state();
    std::lock_guard lock{adapterState.mutex};
    if (!adapterState.callback) return;

    auto contextId = adapterState.callback->onPageCreated(url, makeLocationSnapshot(router.getCurrentLocation()));
    adapterState.routerContexts[&router] = contextId;
}

void notifyPageDestroyed(OreUI::Router& router) {
    auto&           adapterState = state();
    std::lock_guard lock{adapterState.mutex};
    auto            iterator = adapterState.routerContexts.find(&router);
    if (iterator == adapterState.routerContexts.end()) return;

    auto contextId = iterator->second;
    adapterState.routerContexts.erase(iterator);

    if (adapterState.callback) {
        adapterState.callback->onPageDestroyed(contextId);
    }
}

LL_TYPE_INSTANCE_HOOK(
    Stage0TechStackHook,
    ll::memory::HookPriority::Normal,
    ui::ScreenTechStackSelector,
    &ui::ScreenTechStackSelector::getTechStackForScreen,
    ui::TechStack,
    std::string const& screenName
) {
    diagnostic::recordStage0("hook", "event=techstack_entered\tscreen=" + screenName);
    return origin(screenName);
}

LL_TYPE_INSTANCE_HOOK(
    Stage0SceneProviderHook,
    ll::memory::HookPriority::Normal,
    OreUI::SceneProvider,
    &OreUI::SceneProvider::createScene,
    std::shared_ptr<AbstractScene>,
    std::string const&                              url,
    OreUI::Router&                                  router,
    Bedrock::NotNullNonOwnerPtr<ISceneStack> const& sceneStack,
    OreUI::RouteMode                                mode,
    OreUI::FacetRegistryLocation                    location
) {
    diagnostic::recordStage0("hook", "event=sceneprovider_entered\turl=" + url);
    auto result = origin(url, router, sceneStack, mode, location);
    {
        std::lock_guard lock{state().mutex};
        if (state().probe != nullptr && result != nullptr) {
            state().probe->onSceneCreated(url, result.get());
        }
    }
    poc::registerRouter(router, *sceneStack);
    notifyPageCreated(router, url);
    return result;
}

LL_TYPE_INSTANCE_HOOK(
    Stage1RouterDestructorThunkHook,
    ll::memory::HookPriority::Normal,
    OreUI::Router,
    &OreUI::Router::$dtor,
    void
) {
    diagnostic::recordStage0("hook", "event=router_dtor_entered");
    notifyPageDestroyed(*this);
    poc::invalidateRouter(*this);
    origin();
}

LL_TYPE_INSTANCE_HOOK(
    Stage1ClientUpdateHook,
    ll::memory::HookPriority::Normal,
    ClientInstance,
    &ClientInstance::$update,
    bool,
    bool isInitFinished
) {
    diagnostic::recordStage0(
        "hook",
        "event=clientupdate_entered\tinit=" + std::string(isInitFinished ? "true" : "false")
    );
    auto result = origin(isInitFinished);
    // Stage 1 navigation PoC is disabled in stage 3.
    // if (isInitFinished) poc::consumeStage1Navigation();
    return result;
}

// Stage 7.1: capture the real Coherent gameface view. OreUI::View::initialize
// is an exported MCAPI symbol that receives the cohtml::View& exactly once per
// view setup, giving us the JS execution entry (cohtml::View::ExecuteScript)
// without any class-layout assumptions. The hook is read-only: origin runs
// unchanged and we only register the view handle.
//
// Stage 8 observe-the-game: BindingProbe replaces the view's binding vtable
// slots BEFORE origin() so that registrations made inside View::initialize
// (e.g. the game's facet:request handler) are observed too.
LL_TYPE_INSTANCE_HOOK(
    Stage7ViewInitializeHook,
    ll::memory::HookPriority::Normal,
    OreUI::View,
    &OreUI::View::initialize,
    void,
    ::cohtml::View&                          gamefaceView,
    std::unique_ptr<OreUI::ViewRenderer>     renderer,
    std::unique_ptr<OreUI::ViewInputHandler> inputHandler,
    OreUI::Detail::ViewContextFactory&       contextFactory,
    ::IOptions&                              options
) {
    BindingProbe::install(&gamefaceView);
    origin(gamefaceView, std::move(renderer), std::move(inputHandler), contextFactory, options);
    {
        std::lock_guard lock{state().mutex};
        if (state().viewRegistry != nullptr) {
            state().viewRegistry->registerView(&gamefaceView);
        }
        if (state().probe != nullptr) {
            state().probe->onViewInitialized(&gamefaceView);
        }
        if (state().capabilities != nullptr) {
            static_cast<void>(state().capabilities->setLevel(
                api::Capability::HostBridge,
                api::SupportLevel::Experimental,
                "cohtml::View::ExecuteScript captured via OreUI::View::initialize"
            ));
        }
        // Stage 7.1: dump the real cohtml::View vtable so we can map the engine's
        // actual method slots (the mcmeta vtable layout is NOT reliable — the
        // BindCall slot caused heap corruption, see crash trace 11:38).
        dumpViewVtable(&gamefaceView);
    }
}

// Reads 32 bytes of machine code at fn into a 64-char hex buffer. Kept free of
// C++ objects with destructors so MSVC allows SEH (__try) inside it.
bool readCodeBytesHex(void const* fn, char out[64]) {
    if (fn == nullptr) return false;
    __try {
        auto const*           bytes  = static_cast<std::uint8_t const*>(fn);
        static constexpr char kHex[] = "0123456789abcdef";
        for (int b = 0; b < 32; ++b) {
            std::uint8_t const byte = bytes[b];
            out[b * 2]              = kHex[(byte >> 4) & 0xF];
            out[b * 2 + 1]          = kHex[byte & 0xF];
        }
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void dumpViewVtable(void* gamefaceView) {
    auto* vtable = *reinterpret_cast<void***>(gamefaceView);
    if (vtable == nullptr) {
        diagnostic::recordStage0("js", "event=vtable_null");
        return;
    }
    std::string line  = "event=vtable\tptr=";
    line             += std::to_string(reinterpret_cast<std::uintptr_t>(gamefaceView));
    line             += "\tvtable=";
    line             += std::to_string(reinterpret_cast<std::uintptr_t>(vtable));
    line             += "\tslots=";
    for (int i = 0; i < 72; ++i) {
        if (i > 0) line += ",";
        line += std::to_string(reinterpret_cast<std::uintptr_t>(vtable[i]));
    }
    diagnostic::recordStage0("js", line);

    // Stage 7.1 calibration: dump the first 32 bytes of machine code of each
    // vtable slot. Function-prologue fingerprints let us identify simple
    // getters (GetId/GetWidth/GetHeight/IsReadyForBindings return a member
    // quickly) and correlate them with the real engine's vtable order, without
    // ever invoking an unverified slot (the BindCall slot corrupted the heap).
    // 72 slots covers the full SDK interface (up to TriggerEvent at index 75).
    std::string hexLine  = "event=vtable_bytes\tptr=";
    hexLine             += std::to_string(reinterpret_cast<std::uintptr_t>(gamefaceView));
    for (int i = 0; i < 72; ++i) {
        hexLine += "\tslot" + std::to_string(i) + "=";
        char buffer[64];
        if (readCodeBytesHex(vtable[i], buffer)) {
            hexLine.append(buffer, 64);
        } else {
            hexLine += vtable[i] == nullptr ? "-" : "unreadable";
        }
    }
    diagnostic::recordStage0("js", hexLine);
}

// Stage 8 fix (round 3): the OreUI view is REUSED across page switches, so
// neither $dtor nor $OnBindingsReleased fire when leaving /play/all. The
// engine instead removes the page scene from the view (removeScene) and then
// tears down the cohtml document — freeing our BindCall handler on its own
// thread, which corrupts the heap. Unbind at removeScene/_unload so the engine
// never touches the handler afterwards.
LL_TYPE_INSTANCE_HOOK(
    Stage7ViewRemoveSceneHook,
    ll::memory::HookPriority::Normal,
    OreUI::View,
    &OreUI::View::$removeScene,
    void,
    ::OreUI::IScene const& scene
) {
    {
        std::lock_guard lock{state().mutex};
        if (state().viewRegistry != nullptr) {
            state().viewRegistry->notifyBindingsReleased();
        }
    }
    diagnostic::recordStage0("js", "event=view_remove_scene");
    origin(scene);
}

LL_TYPE_INSTANCE_HOOK(
    Stage7ViewUnloadHook,
    ll::memory::HookPriority::Normal,
    OreUI::View,
    &OreUI::View::_unload,
    void
) {
    {
        std::lock_guard lock{state().mutex};
        if (state().viewRegistry != nullptr) {
            state().viewRegistry->notifyBindingsReleased();
        }
    }
    diagnostic::recordStage0("js", "event=view_unload");
    origin();
}

// Stage 8-A: intercept OreUI::FacetRegistryFactory::createFacetRegistry (an
// exported, non-virtual MCAPI method — same hook pattern as View::initialize).
// The returned registry is handed to the facet bridge so the "dearoreui"
// facet can be registered into EVERY registry the game creates, with NO
// member-offset guessing. Optional hook: if it fails the mod still runs
// C++->JS only.
LL_TYPE_INSTANCE_HOOK(
    Stage8AFacetRegistryFactoryHook,
    ll::memory::HookPriority::Normal,
    OreUI::FacetRegistryFactory,
    &OreUI::FacetRegistryFactory::createFacetRegistry,
    std::unique_ptr<OreUI::IFacetRegistry>,
    OreUI::FacetRegistryLocation location
) {
    auto registry = origin(location);
    {
        std::lock_guard lock{state().mutex};
        if (state().onFacetRegistryCreated && registry) {
            state().onFacetRegistryCreated(registry.get());
        }
    }
    return registry;
}

LL_TYPE_INSTANCE_HOOK(
    Stage7ViewDestructorHook,
    ll::memory::HookPriority::Normal,
    OreUI::View,
    &OreUI::View::$dtor,
    void
) {
    {
        std::lock_guard lock{state().mutex};
        if (state().viewRegistry != nullptr) {
            // mGamefaceView is typed as cohtml::View* (TypedStorage scalar
            // specialization), so it converts directly.
            state().viewRegistry->onViewDestroyed(this->mGamefaceView);
        }
    }
    // Restore the binding vtable slots while the view memory is still alive
    // (origin() below is what actually releases it).
    BindingProbe::uninstall(this->mGamefaceView);
    diagnostic::recordStage0("js", "event=view_destroyed");
    origin();
}

// Stage 8 fix: when the OreUI page is unloaded, the engine releases the page's
// bindings by calling IViewListener::OnBindingsReleased. The cohtml view is
// often REUSED (not destroyed), so the $dtor hook never fires. We must unbind
// our native BindCall handler here, otherwise the engine frees the bindings
// while our handler is still registered -> crash when returning to the menu.
LL_TYPE_INSTANCE_HOOK(
    Stage7BindingsReleasedHook,
    ll::memory::HookPriority::Normal,
    OreUI::View,
    &OreUI::View::$OnBindingsReleased,
    void
) {
    {
        std::lock_guard lock{state().mutex};
        if (state().viewRegistry != nullptr) {
            state().viewRegistry->notifyBindingsReleased();
        }
    }
    diagnostic::recordStage0("js", "event=bindings_released");
    origin();
}

LL_TYPE_INSTANCE_HOOK(
    Stage7OnReadyForBindingsHook,
    ll::memory::HookPriority::Normal,
    OreUI::View,
    &OreUI::View::$OnReadyForBindings,
    void
) {
    origin();
    // The engine has created the page's script context: ExecuteScript is now
    // accepted. Release the deferred script queue in the host bridge.
    {
        std::lock_guard lock{state().mutex};
        if (state().viewRegistry != nullptr) {
            state().viewRegistry->markScriptContextReady();
        }
    }
    diagnostic::recordStage0("js", "event=on_ready_for_bindings");
}

LL_TYPE_INSTANCE_HOOK(
    Stage7TriggerEventHook,
    ll::memory::HookPriority::Normal,
    OreUI::View,
    &OreUI::View::$triggerEvent,
    void,
    std::string const& eventName,
    std::string const& eventData
) {
    // Diagnostic-only: cap volume so the event stream stays readable. Record
    // the first events plus anything that looks like a page/navigation event.
    static std::atomic<int> triggerCount{0};
    int const               n = triggerCount.fetch_add(1);
    bool const interesting    = eventName.find("Navigation") != std::string::npos
                             || eventName.find("Ready") != std::string::npos || eventName.find("Load") != std::string::npos
                             || eventName.find("Scene") != std::string::npos;
    if (n < 40 || interesting) {
        diagnostic::recordStage0("js", "event=trigger_entered\tname=" + eventName);
    }
    origin(eventName, eventData);
}

LL_TYPE_INSTANCE_HOOK(
    Stage0RouterChangeHook,
    ll::memory::HookPriority::Normal,
    OreUI::Router,
    &OreUI::Router::_onChange,
    void,
    std::optional<OreUI::RouterLocation> const& oldLocation,
    std::optional<OreUI::RouterLocation> const& currentLocation
) {
    // Stage 7.1 troubleshooting: record the route transition so we can see
    // which screens the game actually shows.
    diagnostic::recordStage0(
        "hook",
        "event=router_onchange_entered\t" + locationFields("old", oldLocation) + "\t"
            + locationFields("cur", currentLocation)
    );
    origin(oldLocation, currentLocation);
}

LL_TYPE_INSTANCE_HOOK(
    Stage0RouterPushHook,
    ll::memory::HookPriority::Normal,
    OreUI::Router,
    &OreUI::Router::_pushRoute,
    bool,
    std::string const&            route,
    OreUI::Router::RouterPushMode mode
) {
    diagnostic::recordStage0("hook", "event=router_push_entered\troute=" + route);
    return origin(route, mode);
}

LL_TYPE_INSTANCE_HOOK(
    Stage0RouterReplaceHook,
    ll::memory::HookPriority::Normal,
    OreUI::Router,
    &OreUI::Router::replaceRoute,
    bool,
    std::string const& route
) {
    diagnostic::recordStage0("hook", "event=router_replace_entered\troute=" + route);
    return origin(route);
}

LL_TYPE_INSTANCE_HOOK(
    Stage0RouterBackHook,
    ll::memory::HookPriority::Normal,
    OreUI::Router,
    &OreUI::Router::goBack,
    void
) {
    diagnostic::recordStage0("hook", "event=router_back_entered");
    origin();
}

bool allInstalled() {
    auto const& value = state();
    return value.techStack && value.scene && value.routeChange && value.routePush && value.routeReplace
        && value.routeBack && value.routerDestructorThunk && value.clientUpdate && value.viewInitialize
        && value.onReadyForBindings && value.viewDestructor && value.bindingsReleased && value.viewRemoveScene
        && value.viewUnload;
}

bool noneInstalled() {
    auto const& value = state();
    return !value.techStack && !value.scene && !value.routeChange && !value.routePush && !value.routeReplace
        && !value.routeBack && !value.routerDestructorThunk && !value.clientUpdate && !value.viewInitialize
        && !value.onReadyForBindings && !value.viewDestructor && !value.bindingsReleased && !value.viewRemoveScene
        && !value.viewUnload;
}

bool removeInstalled() {
    auto& value = state();
    if (value.viewUnload && Stage7ViewUnloadHook::unhook()) value.viewUnload = false;
    if (value.viewRemoveScene && Stage7ViewRemoveSceneHook::unhook()) value.viewRemoveScene = false;
    if (value.bindingsReleased && Stage7BindingsReleasedHook::unhook()) value.bindingsReleased = false;
    if (value.onReadyForBindings && Stage7OnReadyForBindingsHook::unhook()) value.onReadyForBindings = false;
    if (value.viewDestructor && Stage7ViewDestructorHook::unhook()) value.viewDestructor = false;
    if (value.facetRegistryFactory && Stage8AFacetRegistryFactoryHook::unhook()) value.facetRegistryFactory = false;
    if (value.routeBack && Stage0RouterBackHook::unhook()) value.routeBack = false;
    if (value.routerDestructorThunk && Stage1RouterDestructorThunkHook::unhook()) value.routerDestructorThunk = false;
    if (value.clientUpdate && Stage1ClientUpdateHook::unhook()) value.clientUpdate = false;
    if (value.routeReplace && Stage0RouterReplaceHook::unhook()) value.routeReplace = false;
    if (value.routePush && Stage0RouterPushHook::unhook()) value.routePush = false;
    if (value.routeChange && Stage0RouterChangeHook::unhook()) value.routeChange = false;
    if (value.scene && Stage0SceneProviderHook::unhook()) value.scene = false;
    if (value.techStack && Stage0TechStackHook::unhook()) value.techStack = false;
    if (value.viewInitialize && Stage7ViewInitializeHook::unhook()) value.viewInitialize = false;
    return noneInstalled();
}

void destroyAllContexts() {
    auto&                       adapterState = state();
    std::vector<api::ContextId> contexts;
    {
        std::lock_guard lock{adapterState.mutex};
        contexts.reserve(adapterState.routerContexts.size());
        for (auto const& [router, contextId] : adapterState.routerContexts) {
            static_cast<void>(router);
            contexts.push_back(contextId);
        }
        adapterState.routerContexts.clear();
    }
    if (adapterState.callback) {
        for (auto contextId : contexts) {
            adapterState.callback->onPageDestroyed(contextId);
        }
    }
}

} // namespace

OreUIHookAdapter::OreUIHookAdapter(
    IPageHookCallback&            callback,
    capability::ICapabilityQuery& capabilities,
    ipc::CoherentViewRegistry&    viewRegistry,
    diagnostic::DiagnosticLogger& logger,
    std::filesystem::path         dataDirectory
)
: mCallback(callback),
  mCapabilities(capabilities),
  mViewRegistry(viewRegistry),
  mLogger(logger),
  mDataDirectory(std::move(dataDirectory)),
  mProbe(logger) {}

OreUIHookAdapter::~OreUIHookAdapter() { static_cast<void>(uninstall()); }

void OreUIHookAdapter::setOnFacetRegistryCreated(std::function<void(void*)> callback) {
    std::lock_guard lock{state().mutex};
    state().onFacetRegistryCreated = std::move(callback);
}

bool OreUIHookAdapter::install() {
    auto& adapterState = state();
    {
        std::lock_guard lock{adapterState.mutex};
        adapterState.callback     = &mCallback;
        adapterState.probe        = &mProbe;
        adapterState.capabilities = &mCapabilities;
        adapterState.viewRegistry = &mViewRegistry;
    }
    if (allInstalled()) {
        return true;
    }

    adapterState.techStack = Stage0TechStackHook::hook() == 0;
    if (adapterState.techStack) adapterState.scene = Stage0SceneProviderHook::hook() == 0;
    if (adapterState.scene) adapterState.routeChange = Stage0RouterChangeHook::hook() == 0;
    if (adapterState.routeChange) adapterState.routePush = Stage0RouterPushHook::hook() == 0;
    if (adapterState.routePush) adapterState.routeReplace = Stage0RouterReplaceHook::hook() == 0;
    if (adapterState.routeReplace) adapterState.routeBack = Stage0RouterBackHook::hook() == 0;
    if (adapterState.routeBack) adapterState.routerDestructorThunk = Stage1RouterDestructorThunkHook::hook() == 0;
    if (adapterState.routerDestructorThunk) adapterState.clientUpdate = Stage1ClientUpdateHook::hook() == 0;
    if (adapterState.clientUpdate) adapterState.viewInitialize = Stage7ViewInitializeHook::hook() == 0;
    if (adapterState.viewInitialize) adapterState.onReadyForBindings = Stage7OnReadyForBindingsHook::hook() == 0;
    if (adapterState.onReadyForBindings) adapterState.viewDestructor = Stage7ViewDestructorHook::hook() == 0;
    if (adapterState.viewDestructor) adapterState.bindingsReleased = Stage7BindingsReleasedHook::hook() == 0;
    if (adapterState.bindingsReleased) adapterState.viewRemoveScene = Stage7ViewRemoveSceneHook::hook() == 0;
    if (adapterState.viewRemoveScene) adapterState.viewUnload = Stage7ViewUnloadHook::hook() == 0;

    // Stage 8-A: optional createFacetRegistry hook — its failure must not
    // block the mod (the mod still delivers C++->JS without it).
    bool const facetRegistryFactoryInstalled = Stage8AFacetRegistryFactoryHook::hook() == 0;
    adapterState.facetRegistryFactory        = facetRegistryFactoryInstalled;

    // Stage 7.1: triggerEvent is a diagnostic-only hook; its failure must not
    // block the mod (the ODS and vtable diagnostics already cover the rest).
    bool const triggerEventInstalled = Stage7TriggerEventHook::hook() == 0;

    // Stage 7.1 troubleshooting: log the result of every hook installation so a
    // failed view hook is immediately visible in diagnostics.
    mLogger.info("hook", "install_step")
        .withField("tech_stack", adapterState.techStack ? "ok" : "fail")
        .withField("scene", adapterState.scene ? "ok" : "fail")
        .withField("route_change", adapterState.routeChange ? "ok" : "fail")
        .withField("route_push", adapterState.routePush ? "ok" : "fail")
        .withField("route_replace", adapterState.routeReplace ? "ok" : "fail")
        .withField("route_back", adapterState.routeBack ? "ok" : "fail")
        .withField("router_dtor", adapterState.routerDestructorThunk ? "ok" : "fail")
        .withField("client_update", adapterState.clientUpdate ? "ok" : "fail")
        .withField("view_initialize", adapterState.viewInitialize ? "ok" : "fail")
        .withField("ready_for_bindings", adapterState.onReadyForBindings ? "ok" : "fail")
        .withField("view_destructor", adapterState.viewDestructor ? "ok" : "fail")
        .withField("bindings_released", adapterState.bindingsReleased ? "ok" : "fail")
        .withField("view_remove_scene", adapterState.viewRemoveScene ? "ok" : "fail")
        .withField("view_unload", adapterState.viewUnload ? "ok" : "fail")
        .withField("trigger_event", triggerEventInstalled ? "ok" : "fail")
        .withField("facet_registry_factory", facetRegistryFactoryInstalled ? "ok" : "fail")
        .emit();

    if (allInstalled()) {
        diagnostic::recordStage0("status", "event=hooks_installed");
        return true;
    }

    destroyAllContexts();
    {
        std::lock_guard lock{adapterState.mutex};
        adapterState.callback     = nullptr;
        adapterState.probe        = nullptr;
        adapterState.capabilities = nullptr;
        adapterState.viewRegistry = nullptr;
    }
    removeInstalled();
    diagnostic::recordStage0("status", "event=hooks_unavailable");
    return false;
}

bool OreUIHookAdapter::uninstall() {
    auto&       adapterState      = state();
    std::size_t remainingContexts = 0;
    {
        std::lock_guard lock{adapterState.mutex};
        remainingContexts = adapterState.routerContexts.size();
    }
    diagnostic::recordStage0(
        "hook_adapter",
        "event=uninstall_started\tremaining_contexts=" + std::to_string(remainingContexts)
    );
    destroyAllContexts();
    diagnostic::recordStage0("hook_adapter", "event=destroy_all_contexts_completed");
    {
        std::lock_guard lock{adapterState.mutex};
        adapterState.callback               = nullptr;
        adapterState.probe                  = nullptr;
        adapterState.capabilities           = nullptr;
        adapterState.viewRegistry           = nullptr;
        adapterState.onFacetRegistryCreated = nullptr;
    }

    if (noneInstalled()) return true;
    auto result = removeInstalled();
    diagnostic::recordStage0("status", result ? "event=hooks_removed" : "event=hooks_remove_failed");
    adapterState.observed.clear();
    return result;
}

bool OreUIHookAdapter::isInstalled() const { return allInstalled(); }

} // namespace dearoreui::hook
