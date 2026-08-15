#pragma once

#include "api/DearOreUIApi.h"
#include "capability/StaticCapabilityQuery.h"
#include "facet/FacetRegistry.h"
#include "hook/IPageHookCallback.h"
#include "hook/OreUIHookAdapter.h"
#include "inject/IPageInjector.h"
#include "ipc/CoherentHostBridge.h"
#include "ipc/CoherentViewRegistry.h"
#include "ipc/HostDispatcher.h"
#include "ipc/HostMethodRegistry.h"
#include "ipc/IHostBridge.h"
#include "ipc/LoopbackWsServer.h"
#include "page/PageContextManager.h"
#include "registry/ModRegistry.h"
#include "resource/IResourceIndex.h"
#include "runtime/IRuntime.h"
#include "runtime/RuntimeConfig.h"
#include "source/ISourceReader.h"
#include "transform/ChangePlanner.h"
#include "transform/PageTransformer.h"
#include "ui/MountManager.h"
#include "ui/UiPlanner.h"

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
    [[nodiscard]] api::IDearOreUIApi*           api() override;
    [[nodiscard]] page::IPageContextManager*    pageManager() override;
    [[nodiscard]] ipc::HostDispatcher*          hostDispatcher();

private:
    // hook::IPageHookCallback
    [[nodiscard]] api::ContextId
         onPageCreated(std::string_view url, std::optional<api::RouterLocationSnapshot> location) override;
    void onPageDestroyed(api::ContextId id) override;

    void runStage4Injection(api::ContextId id, api::PageInfo const& info);

    // Stage 7.1: register the built-in demo overlay for real-client display
    // verification. Gated by RuntimeConfig::enableDemoOverlay.
    void registerDemoOverlay();

    RuntimeConfig                             mConfig;
    capability::StaticCapabilityQuery         mCapabilities;
    std::unique_ptr<registry::ModRegistry>    mRegistry;
    std::unique_ptr<ipc::HostMethodRegistry>  mHostMethodRegistry;
    std::unique_ptr<facet::FacetRegistry>     mFacetRegistry;
    std::unique_ptr<api::DearOreUIApi>        mApi;
    std::unique_ptr<page::PageContextManager> mPageManager;
    std::unique_ptr<hook::OreUIHookAdapter>   mHookAdapter;
    std::unique_ptr<source::ISourceReader>      mSourceReader;
    std::unique_ptr<transform::ChangePlanner>   mChangePlanner;
    std::unique_ptr<transform::PageTransformer> mPageTransformer;
    std::unique_ptr<inject::IPageInjector>      mInjector;
    std::unique_ptr<ui::UiPlanner>              mUiPlanner;
    std::unique_ptr<ui::MountManager>           mMountManager;
    std::unique_ptr<ui::IMountHost>             mMountHost;
    std::unique_ptr<ipc::HostDispatcher>        mHostDispatcher;
    std::unique_ptr<ipc::IHostBridge>           mHostBridge;
    std::unique_ptr<ipc::CoherentViewRegistry>  mViewRegistry;
    std::unique_ptr<ipc::LoopbackWsServer>      mWsServer; // Stage 8 JS->C++ channel
    bool                                        mInitialized{false};
    bool                                        mEnabled{false};
};

} // namespace dearoreui::runtime
