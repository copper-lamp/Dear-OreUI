#include "transform/ConflictDetector.h"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace dearoreui::transform {

namespace {

struct LocationKey {
    std::string modNamespace;
    std::string path;

    [[nodiscard]] bool operator==(LocationKey const& other) const {
        return modNamespace == other.modNamespace && path == other.path;
    }
};

struct LocationKeyHash {
    [[nodiscard]] std::size_t operator()(LocationKey const& key) const {
        auto h1 = std::hash<std::string>{}(key.modNamespace);
        auto h2 = std::hash<std::string>{}(key.path);
        return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
    }
};

[[nodiscard]] bool isPending(ChangeOperation const& operation) {
    return operation.status == ChangeOperationStatus::Pending;
}

[[nodiscard]] void blockOperation(ChangeOperation& operation, api::ErrorCode code, std::string message) {
    operation.status = ChangeOperationStatus::BlockedConflict;
    operation.error  = api::Error{code, std::move(message)};
}

} // namespace

std::vector<Conflict> ConflictDetector::detect(std::vector<ChangeOperation>& operations) {
    std::vector<Conflict> conflicts;

    // Resource ownership: two pending operations claiming the same (namespace, path).
    {
        std::unordered_map<LocationKey, std::size_t, LocationKeyHash> seen;
        for (std::size_t index = 0; index < operations.size(); ++index) {
            auto& operation = operations[index];
            if (!isPending(operation)) {
                continue;
            }
            LocationKey key{operation.modNamespace, operation.path};
            auto        inserted = seen.emplace(key, index);
            if (inserted.second) {
                continue;
            }

            auto& existing = operations[inserted.first->second];
            if (isPending(existing)) {
                Conflict conflict;
                conflict.kind         = ConflictKind::ResourceOwnership;
                conflict.description  = "resource ownership conflict at " + key.modNamespace + "/" + key.path;
                conflict.involvedMods = {existing.owner, operation.owner};
                conflict.involvedPaths.push_back(key.path);

                blockOperation(existing, api::ErrorCode::ResourceConflict, conflict.description);
                conflicts.push_back(conflict);
            }

            blockOperation(
                operation,
                api::ErrorCode::ResourceConflict,
                "resource ownership conflict at " + key.modNamespace + "/" + key.path
            );
        }
    }

    // Declared conflicts: pending operations whose `conflicts` list intersects
    // another pending operation's location.
    {
        std::unordered_map<std::string, std::vector<std::size_t>> byPath;
        std::unordered_map<std::string, std::vector<std::size_t>> byFullPath;
        for (std::size_t index = 0; index < operations.size(); ++index) {
            auto const& operation = operations[index];
            if (!isPending(operation)) {
                continue;
            }
            byPath[operation.path].push_back(index);
            byFullPath[operation.modNamespace + "/" + operation.path].push_back(index);
        }

        for (auto& operation : operations) {
            if (!isPending(operation)) {
                continue;
            }
            for (auto const& declared : operation.declaredConflicts) {
                std::vector<std::size_t> candidates;
                auto                     byPathIterator = byPath.find(declared);
                if (byPathIterator != byPath.end()) {
                    candidates = byPathIterator->second;
                }
                auto byFullPathIterator = byFullPath.find(declared);
                if (byFullPathIterator != byFullPath.end()) {
                    candidates.insert(candidates.end(), byFullPathIterator->second.begin(), byFullPathIterator->second.end());
                }
                for (auto index : candidates) {
                    auto& target = operations[index];
                    if (&target == &operation || !isPending(target)) {
                        continue;
                    }
                    Conflict conflict;
                    conflict.kind         = ConflictKind::DeclaredConflict;
                    conflict.description  = "declared conflict: " + operation.modNamespace + "/" + operation.path
                                          + " vs " + target.modNamespace + "/" + target.path;
                    conflict.involvedMods = {operation.owner, target.owner};
                    conflict.involvedPaths.push_back(target.path);

                    if (isPending(target)) {
                        blockOperation(target, api::ErrorCode::ResourceConflict, conflict.description);
                    }
                    blockOperation(operation, api::ErrorCode::ResourceConflict, conflict.description);
                    conflicts.push_back(conflict);
                }
            }
        }
    }

    return conflicts;
}

} // namespace dearoreui::transform
