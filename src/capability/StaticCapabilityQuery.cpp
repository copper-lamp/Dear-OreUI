#include "capability/StaticCapabilityQuery.h"

namespace dearoreui::capability {

StaticCapabilityQuery::StaticCapabilityQuery() {
    using api::Capability;
    using api::CapabilityEntry;
    using api::SupportLevel;

    mCapabilities.set(
        CapabilityEntry{
            .capability = Capability::PageLifecycleObservation,
            .level      = SupportLevel::Supported,
            .note       = "Verified: PageContext created on SceneProvider::createScene and destroyed on Router::$dtor",
        }
    );
    mCapabilities.set(
        CapabilityEntry{
            .capability = Capability::RouteNavigation,
            .level      = SupportLevel::Supported,
            .note       = "Verified replaceRoute from /__bedrock__/start_screen to /play/all",
        }
    );
    mCapabilities.set(
        CapabilityEntry{
            .capability = Capability::DiagnosticFileOutput,
            .level      = SupportLevel::Supported,
            .note       = "Structured diagnostics and Stage 0 telemetry file output",
        }
    );
    mCapabilities.set(
        CapabilityEntry{
            .capability = Capability::ResourceInterception,
            .level      = SupportLevel::Experimental,
            .note       = "File-system snapshot reader implemented; runtime hook reader pending verification",
        }
    );
    mCapabilities.set(
        CapabilityEntry{
            .capability = Capability::ScriptInjection,
            .level      = SupportLevel::Experimental,
            .note       = "Runtime script generated; submitted via Coherent executeScript when HostBridge is available",
        }
    );
    mCapabilities.set(
        CapabilityEntry{
            .capability = Capability::HostBridge,
            .level      = SupportLevel::Unsupported,
            .note       = "Coherent JS execution entry not found in telemetry",
        }
    );
    mCapabilities.set(
        CapabilityEntry{
            .capability = Capability::ExperimentalBundlePatch,
            .level      = SupportLevel::Unsupported,
            .note       = "High-risk capability, requires explicit verification",
        }
    );
    mCapabilities.set(
        CapabilityEntry{
            .capability = Capability::ModManifestApi,
            .level      = SupportLevel::Supported,
            .note       = "Mod manifest validation and JSON parsing",
        }
    );
    mCapabilities.set(
        CapabilityEntry{
            .capability = Capability::ModRegistration,
            .level      = SupportLevel::Supported,
            .note       = "Mod-level registration, unregistration and enabled state",
        }
    );
    mCapabilities.set(
        CapabilityEntry{
            .capability = Capability::DependencyResolution,
            .level      = SupportLevel::Supported,
            .note       = "Topological sort, missing/version/cycle detection over registered mods",
        }
    );
    mCapabilities.set(
        CapabilityEntry{
            .capability = Capability::MultiModTransformPlanning,
            .level      = SupportLevel::Experimental,
            .note       = "Change planning and reporting available; real multi-mod page injection pending client verification",
        }
    );
    mCapabilities.set(
        CapabilityEntry{
            .capability = Capability::ResourceRegistration,
            .level      = SupportLevel::Experimental,
            .note       = "In-memory registry only, no real page injection yet",
        }
    );
    mCapabilities.set(
        CapabilityEntry{
            .capability = Capability::ScriptRegistration,
            .level      = SupportLevel::Experimental,
            .note       = "In-memory registry only, no real page injection yet",
        }
    );
    mCapabilities.set(
        CapabilityEntry{
            .capability = Capability::StyleSheetRegistration,
            .level      = SupportLevel::Experimental,
            .note       = "In-memory registry only, no real page injection yet",
        }
    );
    mCapabilities.set(
        CapabilityEntry{
            .capability = Capability::OverlayRegistration,
            .level      = SupportLevel::Experimental,
            .note       = "UI manifest registration and registry storage",
        }
    );
    mCapabilities.set(
        CapabilityEntry{
            .capability = Capability::UiMount,
            .level      = SupportLevel::Experimental,
            .note       = "Page-scoped UI mount planning and lifecycle tracking",
        }
    );
    mCapabilities.set(
        CapabilityEntry{
            .capability = Capability::DeclarativeOverlay,
            .level      = SupportLevel::Experimental,
            .note       = "Independent DOM overlay bootstrap script generation",
        }
    );
}

api::SupportLevel StaticCapabilityQuery::query(api::Capability capability) const {
    return mCapabilities.query(capability);
}

void StaticCapabilityQuery::setLevel(api::Capability capability, api::SupportLevel level, std::string note) {
    mCapabilities.set(api::CapabilityEntry{
        .capability = capability,
        .level      = level,
        .note       = std::move(note),
    });
}

api::CapabilitySet StaticCapabilityQuery::all() const { return mCapabilities; }

} // namespace dearoreui::capability
