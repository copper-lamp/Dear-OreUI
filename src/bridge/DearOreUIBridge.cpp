#define DEAROREUI_BUILDING
#include "bridge/BridgeInternal.h"
#include "bridge/DearOreUIBridge.h"

#include "api/IDearOreUIApi.h"
#include "api/manifest/ManifestValidator.h"

#include <atomic>

namespace dearoreui::bridge {

namespace {
std::atomic<api::IDearOreUIApi*> gApi{nullptr};
}

void setApi(api::IDearOreUIApi* api) { gApi.store(api, std::memory_order_release); }

void clearApi() { gApi.store(nullptr, std::memory_order_release); }

} // namespace dearoreui::bridge

extern "C" DEAROREUI_API DearOreUIBridgeResult DearOreUI_QueryApi(uint32_t requestedProtocolVersion) {
    DearOreUIBridgeResult result{};
    auto*                 api = dearoreui::bridge::gApi.load(std::memory_order_acquire);
    if (api == nullptr) {
        result.status          = DearOreUIBridge_ModNotLoaded;
        result.protocolVersion = dearoreui::api::DearOreUIProtocolVersion;
        return result;
    }
    result.protocolVersion = api->getProtocolVersion();
    if (!api->isReady()) {
        result.status = DearOreUIBridge_NotReady;
        return result;
    }
    if (requestedProtocolVersion != 0 && requestedProtocolVersion != result.protocolVersion) {
        result.status = DearOreUIBridge_VersionMismatch;
        return result;
    }
    result.status = DearOreUIBridge_Ok;
    result.api    = static_cast<void*>(api);
    return result;
}