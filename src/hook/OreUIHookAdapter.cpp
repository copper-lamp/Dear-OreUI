#include "hook/OreUIHookAdapter.h"

#include "diagnostic/DirectFileLog.h"
#include "diagnostic/Stage0TelemetryCompat.h"
#include "poc/Stage1NavigationPoc.h"

#include "ll/api/memory/Hook.h"

#include "mc/client/gui/ScreenTechStackSelector.h"
#include "mc/client/gui/TechStack.h"
#include "mc/client/gui/oreui/SceneProvider.h"
#include "mc/client/gui/oreui/routing/Router.h"
#include "mc/client/game/ClientInstance.h"

#include <atomic>
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

    std::mutex                                          mutex;
    std::unordered_map<OreUI::Router*, api::ContextId>  routerContexts;
    std::unordered_set<std::string>                     observed;
    IPageHookCallback*                                  callback{};
    std::filesystem::path                               dataDirectory;
};

AdapterState& state() {
    static AdapterState value;
    return value;
}

std::filesystem::path directLogPath(std::filesystem::path const& dataDirectory, std::string_view name) {
    return dataDirectory / "telemetry" / name;
}

void appendDirectLog(std::string_view name, std::string_view line) {
    auto const& dataDirectory = state().dataDirectory;
    if (dataDirectory.empty()) return;
    diagnostic::appendLineToFile(directLogPath(dataDirectory, name), line);
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
    return std::string(prefix) + "_path=" + location->getPath() + "\t" + std::string(prefix) + "_query="
         + location->getQuery() + "\t" + std::string(prefix) + "_fragment=" + location->getFragment();
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
    auto& adapterState = state();
    std::lock_guard lock{adapterState.mutex};
    if (!adapterState.callback) return;

    auto contextId = adapterState.callback->onPageCreated(
        url,
        makeLocationSnapshot(router.getCurrentLocation())
    );
    adapterState.routerContexts[&router] = contextId;
}

void notifyPageDestroyed(OreUI::Router& router) {
    auto& adapterState = state();
    std::lock_guard lock{adapterState.mutex};
    auto iterator = adapterState.routerContexts.find(&router);
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
    auto result = origin(screenName);
    recordOnce("tech_stack", "screen=" + screenName + "\tstack=" + techStackName(result));
    return result;
}

LL_TYPE_INSTANCE_HOOK(
    Stage0SceneProviderHook,
    ll::memory::HookPriority::Normal,
    OreUI::SceneProvider,
    &OreUI::SceneProvider::createScene,
    std::shared_ptr<AbstractScene>,
    std::string const& url,
    OreUI::Router& router,
    Bedrock::NotNullNonOwnerPtr<ISceneStack> const& sceneStack,
    OreUI::RouteMode mode,
    OreUI::FacetRegistryLocation location
) {
    auto result = origin(url, router, sceneStack, mode, location);
    poc::registerRouter(router, *sceneStack);
    // Stage 1 navigation PoC is disabled in stage 3 to preserve original UI behavior.
    // poc::armStage1Navigation(
    //     router,
    //     *sceneStack,
    //     url == "/hbui/index.html" && location == OreUI::FacetRegistryLocation::OutOfGame
    // );
    recordOnce(
        "scene",
        "url=" + url + "\troute_mode=" + std::to_string(static_cast<int>(mode)) + "\tlocation="
        + std::to_string(static_cast<int>(location)) + "\tcreated=" + (result ? "true" : "false")
    );
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
    appendDirectLog(
        "router-destructor.txt",
        "router_destructor_entered	this=" + std::to_string(reinterpret_cast<std::uintptr_t>(this))
    );
    notifyPageDestroyed(*this);
    poc::invalidateRouter(*this);
    origin();
    appendDirectLog(
        "router-destructor.txt",
        "router_destructor_exited	this=" + std::to_string(reinterpret_cast<std::uintptr_t>(this))
    );
}

LL_TYPE_INSTANCE_HOOK(
    Stage1ClientUpdateHook,
    ll::memory::HookPriority::Normal,
    ClientInstance,
    &ClientInstance::$update,
    bool,
    bool isInitFinished
) {
    auto result = origin(isInitFinished);
    // Stage 1 navigation PoC is disabled in stage 3.
    // if (isInitFinished) poc::consumeStage1Navigation();
    return result;
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
    origin(oldLocation, currentLocation);
    recordOnce("route_change", locationFields("old", oldLocation) + "\t" + locationFields("current", currentLocation));
    // Stage 1 navigation PoC is disabled in stage 3.
    // if (currentLocation && currentLocation->getPath() == "/__bedrock__/start_screen") {
    //     poc::armStage1NavigationFromStartScreen(*this);
    // }
}

