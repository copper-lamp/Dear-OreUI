#include "poc/Stage1NavigationPoc.h"

#include "diagnostic/Stage0Telemetry.h"

#include "mc/client/gui/oreui/routing/Router.h"
#include "mc/client/gui/screens/interfaces/ISceneStack.h"

#include <atomic>
#include <algorithm>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

namespace dearoreui::poc {

namespace {

std::atomic_bool& completed() {
    static std::atomic_bool value{};
    return value;
}

std::atomic_bool& scheduled() {
    static std::atomic_bool value{};
    return value;
}

std::atomic_bool& updateObserved() {
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

std::atomic<std::uint64_t>& nextGeneration() {
    static std::atomic<std::uint64_t> value{};
    return value;
}

}

void registerRouter(OreUI::Router& router, ISceneStack& sceneStack) {
    auto const generation = nextGeneration().fetch_add(1) + 1;
    {
        std::lock_guard lock(mutex());
        routers()[&router] = RouterRecord{&router, &sceneStack, generation};
    }
    diagnostic::recordStage0(
        "router_lifecycle",
        "event=registered\tgeneration=" + std::to_string(generation)
    );
}

void invalidateRouter(OreUI::Router& router) {
    std::lock_guard lock(mutex());
    auto const iterator = routers().find(&router);
    if (iterator == routers().end()) return;
    diagnostic::recordStage0(
        "router_lifecycle",
        "event=invalidated\treason=router_destroyed\tgeneration=" + std::to_string(iterator->second.generation)
    );
    routers().erase(iterator);
}

void invalidateSceneStack(ISceneStack& sceneStack) {
    std::lock_guard lock(mutex());
    for (auto iterator = routers().begin(); iterator != routers().end();) {
        if (iterator->second.sceneStack != &sceneStack) {
            ++iterator;
            continue;
        }
        diagnostic::recordStage0(
            "router_lifecycle",
            "event=invalidated\treason=scene_stack_destroyed\tgeneration="
                + std::to_string(iterator->second.generation)
        );
        iterator = routers().erase(iterator);
    }
}

void armStage1Navigation(OreUI::Router& router, ISceneStack& sceneStack, bool isOutOfGameRootScene) {
    if (!isOutOfGameRootScene) return;
    if (completed().load() || scheduled().exchange(true)) return;

    registerRouter(router, sceneStack);
    RouterRecord record;
    {
        std::lock_guard lock(mutex());
        record = routers().at(&router);
    }
    diagnostic::recordStage0(
        "poc_navigate",
        "event=armed\tgeneration=" + std::to_string(record.generation)
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
        if (!scheduled().load() || completed().load()) return;
        scheduled().store(false);
        auto const iterator = std::find_if(routers().begin(), routers().end(), [](auto const& entry) {
            return entry.second.sceneStack != nullptr;
        });
        if (iterator == routers().end()) {
            diagnostic::recordStage0("poc_navigate", "event=blocked\treason=router_lifetime_unavailable");
            completed().store(true);
            return;
        }
        record = iterator->second;
    }

    diagnostic::recordStage0("poc_navigate", "event=executing");
    auto const route = std::string{"/play/all?dirtyLevelId="};
    diagnostic::recordStage0("poc_navigate", "event=requested\troute=" + route);
    auto const result = record.router->replaceRoute(route);
    diagnostic::recordStage0(
        "poc_navigate",
        "event=completed\tsuccess=" + std::string(result ? "true" : "false")
    );
    completed().store(true);
}

void stopStage1Navigation() {
    std::lock_guard lock(mutex());
    routers().clear();
    scheduled().store(false);
    updateObserved().store(false);
    completed().store(false);
    diagnostic::recordStage0("poc_navigate", "event=stopped");
}

}
