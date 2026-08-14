#include "ui/UiPlanner.h"

#include "diagnostic/Stage7UiTelemetry.h"

#include <algorithm>
#include <sstream>

namespace dearoreui::ui {

namespace {

[[nodiscard]] std::string conflictReason(
    api::UiManifest const& manifest,
    std::vector<std::string> const& blockedBy
) {
    std::ostringstream stream;
    stream << "UI " << manifest.modNamespace << "/" << manifest.id << " blocked by conflict with:";
    for (auto const& other : blockedBy) {
        stream << " " << other;
    }
    return stream.str();
}

} // namespace

UiPlanner::UiPlanner(registry::IModRegistry& registry) : mRegistry(registry) {}

bool UiPlanner::scopeMatches(api::PageScope target, std::vector<api::PageScope> const& scopes) const {
    if (scopes.empty()) {
        return true;
    }
    for (auto scope : scopes) {
        if (scope == api::PageScope::Any || target == api::PageScope::Any || scope == target) {
            return true;
        }
    }
    return false;
}

std::vector<api::ModId> UiPlanner::buildModOrder(registry::IModRegistry& registry) {
    std::vector<registry::ModRecord> enabled;
    for (auto const& record : registry.allMods()) {
        if (record.enabled) {
            enabled.push_back(record);
        }
    }

    // Simple deterministic ordering: by namespace then mod id.
    // Stage 6 already performs full dependency resolution for resource entries;
    // UI ordering follows the same stable principle without re-implementing the resolver.
    std::sort(
        enabled.begin(),
        enabled.end(),
        [](registry::ModRecord const& a, registry::ModRecord const& b) {
            if (a.manifest.modNamespace != b.manifest.modNamespace) {
                return a.manifest.modNamespace < b.manifest.modNamespace;
            }
            return a.manifest.id.value() < b.manifest.id.value();
        }
    );

    std::vector<api::ModId> order;
    order.reserve(enabled.size());
    for (auto const& record : enabled) {
        order.push_back(record.manifest.id);
    }
    return order;
}

UiMountPlan UiPlanner::plan(api::ContextId contextId, api::PageScope scope) const {
    UiMountPlan plan;
    plan.contextId = contextId;
    plan.pageScope = scope;

    auto modOrder = buildModOrder(mRegistry);
    std::unordered_map<std::string, std::size_t> modOrderIndex;
    for (std::size_t index = 0; index < modOrder.size(); ++index) {
        modOrderIndex.emplace(modOrder[index].value(), index);
    }

    std::unordered_set<std::string> usedContainerIds;
    std::unordered_set<std::string> mountedIds;

    auto uiEntries = mRegistry.listUiEntries();

    // Pre-collect explicit conflicts among all UI entries to handle cross-blocking.
    std::unordered_map<std::string, std::vector<std::string>> explicitConflicts;
    for (auto const& entry : uiEntries) {
        auto key = entry.manifest.modNamespace + "/" + entry.manifest.id;
        for (auto const& conflict : entry.manifest.conflicts) {
            explicitConflicts[key].push_back(conflict);
        }
    }

    for (auto const& entry : uiEntries) {
        if (!mRegistry.isModEnabled(entry.owner)) {
            UiMountItem item;
            item.handle   = entry.handle;
            item.manifest = entry.manifest;
            item.decision = UiMountDecision::Skip;
            item.reason   = "owner mod disabled";
            plan.items.push_back(std::move(item));
            ++plan.skipped;
            continue;
        }

        if (!scopeMatches(scope, entry.manifest.pageScopes)) {
            UiMountItem item;
            item.handle   = entry.handle;
            item.manifest = entry.manifest;
            item.decision = UiMountDecision::Skip;
            item.reason   = "page scope mismatch";
            plan.items.push_back(std::move(item));
            ++plan.skipped;
            continue;
        }

        auto containerId = api::makeUiContainerId(entry.manifest.modNamespace, entry.manifest.kind, entry.manifest.id);
        if (entry.manifest.containerId.empty()) {
            containerId = api::makeUiContainerId(entry.manifest.modNamespace, entry.manifest.kind, entry.manifest.id);
        } else {
            containerId = entry.manifest.containerId;
        }

        std::vector<std::string> blockedBy;

        // Container ID conflict: deterministic id already taken by another UI in this plan.
        if (usedContainerIds.count(containerId) != 0) {
            blockedBy.push_back("container_id:" + containerId);
        }

        // Explicit conflict declarations.
        auto key = entry.manifest.modNamespace + "/" + entry.manifest.id;
        for (auto const& other : mountedIds) {
            if (std::find(explicitConflicts[key].begin(), explicitConflicts[key].end(), other)
                != explicitConflicts[key].end()) {
                blockedBy.push_back(other);
            }
            auto otherKey = other;
            if (std::find(explicitConflicts[otherKey].begin(), explicitConflicts[otherKey].end(), key)
                != explicitConflicts[otherKey].end()) {
                blockedBy.push_back(other);
            }
        }

        if (!blockedBy.empty()) {
            UiMountItem item;
            item.handle   = entry.handle;
            item.manifest = entry.manifest;
            item.decision = UiMountDecision::Blocked;
            item.reason   = conflictReason(entry.manifest, blockedBy);
            plan.items.push_back(std::move(item));
            ++plan.blocked;
            continue;
        }

        OverlaySpec spec;
        spec.handle       = entry.handle;
        spec.modNamespace = entry.manifest.modNamespace;
        spec.uiId         = entry.manifest.id;
        spec.kind         = entry.manifest.kind;
        spec.containerId  = containerId;
        spec.anchor       = entry.manifest.anchor;
        spec.pointerEvents = entry.manifest.pointerEvents;
        spec.htmlBody     = entry.htmlBody;
        spec.scripts      = entry.manifest.scripts;
        spec.styles       = entry.manifest.styles;

        UiMountItem item;
        item.handle   = entry.handle;
        item.manifest = entry.manifest;
        item.decision = UiMountDecision::Mount;
        item.spec     = std::move(spec);
        plan.items.push_back(std::move(item));

        usedContainerIds.insert(containerId);
        mountedIds.insert(key);
        ++plan.mounted;
    }

    // Deterministic ordering: mod order, then namespace, then kind, then id, then handle.
    std::sort(
        plan.items.begin(),
        plan.items.end(),
        [&modOrderIndex](UiMountItem const& a, UiMountItem const& b) {
            auto aOwner = modOrderIndex.find(a.manifest.modNamespace);
            auto bOwner = modOrderIndex.find(b.manifest.modNamespace);
            std::size_t aIndex = (aOwner == modOrderIndex.end()) ? std::numeric_limits<std::size_t>::max() : aOwner->second;
            std::size_t bIndex = (bOwner == modOrderIndex.end()) ? std::numeric_limits<std::size_t>::max() : bOwner->second;
            if (aIndex != bIndex) {
                return aIndex < bIndex;
            }
            if (a.manifest.modNamespace != b.manifest.modNamespace) {
                return a.manifest.modNamespace < b.manifest.modNamespace;
            }
            if (a.manifest.kind != b.manifest.kind) {
                return static_cast<int>(a.manifest.kind) < static_cast<int>(b.manifest.kind);
            }
            if (a.manifest.id != b.manifest.id) {
                return a.manifest.id < b.manifest.id;
            }
            return a.handle.value() < b.handle.value();
        }
    );

    plan.success = plan.errors.empty();

    diagnostic::recordStage7UiPlanned(
        contextId,
        scope,
        plan.mounted,
        plan.skipped,
        plan.blocked
    );

    return plan;
}

} // namespace dearoreui::ui
