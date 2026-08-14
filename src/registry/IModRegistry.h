#pragma once

#include "api/types/Id.h"
#include "api/types/Result.h"
#include "registry/ModRecord.h"
#include "registry/RegistryEntry.h"

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace dearoreui::registry {

class IModRegistry {
public:
    virtual ~IModRegistry() = default;

    // Mod-level registration and lifecycle.
    [[nodiscard]] virtual api::Result<api::ModId> registerMod(ModRecord record) = 0;
    [[nodiscard]] virtual bool                    unregisterMod(api::ModId id)  = 0;
    [[nodiscard]] virtual std::optional<ModRecord> findMod(api::ModId id) const = 0;
    [[nodiscard]] virtual bool                    isModRegistered(api::ModId id) const = 0;
    [[nodiscard]] virtual bool                    setModEnabled(api::ModId id, bool enabled) = 0;
    [[nodiscard]] virtual bool                    isModEnabled(api::ModId id) const = 0;
    [[nodiscard]] virtual std::vector<ModRecord>  allMods() const = 0;
    [[nodiscard]] virtual std::size_t             modCount() const = 0;

    // Entry-level registration and lifecycle.
    [[nodiscard]] virtual api::Result<api::RegistrationHandle> insert(ResourceEntry entry)   = 0;
    [[nodiscard]] virtual api::Result<api::RegistrationHandle> insert(ScriptEntry entry)     = 0;
    [[nodiscard]] virtual api::Result<api::RegistrationHandle> insert(StyleSheetEntry entry) = 0;
    [[nodiscard]] virtual api::Result<api::RegistrationHandle> insert(UiEntry entry)         = 0;

    [[nodiscard]] virtual bool        remove(api::RegistrationHandle handle) = 0;
    [[nodiscard]] virtual std::size_t removeAll(api::ModId owner)            = 0;

    [[nodiscard]] virtual std::optional<RegistryEntry>         find(api::RegistrationHandle handle) const = 0;
    [[nodiscard]] virtual std::vector<api::RegistrationHandle> findByOwner(api::ModId owner) const        = 0;
    [[nodiscard]] virtual std::vector<api::RegistrationHandle> findByNamespace(std::string_view ns) const = 0;
    [[nodiscard]] virtual std::vector<RegistryEntry>           listEntries() const                        = 0;
    [[nodiscard]] virtual std::vector<UiEntry>                 listUiEntries() const                      = 0;

    [[nodiscard]] virtual bool        hasConflict(api::ResourceManifest const& manifest) const = 0;
    [[nodiscard]] virtual bool        hasUiConflict(api::UiManifest const& manifest) const     = 0;
    [[nodiscard]] virtual std::size_t size() const                                             = 0;
    virtual void                      clear()                                                  = 0;
};

} // namespace dearoreui::registry
