#pragma once

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
// Registers a "dearoreui" facet into the game's OWN IFacetRegistry. The page
// then uses the vanilla facet protocol
// (engine.trigger("facet:request", "dearoreui", id, {params: ...})) and the
// game's pre-registered facet:request handler dispatches it to our
// DearOreUIFacet::init — NO RegisterForEvent/BindCall involved (those crash
// this client). Requests are routed into HostDispatcher through
// handleJsPayload; the serialized response is pushed back into the page via
// IHostBridge::sendScript (ExecuteScript) into window.__DearOreUI__.bus.
//
// The registry is obtained WITHOUT any member-offset guessing: the hook
// module intercepts OreUI::FacetRegistryFactory::createFacetRegistry (an
// exported, non-virtual MCAPI method) and hands each freshly created registry
// to onFacetRegistryCreated, where the facet is registered via the interface's
// pure-virtual registerFacet (vtable dispatch, no symbol dependency).
class OreUIFacetBridge {
public:
    OreUIFacetBridge(
        HostDispatcher& dispatcher,
        IHostBridge&    bridge
    );
    ~OreUIFacetBridge();

    // Called by the createFacetRegistry hook (game thread) for every fresh
    // IFacetRegistry the game creates. Registers the "dearoreui" facet into
    // it (idempotent per registry instance).
    void onFacetRegistryCreated(void* registryPtr);

    // Called by DearOreUIFacet::init (game thread) with the JS payload map.
    // Extracts "params" (an IpcMessage JSON string), dispatches it, and pushes
    // the serialized response back to the page bus. Returns false on hard
    // protocol failure (maps to OreUI::Status::Error -> facet:error to JS).
    [[nodiscard]] bool onFacetInit(
        std::unordered_map<std::string, std::variant<double, bool, std::string>> const& payload
    );

    [[nodiscard]] char const* facetName() const;

private:
    void registerIntoRegistry(void* registryPtr);

    HostDispatcher& mDispatcher;
    IHostBridge&    mBridge;

    std::mutex mMutex;
    bool       mRegistered{false};           // any registry registered so far
    void*      mRegisteredRegistry{nullptr}; // last registry we registered into
};

} // namespace dearoreui::ipc
