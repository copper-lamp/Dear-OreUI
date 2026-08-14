#pragma once

#include "registry/IModRegistry.h"

#include <atomic>
#include <mutex>
#include <unordered_map>

namespace dearoreui::registry {

class ModRegistry : public IModRegistry {
public:
    ModRegistry() = default;

    // Mod-level registration and lifecycle.
    [[nodiscard]] api::Result<api::ModId> registerMod(ModRecord record) override;
    [[nodiscard]] bool                    unregisterMod(api::ModId id) override;
    [[nodiscard]] std::optional<ModRecord> findMod(api::ModId id) const override;
    [[nodiscard]] bool                    isModRegistered(api::ModId id) const override;
    [[nodiscard]] bool                    setModEnabled(api::ModId id, bool enabled) override;
    [[nodiscard]] bool                    isModEnabled(api::ModId id) const override;
    [[nodiscard]] std::vector<ModRecord>  allMods() const override;
    [[nodiscard]] std::size_t             modCount() const override;

    // Entry-level registration and lifecycle.
    [[nodiscard]] api::Result<api::RegistrationHandle> insert(ResourceEntry entry) override;
    [[nodiscard]] api::Result<api::RegistrationHandle> insert(ScriptEntry entry) override;
    [[nodiscard]] api::Result<api::RegistrationHandle> insert(StyleSheetEntry entry) override;

    [[nodiscard]] bool        remove(api::RegistrationHandle handle) override;
    [[nodiscard]] std::size_t removeAll(api::ModId owner) override;

    [[nodiscard]] std::optional<RegistryEntry>         find(api::RegistrationHandle handle) const override;
    [[nodiscard]] std::vector<api::RegistrationHandle> findByOwner(api::ModId owner) const override;
    [[nodiscard]] std::vector<api::RegistrationHandle> findByNamespace(std::string_view ns) const override;
    [[nodiscard]] std::vector<RegistryEntry>           listEntries() const override;

    [[nodiscard]] bool        hasConflict(api::ResourceManifest const& manifest) const override;
    [[nodiscard]] std::size_t size() const override;
    void                      clear() override;

private:
    struct ConflictKey {
        std::string modNamespace;
        std::string path;

        [[nodiscard]] bool operator==(ConflictKey const& other) const {
            return modNamespace == other.modNamespace && path == other.path;
        }
    };

    struct ConflictKeyHash {
        [[nodiscard]] std::size_t operator()(ConflictKey const& key) const {
            auto h1 = std::hash<std::string>{}(key.modNamespace);
            auto h2 = std::hash<std::string>{}(key.path);
            return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
        }
    };

    using ConflictIndex = std::unordered_map<ConflictKey, api::RegistrationHandle, ConflictKeyHash>;

    template <typename Entry>
    [[nodiscard]] api::Result<api::RegistrationHandle> insertImpl(Entry entry);

    template <typename Entry>
    [[nodiscard]] static ConflictKey conflictKeyFor(Entry const& entry);

    [[nodiscard]] api::RegistrationHandle nextHandle();

    // Removes all entries owned by `owner`. Caller must hold mMutex.
    [[nodiscard]] std::size_t removeEntriesForOwnerLocked(const api::ModId& owner);

    mutable std::mutex                                         mMutex;
    std::unordered_map<api::RegistrationHandle, RegistryEntry> mEntries;
    ConflictIndex                                              mConflictIndex;
    std::unordered_map<api::ModId, ModRecord>                  mMods;
    std::atomic<std::uint64_t>                                 mNextHandle{1};
};

} // namespace dearoreui::registry
