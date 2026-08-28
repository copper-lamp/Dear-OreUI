#pragma once

#include "api/manifest/ResourceManifest.h"
#include "api/manifest/ScriptManifest.h"
#include "api/manifest/StyleSheetManifest.h"
#include "api/manifest/UiManifest.h"
#include "api/types/Id.h"
#include "api/types/DomNode.h"

#include <chrono>
#include <string>
#include <variant>

namespace dearoreui::registry {

struct ResourceEntry {
    api::RegistrationHandle               handle;
    api::ModId                            owner;
    api::ResourceManifest                 manifest;
    std::string                           payload;
    std::chrono::system_clock::time_point registeredAt;
};

struct ScriptEntry {
    api::RegistrationHandle               handle;
    api::ModId                            owner;
    api::ScriptManifest                   manifest;
    std::string                           source;
    std::chrono::system_clock::time_point registeredAt;
};

struct StyleSheetEntry {
    api::RegistrationHandle               handle;
    api::ModId                            owner;
    api::StyleSheetManifest               manifest;
    std::string                           source;
    std::chrono::system_clock::time_point registeredAt;
};

struct UiEntry {
    api::RegistrationHandle handle;
    api::ModId              owner;
    api::UiManifest         manifest;
    std::string             htmlBody;
    // M8.1.2: pre-rendered DomNode forest (component-registered UIs only).
    // Carries per-state cssText (stateStyles) that the htmlBody round-trip
    // cannot represent; injection prefers this over parsing htmlBody.
    std::vector<api::DomNode>          domNodes;
    std::chrono::system_clock::time_point registeredAt;
};

using RegistryEntry = std::variant<ResourceEntry, ScriptEntry, StyleSheetEntry, UiEntry>;

} // namespace dearoreui::registry
