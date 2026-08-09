#include "hook/Stage0OreUIHooks.h"

#include "diagnostic/Stage0TelemetryCompat.h"
#include "poc/Stage1NavigationPoc.h"

#include "ll/api/memory/Hook.h"

#include "mc/client/gui/ScreenTechStackSelector.h"
#include "mc/client/gui/TechStack.h"
#include "mc/client/gui/oreui/SceneProvider.h"
#include "mc/client/gui/oreui/routing/Router.h"
#include "mc/client/game/ClientInstance.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>

namespace dearoreui::hook {

namespace {

struct HookState {
    bool techStack{};
    bool scene{};
    bool routeChange{};
    bool routePush{};
    bool routeReplace{};
    bool routeBack{};
    bool routerDestructorThunk{};
    bool clientUpdate{};
};

HookState& state() {
    static HookState value;
    return value;
}

std::unordered_set<std::string>& observed() {
    static std::unordered_set<std::string> values;
    return values;
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

void recordOnce(std::string event, std::string fields) {
    auto key = event + "\t" + fields;
    if (!observed().insert(key).second) return;
    diagnostic::recordStage0(event, fields);
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
    poc::armStage1Navigation(
        router,
        *sceneStack,
        url == "/hbui/index.html" && location == OreUI::FacetRegistryLocation::OutOfGame
    );
    recordOnce(
        "scene",
        "url=" + url + "\troute_mode=" + std::to_string(static_cast<int>(mode)) + "\tlocation="
        + std::to_string(static_cast<int>(location)) + "\tcreated=" + (result ? "true" : "false")
    );
    return result;
}

LL_TYPE_INSTANCE_HOOK(
    Stage1RouterDestructorThunkHook,
    ll::memory::HookPriority::Normal,
    OreUI::Router,
    &OreUI::Router::$dtor,
    void
) {
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
    auto result = origin(isInitFinished);
    if (isInitFinished) poc::consumeStage1Navigation();
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
    if (currentLocation && currentLocation->getPath() == "/__bedrock__/start_screen") {
        poc::armStage1NavigationFromStartScreen(*this);
    }
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

}

bool installStage0OreUIHooks() {
    auto& value = state();
    if (allInstalled()) return true;

    value.techStack = Stage0TechStackHook::hook() == 0;
    if (value.techStack) value.scene = Stage0SceneProviderHook::hook() == 0;
    if (value.scene) value.routeChange = Stage0RouterChangeHook::hook() == 0;
    if (value.routeChange) value.routePush = Stage0RouterPushHook::hook() == 0;
    if (value.routePush) value.routeReplace = Stage0RouterReplaceHook::hook() == 0;
    if (value.routeReplace) value.routeBack = Stage0RouterBackHook::hook() == 0;
    if (value.routeBack) value.routerDestructorThunk = Stage1RouterDestructorThunkHook::hook() == 0;
    if (value.routerDestructorThunk) value.clientUpdate = Stage1ClientUpdateHook::hook() == 0;

    if (allInstalled()) {
        diagnostic::recordStage0("status", "event=hooks_installed");
        return true;
    }

    removeInstalled();
    diagnostic::recordStage0("status", "event=hooks_unavailable");
    return false;
}

bool uninstallStage0OreUIHooks() {
    if (noneInstalled()) return true;
    auto result = removeInstalled();
    diagnostic::recordStage0("status", result ? "event=hooks_removed" : "event=hooks_remove_failed");
    observed().clear();
    return result;
}

}
