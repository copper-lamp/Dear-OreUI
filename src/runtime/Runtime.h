#pragma once

#include "api/DearOreUIApi.h"
#include "capability/StaticCapabilityQuery.h"
#include "hook/IPageHookCallback.h"
#include "hook/OreUIHookAdapter.h"
#include "page/PageContextManager.h"
#include "registry/ModRegistry.h"
#include "runtime/IRuntime.h"
#include "runtime/RuntimeConfig.h"

#include <memory>

namespace dearoreui::runtime {

class Runtime : public IRuntime, private hook::IPageHookCallback {
public:
    explicit Runtime(RuntimeConfig config);

    [[nodiscard]] bool initialize() override;
    [[nodiscard]] bool enable() override;
    [[nodiscard]] bool disable() override;

    [[nodiscard]] diagnostic::DiagnosticLogger& diagnostics() override;
    [[nodiscard]] capability::ICapabilityQuery& capabilities() override;
    [[nodiscard]] api::IDearOreUIApi* api() override;
    [[nodiscard]] page::IPageContextManager* pageManager() override;

private:
    // hook::IPageHookCallback
    [[nodiscard]] api::ContextId onPageCreated(
        std::string_view url, std::optional<api::RouterLocationSnapshot> location
    ) override;
    void onPageDestroyed(api::ContextId id) override;

    RuntimeConfig                     mConfig;
    capability::StaticCapabilityQuery mCapabilities;
    std::unique_ptr<registry::ModRegistry> mRegistry;
    std::unique_ptr<api::DearOreUIApi>     mApi;
    std::unique_ptr<page::PageContextManager> mPageManager;
    std::unique_ptr<hook::OreUIHookAdapter>   mHookAdapter;
    bool                              mInitialized{false};
    bool                              mEnabled{false};
};

} // namespace dearoreui::runtime
