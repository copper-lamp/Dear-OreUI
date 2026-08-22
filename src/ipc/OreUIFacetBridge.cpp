#include "ipc/OreUIFacetBridge.h"

#include "api/types/Error.h"
#include "diagnostic/Stage5IpcTelemetry.h"
#include "ipc/IpcMessage.h"
#include "ipc/JsPayloadHandler.h"

#include "mc/client/gui/oreui/interface/IFacet.h"
#include "mc/client/gui/oreui/interface/IFacetRegistry.h"
#include "mc/client/gui/oreui/interface/Status.h"

#include <chrono>
#include <cstdint>
#include <sstream>
#include <string_view>

namespace dearoreui::ipc {

namespace {

constexpr char const*               kFacetName = "dearoreui";
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

    OreUI::Status
    init(std::unordered_map<std::string, std::variant<double, bool, std::string>> const& payload) override {
        return mBridge.onFacetInit(payload) ? OreUI::Status::Ok : OreUI::Status::Error;
    }

private:
    OreUIFacetBridge& mBridge;
};

} // namespace

OreUIFacetBridge::OreUIFacetBridge(HostDispatcher& dispatcher, IHostBridge& bridge)
: mDispatcher(dispatcher),
  mBridge(bridge) {}

OreUIFacetBridge::~OreUIFacetBridge() = default;

char const* OreUIFacetBridge::facetName() const { return kFacetName; }

void OreUIFacetBridge::onFacetRegistryCreated(void* registryPtr) {
    if (registryPtr == nullptr) {
        diagnostic::recordStage5FacetRegistered(kFacetName, "registry_null", 0);
        return;
    }
    std::lock_guard lock{mMutex};
    if (mRegistered && registryPtr == mRegisteredRegistry) {
        // Same registry instance re-exposed (registry factory call again) —
        // already wired.
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
    mRegistered         = true;
    mRegisteredRegistry = registryPtr;
    diagnostic::recordStage5FacetRegistered(kFacetName, "ok", reinterpret_cast<std::uintptr_t>(registryPtr));
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
            api::RequestId{},
            api::ErrorCode::InvalidFormat,
            "facet params is not a valid IpcMessage"
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
           << "window.__DearOreUI__.bus.push(" << request.id.value() << ",\"" << escapeJsString(response) << "\");";
    static_cast<void>(mBridge.sendScript(request.contextId, script.str()));
    return true;
}

} // namespace dearoreui::ipc
