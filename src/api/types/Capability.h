#pragma once

#include "api/types/Version.h"

#include <map>
#include <optional>
#include <string>

namespace dearoreui::api {

enum class Capability {
    None,
    PageLifecycleObservation,
    RouteNavigation,
    ResourceInterception,
    ScriptInjection,
    HostBridge,
    DiagnosticFileOutput,
    ExperimentalBundlePatch,
    ModManifestApi,
    ModRegistration,
    DependencyResolution,
    MultiModTransformPlanning,
    ResourceRegistration,
    ScriptRegistration,
    StyleSheetRegistration,
    OverlayRegistration,
    UiMount,
    DeclarativeOverlay,
    ComponentLibrary,
    ComponentRegistration,
    JsToNativeCall,
};

enum class SupportLevel {
    Unknown,
    Unsupported,
    Experimental,
    Supported,
};

struct CapabilityEntry {
    Capability             capability{Capability::None};
    SupportLevel           level{SupportLevel::Unknown};
    std::optional<Version> verifiedSince;
    std::string            note;
};

class CapabilitySet {
public:
    CapabilitySet() = default;

    explicit CapabilitySet(std::map<Capability, CapabilityEntry> entries) : mEntries(std::move(entries)) {}

    [[nodiscard]] SupportLevel query(Capability capability) const {
        auto iterator = mEntries.find(capability);
        return iterator == mEntries.end() ? SupportLevel::Unknown : iterator->second.level;
    }

    [[nodiscard]] bool isSupported(Capability capability) const { return query(capability) == SupportLevel::Supported; }

    [[nodiscard]] bool isExperimentalOrSupported(Capability capability) const {
        auto level = query(capability);
        return level == SupportLevel::Experimental || level == SupportLevel::Supported;
    }

    [[nodiscard]] std::map<Capability, CapabilityEntry> const& all() const { return mEntries; }

    void set(CapabilityEntry entry) { mEntries[entry.capability] = std::move(entry); }

private:
    std::map<Capability, CapabilityEntry> mEntries;
};

} // namespace dearoreui::api
