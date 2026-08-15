#include "hook/Stage0OreUIHooks.h"

#include "capability/StaticCapabilityQuery.h"
#include "diagnostic/DiagnosticLogger.h"
#include "hook/OreUIHookAdapter.h"
#include "ipc/CoherentViewRegistry.h"

#include <optional>
#include <string_view>

namespace dearoreui::hook {

namespace {

struct NullPageHookCallback : IPageHookCallback {
    [[nodiscard]] api::ContextId onPageCreated(std::string_view, std::optional<api::RouterLocationSnapshot>) override {
        return api::ContextId{};
    }

    void onPageDestroyed(api::ContextId) override {}
};

OreUIHookAdapter& sharedAdapter() {
    static capability::StaticCapabilityQuery capabilities;
    static diagnostic::DiagnosticLogger      logger;
    static ipc::CoherentViewRegistry         viewRegistry;
    static NullPageHookCallback              callback;
    static OreUIHookAdapter                  adapter(callback, capabilities, viewRegistry, logger);
    return adapter;
}

} // namespace

bool installStage0OreUIHooks() {
    auto& adapter = sharedAdapter();
    if (adapter.isInstalled()) return true;
    return adapter.install();
}

bool uninstallStage0OreUIHooks() { return sharedAdapter().uninstall(); }

} // namespace dearoreui::hook