LL_TYPE_INSTANCE_HOOK(
    Stage0RouterPushHook,
    ll::memory::HookPriority::Normal,
    OreUI::Router,
    &OreUI::Router::_pushRoute,
    bool,
    std::string const& route,
    OreUI::Router::RouterPushMode mode
) {
    auto result = origin(route, mode);
    recordOnce(
        "route_push",
        "route=" + route + "\tpush_mode=" + std::to_string(static_cast<int>(mode)) + "\tsuccess="
        + (result ? "true" : "false")
    );
    return result;
}

LL_TYPE_INSTANCE_HOOK(
    Stage0RouterReplaceHook,
    ll::memory::HookPriority::Normal,
    OreUI::Router,
    &OreUI::Router::replaceRoute,
    bool,
    std::string const& route
) {
    auto result = origin(route);
    recordOnce("route_replace", "route=" + route + "\tsuccess=" + (result ? "true" : "false"));
    return result;
}

LL_TYPE_INSTANCE_HOOK(
    Stage0RouterBackHook,
    ll::memory::HookPriority::Normal,
    OreUI::Router,
    &OreUI::Router::goBack,
    void
) {
    origin();
    recordOnce("route_back", {});
}

bool allInstalled() {
    auto const& value = state();
    return value.techStack && value.scene && value.routeChange && value.routePush && value.routeReplace
        && value.routeBack && value.routerDestructorThunk && value.clientUpdate;
}

bool noneInstalled() {
    auto const& value = state();
    return !value.techStack && !value.scene && !value.routeChange && !value.routePush && !value.routeReplace
        && !value.routeBack && !value.routerDestructorThunk && !value.clientUpdate;
}

bool removeInstalled() {
    auto& value = state();
    if (value.routeBack && Stage0RouterBackHook::unhook()) value.routeBack = false;
    if (value.routerDestructorThunk && Stage1RouterDestructorThunkHook::unhook()) value.routerDestructorThunk = false;
    if (value.clientUpdate && Stage1ClientUpdateHook::unhook()) value.clientUpdate = false;
    if (value.routeReplace && Stage0RouterReplaceHook::unhook()) value.routeReplace = false;
    if (value.routePush && Stage0RouterPushHook::unhook()) value.routePush = false;
    if (value.routeChange && Stage0RouterChangeHook::unhook()) value.routeChange = false;
    if (value.scene && Stage0SceneProviderHook::unhook()) value.scene = false;
    if (value.techStack && Stage0TechStackHook::unhook()) value.techStack = false;
    return noneInstalled();
}

void destroyAllContexts() {
    auto& adapterState = state();
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
    IPageHookCallback& callback,
    capability::ICapabilityQuery& capabilities,
    diagnostic::DiagnosticLogger& logger,
    std::filesystem::path dataDirectory
)
    : mCallback(callback), mCapabilities(capabilities), mLogger(logger),
      mDataDirectory(std::move(dataDirectory)) {}

OreUIHookAdapter::~OreUIHookAdapter() {
    static_cast<void>(uninstall());
}

bool OreUIHookAdapter::install() {
    auto& adapterState = state();
    {
        std::lock_guard lock{adapterState.mutex};
        adapterState.callback     = &mCallback;
        adapterState.dataDirectory = mDataDirectory;
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

    if (allInstalled()) {
        diagnostic::recordStage0("status", "event=hooks_installed");
        return true;
    }

    destroyAllContexts();
    {
        std::lock_guard lock{adapterState.mutex};
        adapterState.callback = nullptr;
    }
    removeInstalled();
    diagnostic::recordStage0("status", "event=hooks_unavailable");
    return false;
}

bool OreUIHookAdapter::uninstall() {
    auto& adapterState = state();
    std::size_t remainingContexts = 0;
    {
        std::lock_guard lock{adapterState.mutex};
        remainingContexts = adapterState.routerContexts.size();
    }
    appendDirectLog(
        "hook-adapter.txt",
        "uninstall_started\tremaining_contexts=" + std::to_string(remainingContexts)
    );
    diagnostic::recordStage0(
        "hook_adapter",
        "event=uninstall_started\tremaining_contexts=" + std::to_string(remainingContexts)
    );
    destroyAllContexts();
    appendDirectLog("hook-adapter.txt", "destroy_all_contexts_completed");
    diagnostic::recordStage0("hook_adapter", "event=destroy_all_contexts_completed");
    {
        std::lock_guard lock{adapterState.mutex};
        adapterState.callback = nullptr;
    }

    if (noneInstalled()) return true;
    auto result = removeInstalled();
    diagnostic::recordStage0("status", result ? "event=hooks_removed" : "event=hooks_remove_failed");
    adapterState.observed.clear();
    return result;
}

bool OreUIHookAdapter::isInstalled() const {
    return allInstalled();
}

} // namespace dearoreui::hook
