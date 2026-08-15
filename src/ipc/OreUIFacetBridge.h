#pragma once

#include "ipc/CoherentViewRegistry.h"
#include "ipc/HostDispatcher.h"
#include "ipc/IHostBridge.h"

#include <cstddef>
#include <mutex>
#include <string>
#include <unordered_map>
#include <variant>

namespace dearoreui::ipc {

// Stage 8-A: native OreUI facet JS->C++ channel.
//
// Registers a "dearoreui" facet into the game's OWN IFacetRegistry (owned by
// OreUI::View). The page then uses the vanilla facet protocol
// (engine.trigger("facet:request", "dearoreui", id, {params: ...})) and the
// game's pre-registered facet:request handler dispatches it to our
// DearOreUIFacet::init — NO RegisterForEvent/BindCall involved (those crash
// this client). Requests are routed into HostDispatcher through
// handleJsPayload; the serialized response is pushed back into the page via
// IHostBridge::sendScript (ExecuteScript) into window.__DearOreUI__.bus.
//
// OreUI::View member layout is probed at runtime (Phase 1) before the facet
// is registered: only when (view + kOreUIViewGamefaceViewOffset) equals the
// cohtml::View captured by CoherentViewRegistry is the derived
// kOreUIViewFacetRegistryOffset trusted. Any mismatch degrades safely (no
// registration, facet/layout_mismatch telemetry) and the existing C++->JS
// channel keeps working.
class OreUIFacetBridge {
public:
    // Providers for the game's vftable addresses, injected by Runtime (the
    // test binary does not link the game, so it leaves them null and relies
    // on the gameface-view-pointer scan).
    using VftableProvider = void* (*)();

    OreUIFacetBridge(
        HostDispatcher&        dispatcher,
        CoherentViewRegistry&  viewRegistry,
        IHostBridge&           bridge
    );
    ~OreUIFacetBridge();

    // Wired by Runtime: OreUI::FacetRegistry::$vftable and
    // OreUI::View::$vftableForIViewListener. Null providers disable the
    // vftable-identity probes (fall back to the pointer scan).
    void setVftableProviders(VftableProvider facetRegistryProvider, VftableProvider iViewListenerProvider);

    // Wired by Runtime to CoherentViewRegistry::setOnOreUIViewRegistered.
    // Called on the game thread when an OreUI::View wrapper is initialized.
    void onOreUIViewRegistered(void* oreuiView);

    // Called by DearOreUIFacet::init (game thread) with the JS payload map.
    // Extracts "params" (an IpcMessage JSON string), dispatches it, and pushes
    // the serialized response back to the page bus. Returns false on hard
    // protocol failure (maps to OreUI::Status::Error -> facet:error to JS).
    [[nodiscard]] bool onFacetInit(
        std::unordered_map<std::string, std::variant<double, bool, std::string>> const& payload
    );

    [[nodiscard]] char const* facetName() const;

private:
    // Locates the view's IFacetRegistry without hardcoding member offsets:
    //   1. scan for a pointer whose target vtable == FacetRegistry::$vftable
    //      (deterministic identity, independent of member-assignment timing);
    //   2. else locate the IViewListener subobject via its vftable and derive
    //      mFacetRegistry = listenerOffset + 8 (mGamefaceView) + 128 (member
    //      chain: mRenderer+mInputHandler+mUrl+mScenes+mBedrockInputSource+
    //      mClientInstance+mKeyboardManager+mFacetBinder);
    //   3. else scan for the captured cohtml::View pointer and use the same
    //      +128 member chain.
    [[nodiscard]] void* locateFacetRegistry(void* oreuiView, std::string& probeMethod);

    void registerIntoRegistry(void* registryPtr);

    HostDispatcher&       mDispatcher;
    CoherentViewRegistry& mViewRegistry;
    IHostBridge&          mBridge;

    VftableProvider mFacetRegistryVftableProvider{nullptr};
    VftableProvider mIViewListenerVftableProvider{nullptr};

    std::mutex mMutex;
    bool       mRegistered{false};           // any registry registered so far
    void*      mRegisteredRegistry{nullptr}; // last registry we registered into
};

} // namespace dearoreui::ipc
