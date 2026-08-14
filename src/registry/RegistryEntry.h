#pragma once

#include "api/manifest/ResourceManifest.h"
#include "api/manifest/ScriptManifest.h"
#include "api/manifest/StyleSheetManifest.h"
#include "api/manifest/UiManifest.h"
#include "api/types/Id.h"

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
    api::RegistrationHandle               handle;
    api::ModId                            owner;
    api::UiManifest                       manifest;
    std::string                           htmlBody;
    std::chrono::system_clock::time_point registeredAt;
};

using RegistryEntry = std::variant<ResourceEntry, ScriptEntry, StyleSheetEntry, UiEntry>;

} // namespace dearoreui::registry
