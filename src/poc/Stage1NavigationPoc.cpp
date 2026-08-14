#include "poc/Stage1NavigationPoc.h"
#include "poc/Stage1NavigationState.h"

#include "diagnostic/Stage0TelemetryCompat.h"

#include "mc/client/gui/oreui/routing/Router.h"
#include "mc/client/gui/screens/interfaces/ISceneStack.h"

#include <atomic>
#include <algorithm>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace dearoreui::poc {

namespace {

Stage1NavigationState& navigationState() {
    static Stage1NavigationState value;
    return value;
}

std::atomic_bool& updateObserved() {
    static std::atomic_bool value{};
    return value;
}

std::atomic_bool& waitingRecorded() {
    static std::atomic_bool value{};
    return value;
}

struct RouterRecord {
    OreUI::Router* router{};
    ISceneStack* sceneStack{};
    std::uint64_t generation{};
};

std::mutex& mutex() {
    static std::mutex value;
    return value;
}

std::unordered_map<OreUI::Router*, RouterRecord>& routers() {
    static std::unordered_map<OreUI::Router*, RouterRecord> value;
    return value;
}

std::optional<RouterRecord>& pendingRouter() {
    static std::optional<RouterRecord> value;
    return value;
}

std::atomic<std::uint64_t>& nextGeneration() {
    static std::atomic<std::uint64_t> value{};
    return value;
}

}

void registerRouter(OreUI::Router& router, ISceneStack& sceneStack) {
    auto const generation = nextGeneration().fetch_add(1) + 1;
    std::lock_guard lock(mutex());
    routers()[&router] = RouterRecord{&router, &sceneStack, generation};
}

void invalidateRouter(OreUI::Router& router) {
    std::lock_guard lock(mutex());
    routers().erase(&router);
}

void invalidateSceneStack(ISceneStack& sceneStack) {
    std::lock_guard lock(mutex());
    for (auto iterator = routers().begin(); iterator != routers().end();) {
        if (iterator->second.sceneStack != &sceneStack) {
            ++iterator;
            continue;
        }
        iterator = routers().erase(iterator);
    }
}

void armStage1Navigation(OreUI::Router& router, ISceneStack& sceneStack, bool isOutOfGameRootScene) {
    if (!isOutOfGameRootScene) return;
    registerRouter(router, sceneStack);
    armStage1NavigationFromStartScreen(router);
}

void armStage1NavigationFromStartScreen(OreUI::Router& router) {
    if (!navigationState().trySchedule()) {
        diagnostic::recordStage0("poc_navigate", "event=skipped\treason=navigation_already_started");
        return;
    }

    RouterRecord record;
    {
        std::lock_guard lock(mutex());
        auto const iterator = routers().find(&router);
        if (iterator != routers().end()) {
            record = iterator->second;
        } else {
            record = RouterRecord{&router, nullptr, nextGeneration().fetch_add(1) + 1};
            routers()[&router] = record;
            diagnostic::recordStage0(
                "router_lifecycle",
                "event=registered\tsource=start_screen\tgeneration=" + std::to_string(record.generation)
            );
        }
        pendingRouter() = record;
    }
    waitingRecorded().store(false);
    diagnostic::recordStage0(
        "poc_navigate",
        "event=armed\tsource=start_screen\tgeneration=" + std::to_string(record.generation)
    );
    diagnostic::recordStage0("poc_navigate", "event=scheduled");

    static_cast<void>(record);
}

void consumeStage1Navigation() {
    if (!updateObserved().exchange(true)) {
        diagnostic::recordStage0("poc_navigate", "event=client_update_seen");
    }

    RouterRecord record;
    {
        std::lock_guard lock(mutex());
        if (!navigationState().isScheduled()) return;
        if (!pendingRouter()) {
            diagnostic::recordStage0("poc_navigate", "event=blocked\treason=router_lifetime_unavailable");
            navigationState().complete();
            return;
        }
        record = *pendingRouter();
        auto const iterator = routers().find(record.router);
        if (
            iterator == routers().end() || iterator->second.generation != record.generation
        ) {
            diagnostic::recordStage0("poc_navigate", "event=blocked\treason=router_lifetime_unavailable");
            pendingRouter().reset();
            navigationState().complete();
            return;
        }
    }

    auto const location = record.router->getCurrentLocation();
    auto const path = location ? location->getPath() : std::string{"none"};
    if (!location || path != "/__bedrock__/start_screen") {
        if (!waitingRecorded().exchange(true)) {
            diagnostic::recordStage0(
                "poc_navigate",
                "event=waiting\treason=current_route_not_start_screen\tpath=" + path
            );
        }
        return;
    }

    {
        std::lock_guard lock(mutex());
        if (!pendingRouter()) return;
        auto const iterator = routers().find(record.router);
        if (iterator == routers().end() || iterator->second.generation != record.generation) {
            diagnostic::recordStage0("poc_navigate", "event=blocked\treason=router_lifetime_unavailable");
            pendingRouter().reset();
            navigationState().complete();
            return;
        }
        if (!navigationState().tryBeginExecution()) return;
        pendingRouter().reset();
    }

    diagnostic::recordStage0(
        "poc_navigate",
        "event=ready\tgeneration=" + std::to_string(record.generation) + "\tpath=" + path
    );
    diagnostic::recordStage0("poc_navigate", "event=executing");
    auto const route = std::string{"/play/all?dirtyLevelId="};
    diagnostic::recordStage0(
        "poc_navigate",
        "event=requested\tgeneration=" + std::to_string(record.generation) + "\tfrom_path=" + path + "\troute="
            + route
    );
    auto const result = record.router->replaceRoute(route);
    diagnostic::recordStage0(
        "poc_navigate",
        "event=completed\tsuccess=" + std::string(result ? "true" : "false")
    );
    navigationState().complete();
}

void stopStage1Navigation() {
    std::lock_guard lock(mutex());
    routers().clear();
    pendingRouter().reset();
    navigationState().reset();
    updateObserved().store(false);
    waitingRecorded().store(false);
    diagnostic::recordStage0("poc_navigate", "event=stopped");
}

}
