#include "registry/ModRegistry.h"

#include "api/types/Error.h"

#include <algorithm>
#include <chrono>

namespace dearoreui::registry {

namespace {

struct FingerprintVisitor {
    [[nodiscard]] std::string const& operator()(ResourceEntry const& entry) const { return entry.manifest.fingerprint; }

    [[nodiscard]] std::string const& operator()(ScriptEntry const& entry) const { return entry.manifest.fingerprint; }

    [[nodiscard]] std::string const& operator()(StyleSheetEntry const& entry) const {
        return entry.manifest.fingerprint;
    }

    [[nodiscard]] std::string const& operator()(UiEntry const& entry) const { return entry.manifest.fingerprint; }
};

struct NamespaceVisitor {
    [[nodiscard]] std::string const& operator()(ResourceEntry const& entry) const {
        return entry.manifest.modNamespace;
    }

    [[nodiscard]] std::string const& operator()(ScriptEntry const& entry) const { return entry.manifest.modNamespace; }

    [[nodiscard]] std::string const& operator()(StyleSheetEntry const& entry) const {
        return entry.manifest.modNamespace;
    }

    [[nodiscard]] std::string const& operator()(UiEntry const& entry) const { return entry.manifest.modNamespace; }
};

} // namespace

template <typename Entry>
api::Result<api::RegistrationHandle> ModRegistry::insertImpl(Entry entry) {
    auto key = conflictKeyFor(entry);
    auto now = std::chrono::system_clock::now();

    std::lock_guard lock{mMutex};

    auto iterator = mConflictIndex.find(key);
    if (iterator != mConflictIndex.end()) {
        auto existing = mEntries.find(iterator->second);
        if (existing != mEntries.end()) {
            auto const& existingFingerprint = std::visit(FingerprintVisitor{}, existing->second);
            if (existingFingerprint == entry.manifest.fingerprint) {
                return iterator->second;
            }
        }
        return api::Error{api::ErrorCode::ResourceConflict, "Resource path already registered by another entry"};
    }

    auto handle        = nextHandle();
    entry.handle       = handle;
    entry.registeredAt = now;

    mEntries.emplace(handle, std::move(entry));
    mConflictIndex.emplace(std::move(key), handle);
    return handle;
}

template <>
ModRegistry::ConflictKey ModRegistry::conflictKeyFor(ResourceEntry const& entry) {
    return ConflictKey{entry.manifest.modNamespace, entry.manifest.path};
}

template <>
ModRegistry::ConflictKey ModRegistry::conflictKeyFor(ScriptEntry const& entry) {
    return ConflictKey{entry.manifest.modNamespace, entry.manifest.path};
}

template <>
ModRegistry::ConflictKey ModRegistry::conflictKeyFor(StyleSheetEntry const& entry) {
    return ConflictKey{entry.manifest.modNamespace, entry.manifest.path};
}

ModRegistry::UiConflictKey ModRegistry::uiConflictKeyFor(UiEntry const& entry) {
    return UiConflictKey{entry.manifest.modNamespace, entry.manifest.id};
}

api::Result<api::RegistrationHandle> ModRegistry::insert(ResourceEntry entry) { return insertImpl(std::move(entry)); }

api::Result<api::RegistrationHandle> ModRegistry::insert(ScriptEntry entry) { return insertImpl(std::move(entry)); }

api::Result<api::RegistrationHandle> ModRegistry::insert(StyleSheetEntry entry) { return insertImpl(std::move(entry)); }

api::Result<api::RegistrationHandle> ModRegistry::insert(UiEntry entry) {
    auto key = uiConflictKeyFor(entry);
    auto now = std::chrono::system_clock::now();

    std::lock_guard lock{mMutex};

    auto iterator = mUiConflictIndex.find(key);
    if (iterator != mUiConflictIndex.end()) {
        auto existing = mEntries.find(iterator->second);
        if (existing != mEntries.end()) {
            auto const& existingFingerprint = std::visit(FingerprintVisitor{}, existing->second);
            if (existingFingerprint == entry.manifest.fingerprint) {
                return iterator->second;
            }
        }
        return api::Error{api::ErrorCode::ResourceConflict, "UI id already registered by another entry"};
    }

    auto handle        = nextHandle();
    entry.handle       = handle;
    entry.registeredAt = now;

    mEntries.emplace(handle, std::move(entry));
    mUiConflictIndex.emplace(std::move(key), handle);
    return handle;
}

api::Result<api::ModId> ModRegistry::registerMod(ModRecord record) {
    std::lock_guard lock{mMutex};

    auto const& manifest = record.manifest;
    if (mMods.find(manifest.id) != mMods.end()) {
        return api::Error{api::ErrorCode::AlreadyExists, "mod already registered: " + manifest.id.value()};
    }

    for (auto const& [id, existing] : mMods) {
        static_cast<void>(id);
        if (existing.manifest.modNamespace == manifest.modNamespace) {
            return api::Error{
                api::ErrorCode::NamespaceConflict,
                "mod namespace already registered by another mod: " + manifest.modNamespace
            };
        }
    }

    api::ModId id       = manifest.id; // Copy before `record` is moved into the registry.
    record.registeredAt = std::chrono::system_clock::now();
    mMods.emplace(id, std::move(record));
    return id;
}

bool ModRegistry::unregisterMod(api::ModId id) {
    std::lock_guard lock{mMutex};

    auto modIterator = mMods.find(id);
    if (modIterator == mMods.end()) {
        return false;
    }
    mMods.erase(modIterator);

    static_cast<void>(removeEntriesForOwnerLocked(id));
    return true;
}

std::optional<ModRecord> ModRegistry::findMod(api::ModId id) const {
    std::lock_guard lock{mMutex};
    auto            iterator = mMods.find(id);
    if (iterator == mMods.end()) {
        return std::nullopt;
    }
    return iterator->second;
}

bool ModRegistry::isModRegistered(api::ModId id) const {
    std::lock_guard lock{mMutex};
    return mMods.find(id) != mMods.end();
}

bool ModRegistry::setModEnabled(api::ModId id, bool enabled) {
    std::lock_guard lock{mMutex};
    auto            iterator = mMods.find(id);
    if (iterator == mMods.end()) {
        return false;
    }
    iterator->second.enabled = enabled;
    return true;
}

bool ModRegistry::isModEnabled(api::ModId id) const {
    std::lock_guard lock{mMutex};
    auto            iterator = mMods.find(id);
    return iterator != mMods.end() && iterator->second.enabled;
}

std::vector<ModRecord> ModRegistry::allMods() const {
    std::lock_guard        lock{mMutex};
    std::vector<ModRecord> result;
    result.reserve(mMods.size());
    for (auto const& [id, record] : mMods) {
        static_cast<void>(id);
        result.push_back(record);
    }
    return result;
}

std::size_t ModRegistry::modCount() const {
    std::lock_guard lock{mMutex};
    return mMods.size();
}

bool ModRegistry::remove(api::RegistrationHandle handle) {
    std::lock_guard lock{mMutex};

    auto iterator = mEntries.find(handle);
    if (iterator == mEntries.end()) {
        return false;
    }

    std::visit(
        [this](auto const& entry) {
            using EntryType = std::decay_t<decltype(entry)>;
            if constexpr (std::is_same_v<EntryType, UiEntry>) {
                mUiConflictIndex.erase(uiConflictKeyFor(entry));
            } else {
                mConflictIndex.erase(conflictKeyFor(entry));
            }
        },
        iterator->second
    );

    mEntries.erase(iterator);
    return true;
}

std::size_t ModRegistry::removeAll(api::ModId owner) {
    std::lock_guard lock{mMutex};
    return removeEntriesForOwnerLocked(owner);
}

std::size_t ModRegistry::removeEntriesForOwnerLocked(const api::ModId& owner) {
    std::vector<api::RegistrationHandle> toRemove;
    for (auto const& [handle, entry] : mEntries) {
        auto const* entryOwner = std::visit([](auto const& e) -> api::ModId const* { return &e.owner; }, entry);
        if (entryOwner->value() == owner.value()) {
            toRemove.push_back(handle);
        }
    }

    for (auto handle : toRemove) {
        auto iterator = mEntries.find(handle);
        if (iterator == mEntries.end()) {
            continue;
        }
        std::visit(
            [this](auto const& entry) {
                using EntryType = std::decay_t<decltype(entry)>;
                if constexpr (std::is_same_v<EntryType, UiEntry>) {
                    mUiConflictIndex.erase(uiConflictKeyFor(entry));
                } else {
                    mConflictIndex.erase(conflictKeyFor(entry));
                }
            },
            iterator->second
        );
        mEntries.erase(iterator);
    }

    return toRemove.size();
}

std::optional<RegistryEntry> ModRegistry::find(api::RegistrationHandle handle) const {
    std::lock_guard lock{mMutex};
    auto            iterator = mEntries.find(handle);
    if (iterator == mEntries.end()) {
        return std::nullopt;
    }
    return iterator->second;
}

std::vector<api::RegistrationHandle> ModRegistry::findByOwner(api::ModId owner) const {
    std::lock_guard                      lock{mMutex};
    std::vector<api::RegistrationHandle> result;
    for (auto const& [handle, entry] : mEntries) {
        auto const* entryOwner = std::visit([](auto const& e) -> api::ModId const* { return &e.owner; }, entry);
        if (entryOwner->value() == owner.value()) {
            result.push_back(handle);
        }
    }
    return result;
}

std::vector<api::RegistrationHandle> ModRegistry::findByNamespace(std::string_view ns) const {
    std::lock_guard                      lock{mMutex};
    std::vector<api::RegistrationHandle> result;
    for (auto const& [handle, entry] : mEntries) {
        auto const& modNamespace = std::visit(NamespaceVisitor{}, entry);
        if (modNamespace == ns) {
            result.push_back(handle);
        }
    }
    return result;
}

std::vector<RegistryEntry> ModRegistry::listEntries() const {
    std::lock_guard            lock{mMutex};
    std::vector<RegistryEntry> result;
    result.reserve(mEntries.size());
    for (auto const& [handle, entry] : mEntries) {
        static_cast<void>(handle);
        result.push_back(entry);
    }
    return result;
}

std::vector<UiEntry> ModRegistry::listUiEntries() const {
    std::lock_guard      lock{mMutex};
    std::vector<UiEntry> result;
    for (auto const& [handle, entry] : mEntries) {
        static_cast<void>(handle);
        if (std::holds_alternative<UiEntry>(entry)) {
            result.push_back(std::get<UiEntry>(entry));
        }
    }
    return result;
}

bool ModRegistry::hasConflict(api::ResourceManifest const& manifest) const {
    std::lock_guard lock{mMutex};
    return mConflictIndex.find(ConflictKey{manifest.modNamespace, manifest.path}) != mConflictIndex.end();
}

bool ModRegistry::hasUiConflict(api::UiManifest const& manifest) const {
    std::lock_guard lock{mMutex};
    return mUiConflictIndex.find(UiConflictKey{manifest.modNamespace, manifest.id}) != mUiConflictIndex.end();
}

std::size_t ModRegistry::size() const {
    std::lock_guard lock{mMutex};
    return mEntries.size();
}

void ModRegistry::clear() {
    std::lock_guard lock{mMutex};
    mEntries.clear();
    mConflictIndex.clear();
    mUiConflictIndex.clear();
    mMods.clear();
}

api::RegistrationHandle ModRegistry::nextHandle() {
    return api::RegistrationHandle{mNextHandle.fetch_add(1, std::memory_order_relaxed)};
}

} // namespace dearoreui::registry
