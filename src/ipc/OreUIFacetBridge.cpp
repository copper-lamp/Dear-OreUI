#include "ipc/OreUIFacetBridge.h"

#include "api/types/Error.h"
#include "diagnostic/Stage5IpcTelemetry.h"
#include "ipc/IpcMessage.h"
#include "ipc/JsPayloadHandler.h"

#include "mc/client/gui/oreui/interface/IFacet.h"
#include "mc/client/gui/oreui/interface/IFacetRegistry.h"
#include "mc/client/gui/oreui/interface/Status.h"
#include "mc/client/gui/oreui/views/View.h"

#include <windows.h>

#include <chrono>
#include <cstdint>
#include <sstream>
#include <string_view>

namespace dearoreui::ipc {

namespace {

constexpr char const*            kFacetName           = "dearoreui";
constexpr std::chrono::milliseconds kFacetDispatchTimeout{3000};

// Escape a value into a double-quoted JS string literal (used when embedding
// the response JSON into the ExecuteScript push).
[[nodiscard]] std::string escapeJsString(std::string_view value) {
    std::ostringstream stream;
    for (char c : value) {
        switch (c) {
        case '\\':
            stream << "\\\\";
            break;
        case '"':
            stream << "\\\"";
            break;
        case '\n':
            stream << "\\n";
            break;
        case '\r':
            stream << "\\r";
            break;
        default:
            stream << c;
            break;
        }
    }
    return stream.str();
}

// Reads a pointer-sized member at a byte offset, guarded by SEH so the layout
// probe can never crash the client. C-style only (no C++ objects with
// destructors) so MSVC allows __try with /EHa.
bool tryReadPointer(void const* object, std::size_t offset, std::uintptr_t& out) {
    if (object == nullptr) {
        return false;
    }
    __try {
        out = *reinterpret_cast<std::uintptr_t const*>(static_cast<char const*>(object) + offset);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Reads the vptr of the object a candidate pointer references, guarded by SEH.
bool tryReadObjectVptr(void const* object, std::uintptr_t& vptrOut) {
    if (object == nullptr) {
        return false;
    }
    __try {
        vptrOut = *reinterpret_cast<std::uintptr_t const*>(object);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

constexpr std::size_t kProbeScanBytes  = 256;
constexpr std::size_t kProbeQwordDump  = 16;  // first N qwords logged on failure
// Member chain from mGamefaceView to mFacetRegistry (declaration order in the
// levimc header, all 8-aligned): mRenderer 8 + mInputHandler 8 + mUrl 32 +
// mScenes 24 + mBedrockInputSource 8 + mClientInstance 24 + mKeyboardManager 8
// + mFacetBinder 8 = 128.
constexpr std::size_t kGamefaceToFacetRegistryChain = 128;

// The native facet activated when the page triggers
// engine.trigger("facet:request", "dearoreui", id, {params: ...}).
// Owned by the game's FacetRegistry (unique_ptr) — the bridge must outlive it.
class DearOreUIFacet final : public OreUI::IFacet {
public:
    explicit DearOreUIFacet(OreUIFacetBridge& bridge) : mBridge(bridge) {}

    void unbind(OreUI::FacetBinder&) override {}

    void sync(OreUI::FacetBinder&, std::string const&) override {}

    bool update() override { return false; }

    char const* name() const override { return kFacetName; }

    OreUI::Status init(
        std::unordered_map<std::string, std::variant<double, bool, std::string>> const& payload
    ) override {
        return mBridge.onFacetInit(payload) ? OreUI::Status::Ok : OreUI::Status::Error;
    }

private:
    OreUIFacetBridge& mBridge;
};

} // namespace

OreUIFacetBridge::OreUIFacetBridge(HostDispatcher& dispatcher, CoherentViewRegistry& viewRegistry, IHostBridge& bridge)
: mDispatcher(dispatcher), mViewRegistry(viewRegistry), mBridge(bridge) {}

OreUIFacetBridge::~OreUIFacetBridge() = default;

void OreUIFacetBridge::setVftableProviders(VftableProvider facetRegistryProvider, VftableProvider iViewListenerProvider) {
    mFacetRegistryVftableProvider = facetRegistryProvider;
    mIViewListenerVftableProvider = iViewListenerProvider;
}

char const* OreUIFacetBridge::facetName() const { return kFacetName; }

void* OreUIFacetBridge::locateFacetRegistry(void* oreuiView, std::string& probeMethod) {
    // Probe 1: registry vftable identity scan. The view owns the registry via
    // unique_ptr<IFacetRegistry>; scan its prefix for a pointer whose target
    // vtable == OreUI::FacetRegistry::$vftable(). Deterministic — does not
    // depend on when mGamefaceView is assigned.
    if (mFacetRegistryVftableProvider != nullptr) {
        auto* vftable = mFacetRegistryVftableProvider();
        for (std::size_t off = 0; off + sizeof(void*) <= kProbeScanBytes; off += sizeof(void*)) {
            std::uintptr_t candidate = 0;
            if (!tryReadPointer(oreuiView, off, candidate)) {
                break;
            }
            if (candidate == 0) {
                continue;
            }
            std::uintptr_t vptr = 0;
            if (tryReadObjectVptr(reinterpret_cast<void*>(candidate), vptr)
                && vftable != nullptr
                && vptr == reinterpret_cast<std::uintptr_t>(vftable)) {
                probeMethod = "registry_vftable";
                return reinterpret_cast<void*>(candidate);
            }
        }
    }

    // Probe 2: locate the IViewListener subobject via its vftable, then
    // mFacetRegistry = listenerOffset + 8 (mGamefaceView) + chain.
    if (mIViewListenerVftableProvider != nullptr) {
        auto* listenerVftable = mIViewListenerVftableProvider();
        for (std::size_t off = 0; off + sizeof(void*) <= kProbeScanBytes; off += sizeof(void*)) {
            std::uintptr_t candidate = 0;
            if (!tryReadPointer(oreuiView, off, candidate)) {
                break;
            }
            if (listenerVftable != nullptr && candidate == reinterpret_cast<std::uintptr_t>(listenerVftable)) {
                std::uintptr_t registryPtr = 0;
                if (tryReadPointer(oreuiView, off + sizeof(void*) + kGamefaceToFacetRegistryChain, registryPtr)
                    && registryPtr != 0) {
                    probeMethod = "listener_vftable";
                    return reinterpret_cast<void*>(registryPtr);
                }
                break;
            }
        }
    }

    // Probe 3: scan for the captured cohtml::View pointer (== activeView),
    // then use the same +128 member chain.
    void* activeView = mViewRegistry.activeView();
    if (activeView != nullptr) {
        for (std::size_t off = 0; off + sizeof(void*) <= kProbeScanBytes; off += sizeof(void*)) {
            std::uintptr_t candidate = 0;
            if (!tryReadPointer(oreuiView, off, candidate)) {
                break;
            }
            if (candidate == reinterpret_cast<std::uintptr_t>(activeView)) {
                std::uintptr_t registryPtr = 0;
                if (tryReadPointer(oreuiView, off + kGamefaceToFacetRegistryChain, registryPtr)
                    && registryPtr != 0) {
                    probeMethod = "gameface_scan";
                    return reinterpret_cast<void*>(registryPtr);
                }
                break;
            }
        }
    }

    probeMethod = "none";
    return nullptr;
}

void OreUIFacetBridge::onOreUIViewRegistered(void* oreuiView) {
    if (oreuiView == nullptr) {
        return;
    }

    std::string probeMethod;
    void*       registryPtr = locateFacetRegistry(oreuiView, probeMethod);

    if (registryPtr == nullptr) {
        std::string dump;
        for (std::size_t i = 0; i < kProbeQwordDump; ++i) {
            std::uintptr_t value = 0;
            if (!tryReadPointer(oreuiView, i * sizeof(void*), value)) {
                break;
            }
            if (i > 0) {
                dump += ",";
            }
            dump += std::to_string(value);
        }
        diagnostic::recordStage5FacetProbe(probeMethod, dump);
        diagnostic::recordStage5FacetRegistered(kFacetName, "layout_mismatch", 0);
        return;
    }

    std::lock_guard lock{mMutex};
    if (mRegistered && registryPtr == mRegisteredRegistry) {
        // Same registry (reused view or duplicate initialize) — already wired.
        diagnostic::recordStage5FacetRegistered(kFacetName, "already_registered", 0);
        return;
    }
    registerIntoRegistry(registryPtr);
}

void OreUIFacetBridge::registerIntoRegistry(void* registryPtr) {
    auto* registry = static_cast<OreUI::IFacetRegistry*>(registryPtr);
    // registerFacet is a pure-virtual on IFacetRegistry; the game's
    // FacetRegistry implements it by pushing a FacetPrototype. Called on the
    // game thread (view initialize). std::function ABI matches (MSVC STL).
    registry->registerFacet(kFacetName, [this]() { return std::make_unique<DearOreUIFacet>(*this); });
    mRegistered          = true;
    mRegisteredRegistry = registryPtr;
    diagnostic::recordStage5FacetRegistered(
        kFacetName,
        "ok",
        reinterpret_cast<std::uintptr_t>(registryPtr)
    );
}

bool OreUIFacetBridge::onFacetInit(
    std::unordered_map<std::string, std::variant<double, bool, std::string>> const& payload
) {
    // The JS client sends either a plain diagnostics report (fire-and-forget)
    // or the whole IpcMessage JSON inside "params".
    auto found = payload.find("params");
    if (found == payload.end() || !std::holds_alternative<std::string>(found->second)) {
        diagnostic::recordStage5FacetActivated(kFacetName, false, 0);
        return false;
    }
    auto const& params = std::get<std::string>(found->second);
    diagnostic::recordStage5FacetActivated(kFacetName, !params.empty(), params.size());

    if (params.empty()) {
        return true;
    }

    // Plain (non-JSON) payloads are the Stage 7.1 diagnostics reports; let
    // handleJsPayload classify and fire-and-forget them (no response pushed).
    if (params.front() != '{') {
        static_cast<void>(handleJsPayload(mDispatcher, params, kFacetDispatchTimeout));
        return true;
    }

    // Protocol request: parse once for telemetry (id / context / method).
    auto parsed = parseIpcMessage(params);
    if (parsed.isErr()) {
        diagnostic::recordStage5FacetError(
            api::RequestId{}, api::ErrorCode::InvalidFormat, "facet params is not a valid IpcMessage"
        );
        return false;
    }
    auto const& request = parsed.value();
    diagnostic::recordStage5FacetPayload(request.id, request.contextId, request.method);

    if (request.type != IpcMessageType::Request) {
        // Non-request frames are fire-and-forget; nothing to answer.
        return true;
    }

    auto response = handleJsPayload(mDispatcher, params, kFacetDispatchTimeout);
    if (response.empty()) {
        return true;
    }

    diagnostic::recordStage5FacetResponse(request.id, response.size());

    // Push the serialized response into the page bus:
    //   window.__DearOreUI__ && window.__DearOreUI__.bus &&
    //       window.__DearOreUI__.bus.push(<id>, "<responseJson>")
    std::ostringstream script;
    script << "window.__DearOreUI__&&window.__DearOreUI__.bus&&"
           << "window.__DearOreUI__.bus.push(" << request.id.value() << ",\""
           << escapeJsString(response) << "\");";
    static_cast<void>(mBridge.sendScript(request.contextId, script.str()));
    return true;
}

} // namespace dearoreui::ipc
