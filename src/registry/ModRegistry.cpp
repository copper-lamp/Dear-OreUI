#include "registry/ModRegistry.h"

#include "api/types/Error.h"

#include <chrono>

namespace dearoreui::registry {

namespace {

struct FingerprintVisitor {
    [[nodiscard]] std::string const& operator()(ResourceEntry const& entry) const {
        return entry.manifest.fingerprint;
    }

    [[nodiscard]] std::string const& operator()(ScriptEntry const& entry) const {
        return entry.manifest.fingerprint;
    }

    [[nodiscard]] std::string const& operator()(StyleSheetEntry const& entry) const {
        return entry.manifest.fingerprint;
    }
};

struct NamespaceVisitor {
    [[nodiscard]] std::string const& operator()(ResourceEntry const& entry) const {
        return entry.manifest.modNamespace;
    }

    [[nodiscard]] std::string const& operator()(ScriptEntry const& entry) const {
        return entry.manifest.modNamespace;
    }

    [[nodiscard]] std::string const& operator()(StyleSheetEntry const& entry) const {
        return entry.manifest.modNamespace;
    }
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

    auto handle = nextHandle();
    entry.handle    = handle;
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

api::Result<api::RegistrationHandle> ModRegistry::insert(ResourceEntry entry) {
    return insertImpl(std::move(entry));
}

api::Result<api::RegistrationHandle> ModRegistry::insert(ScriptEntry entry) {
    return insertImpl(std::move(entry));
}

api::Result<api::RegistrationHandle> ModRegistry::insert(StyleSheetEntry entry) {
    return insertImpl(std::move(entry));
}

bool ModRegistry::remove(api::RegistrationHandle handle) {
    std::lock_guard lock{mMutex};

    auto iterator = mEntries.find(handle);
    if (iterator == mEntries.end()) {
        return false;
    }

    auto key = std::visit(
        [](auto const& entry) -> ConflictKey {
            return ConflictKey{entry.manifest.modNamespace, entry.manifest.path};
        },
        iterator->second
    );

    mConflictIndex.erase(key);
    mEntries.erase(iterator);
    return true;
}

std::size_t ModRegistry::removeAll(api::ModId owner) {
    std::lock_guard lock{mMutex};

    std::vector<api::RegistrationHandle> toRemove;
    for (auto const& [handle, entry] : mEntries) {
        auto const* entryOwner = std::visit(
            [](auto const& e) -> api::ModId const* { return &e.owner; },
            entry
        );
        if (entryOwner->value() == owner.value()) {
            toRemove.push_back(handle);
        }
    }

    for (auto handle : toRemove) {
        auto iterator = mEntries.find(handle);
        if (iterator == mEntries.end()) {
            continue;
        }
        auto key = std::visit(
            [](auto const& entry) -> ConflictKey {
                return ConflictKey{entry.manifest.modNamespace, entry.manifest.path};
            },
            iterator->second
        );
        mConflictIndex.erase(key);
        mEntries.erase(iterator);
    }

    return toRemove.size();
}

std::optional<RegistryEntry> ModRegistry::find(api::RegistrationHandle handle) const {
    std::lock_guard lock{mMutex};
    auto iterator = mEntries.find(handle);
    if (iterator == mEntries.end()) {
        return std::nullopt;
    }
    return iterator->second;
}

std::vector<api::RegistrationHandle> ModRegistry::findByOwner(api::ModId owner) const {
    std::lock_guard lock{mMutex};
    std::vector<api::RegistrationHandle> result;
    for (auto const& [handle, entry] : mEntries) {
        auto const* entryOwner = std::visit(
            [](auto const& e) -> api::ModId const* { return &e.owner; },
            entry
        );
        if (entryOwner->value() == owner.value()) {
            result.push_back(handle);
        }
    }
    return result;
}

std::vector<api::RegistrationHandle> ModRegistry::findByNamespace(std::string_view ns) const {
    std::lock_guard lock{mMutex};
    std::vector<api::RegistrationHandle> result;
    for (auto const& [handle, entry] : mEntries) {
        auto const& modNamespace = std::visit(NamespaceVisitor{}, entry);
        if (modNamespace == ns) {
            result.push_back(handle);
        }
    }
    return result;
}

bool ModRegistry::hasConflict(api::ResourceManifest const& manifest) const {
    std::lock_guard lock{mMutex};
    return mConflictIndex.find(ConflictKey{manifest.modNamespace, manifest.path}) != mConflictIndex.end();
}

std::size_t ModRegistry::size() const {
    std::lock_guard lock{mMutex};
    return mEntries.size();
}

void ModRegistry::clear() {
    std::lock_guard lock{mMutex};
    mEntries.clear();
    mConflictIndex.clear();
}

api::RegistrationHandle ModRegistry::nextHandle() {
    return api::RegistrationHandle{mNextHandle.fetch_add(1, std::memory_order_relaxed)};
}

} // namespace dearoreui::registry
