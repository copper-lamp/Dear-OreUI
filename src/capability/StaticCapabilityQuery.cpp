#include "capability/StaticCapabilityQuery.h"

namespace dearoreui::capability {

StaticCapabilityQuery::StaticCapabilityQuery() {
    using api::Capability;
    using api::CapabilityEntry;
    using api::SupportLevel;

    mCapabilities.set(CapabilityEntry{
        .capability = Capability::PageLifecycleObservation,
        .level      = SupportLevel::Supported,
        .note       = "Verified via ScreenTechStackSelector and SceneProvider hooks",
    });
    mCapabilities.set(CapabilityEntry{
        .capability = Capability::RouteNavigation,
        .level      = SupportLevel::Supported,
        .note       = "Verified replaceRoute from /__bedrock__/start_screen to /play/all",
    });
    mCapabilities.set(CapabilityEntry{
        .capability = Capability::DiagnosticFileOutput,
        .level      = SupportLevel::Supported,
        .note       = "Structured diagnostics and Stage 0 telemetry file output",
    });
    mCapabilities.set(CapabilityEntry{
        .capability = Capability::ResourceInterception,
        .level      = SupportLevel::Unknown,
        .note       = "Not yet verified",
    });
    mCapabilities.set(CapabilityEntry{
        .capability = Capability::ScriptInjection,
        .level      = SupportLevel::Unknown,
        .note       = "Not yet verified",
    });
    mCapabilities.set(CapabilityEntry{
        .capability = Capability::HostBridge,
        .level      = SupportLevel::Unknown,
        .note       = "Not yet verified",
    });
    mCapabilities.set(CapabilityEntry{
        .capability = Capability::ExperimentalBundlePatch,
        .level      = SupportLevel::Unsupported,
        .note       = "High-risk capability, requires explicit verification",
    });
    mCapabilities.set(CapabilityEntry{
        .capability = Capability::ModManifestApi,
        .level      = SupportLevel::Supported,
        .note       = "Mod manifest validation and JSON parsing",
    });
    mCapabilities.set(CapabilityEntry{
        .capability = Capability::ResourceRegistration,
        .level      = SupportLevel::Experimental,
        .note       = "In-memory registry only, no real page injection yet",
    });
    mCapabilities.set(CapabilityEntry{
        .capability = Capability::ScriptRegistration,
        .level      = SupportLevel::Experimental,
        .note       = "In-memory registry only, no real page injection yet",
    });
    mCapabilities.set(CapabilityEntry{
        .capability = Capability::StyleSheetRegistration,
        .level      = SupportLevel::Experimental,
        .note       = "In-memory registry only, no real page injection yet",
    });
}

api::SupportLevel StaticCapabilityQuery::query(api::Capability capability) const {
    return mCapabilities.query(capability);
}

api::CapabilitySet StaticCapabilityQuery::all() const {
    return mCapabilities;
}

} // namespace dearoreui::capability
