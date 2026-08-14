#include "transform/ChangePlanner.h"

#include "diagnostic/Stage6TransformTelemetry.h"
#include "transform/ConflictDetector.h"
#include "transform/DependencyResolver.h"

#include <algorithm>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace dearoreui::transform {

ChangePlanner::ChangePlanner(registry::IModRegistry& registry) : mRegistry(registry) {}

ChangeOperation ChangePlanner::toOperation(registry::RegistryEntry const& entry) {
    return std::visit(
        [](auto const& e) {
            ChangeOperation operation;
            operation.handle              = e.handle;
            operation.owner               = e.owner;
            operation.modNamespace        = e.manifest.modNamespace;
            operation.path                = e.manifest.path;
            operation.fingerprint         = e.manifest.fingerprint;
            operation.pageScopes          = e.manifest.pageScopes;
            operation.declaredConflicts   = e.manifest.conflicts;
            operation.dependencies        = e.manifest.dependencies;
            operation.versionConstrained  = e.manifest.versionConstraint.has_value();

            using EntryType = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<EntryType, registry::ScriptEntry>) {
                operation.kind    = ChangeOperationKind::AddScript;
                operation.content = e.source;
            } else if constexpr (std::is_same_v<EntryType, registry::StyleSheetEntry>) {
                operation.kind    = ChangeOperationKind::AddStyleSheet;
                operation.content = e.source;
            } else {
                operation.kind         = ChangeOperationKind::AddResource;
                operation.resourceKind = e.manifest.kind;
                operation.content      = e.payload;
            }
            return operation;
        },
        entry
    );
}

bool ChangePlanner::scopeMatches(api::PageScope target, std::vector<api::PageScope> const& scopes) {
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

DependencyProblem::Kind
ChangePlanner::problemKindFor(api::ModId owner, std::vector<DependencyProblem> const& problems) {
    for (auto const& problem : problems) {
        if (problem.dependant == owner) {
            return problem.kind;
        }
    }
    return DependencyProblem::Kind::Missing;
}

ChangePlan ChangePlanner::plan(api::ContextId contextId, api::PageScope scope) const {
    ChangePlan plan;
    plan.contextId = contextId;
    plan.pageScope = scope;

    // 1. Collect mod state.
    std::vector<registry::ModRecord> enabledRecords;
    std::unordered_set<std::string>  registeredIds;
    std::unordered_set<std::string>  disabledIds;
    std::unordered_set<std::string>  enabledNamespaces;
    for (auto const& record : mRegistry.allMods()) {
        registeredIds.insert(record.manifest.id.value());
        if (record.enabled) {
            enabledRecords.push_back(record);
            enabledNamespaces.insert(record.manifest.modNamespace);
        } else {
            disabledIds.insert(record.manifest.id.value());
        }
    }

    // 2. Resolve the mod-level dependency graph over enabled mods.
    auto resolution = DependencyResolver::resolve(enabledRecords);
    plan.modOrder           = resolution.order;
    plan.dependencyProblems = resolution.problems;

    std::unordered_set<std::string> orderedIds;
    for (auto const& id : resolution.order) {
        orderedIds.insert(id.value());
    }

    std::unordered_map<std::string, std::size_t> modOrderIndex;
    for (std::size_t index = 0; index < resolution.order.size(); ++index) {
        modOrderIndex.emplace(resolution.order[index].value(), index);
    }

    // 3. Convert entries into operations and classify each operation.
    for (auto const& entry : mRegistry.listEntries()) {
        auto operation = toOperation(entry);

        if (!scopeMatches(scope, operation.pageScopes)) {
            continue; // Out of page scope; excluded from this plan entirely.
        }

        if (registeredIds.count(operation.owner.value()) == 0) {
            operation.status = ChangeOperationStatus::BlockedPermissionDenied;
            operation.error  = api::Error{
                api::ErrorCode::InvalidState, "owner mod is not registered: " + operation.owner.value()
            };
        } else if (disabledIds.count(operation.owner.value()) != 0) {
            operation.status = ChangeOperationStatus::SkippedDisabled;
            operation.error  = api::Error{
                api::ErrorCode::InvalidState, "owner mod is disabled: " + operation.owner.value()
            };
        } else if (orderedIds.count(operation.owner.value()) == 0) {
            switch (problemKindFor(operation.owner, resolution.problems)) {
            case DependencyProblem::Kind::Cycle:
                operation.status = ChangeOperationStatus::BlockedConflict;
                operation.error  = api::Error{
                    api::ErrorCode::DependencyCycle, "owner mod is part of a dependency cycle"
                };
                break;
            case DependencyProblem::Kind::VersionMismatch:
            case DependencyProblem::Kind::InvalidRange:
                operation.status = ChangeOperationStatus::SkippedVersionMismatch;
                operation.error  = api::Error{
                    api::ErrorCode::VersionMismatch, "owner mod fails dependency version check"
                };
                break;
            default:
                operation.status = ChangeOperationStatus::SkippedDependencyMissing;
                operation.error  = api::Error{
                    api::ErrorCode::DependencyMissing, "owner mod has a missing dependency"
                };
                break;
            }
        } else if (operation.versionConstrained) {
            // Version-constrained changes target a specific original page version;
            // execution requires Stage 8 bundle patch support. Never execute here.
            operation.status = ChangeOperationStatus::BlockedNotSupported;
            operation.error  = api::Error{
                api::ErrorCode::NotSupported,
                "version-constrained changes require stage 8 bundle patch support"
            };
        } else {
            for (auto const& dep : operation.dependencies) {
                if (dep.optional) {
                    continue;
                }
                if (enabledNamespaces.count(dep.modNamespace) == 0) {
                    operation.status = ChangeOperationStatus::SkippedDependencyMissing;
                    operation.error  = api::Error{
                        api::ErrorCode::DependencyMissing,
                        "entry dependency namespace missing: " + dep.modNamespace
                    };
                    break;
                }
            }
        }

        plan.operations.push_back(std::move(operation));
    }

    // 4. Deterministic ordering: pending operations first by mod topo order, then
    //    by (namespace, path, handle); non-pending operations follow in the same key order.
    std::stable_sort(
        plan.operations.begin(),
        plan.operations.end(),
        [&modOrderIndex](ChangeOperation const& a, ChangeOperation const& b) {
            bool const aPending = a.status == ChangeOperationStatus::Pending;
            bool const bPending = b.status == ChangeOperationStatus::Pending;
            if (aPending != bPending) {
                return aPending;
            }
            if (aPending) {
                std::size_t const aIndex = modOrderIndex.at(a.owner.value());
                std::size_t const bIndex = modOrderIndex.at(b.owner.value());
                if (aIndex != bIndex) {
                    return aIndex < bIndex;
                }
            }
            if (a.modNamespace != b.modNamespace) {
                return a.modNamespace < b.modNamespace;
            }
            if (a.path != b.path) {
                return a.path < b.path;
            }
            return a.handle.value() < b.handle.value();
        }
    );

    for (std::size_t index = 0; index < plan.operations.size(); ++index) {
        plan.operations[index].orderIndex = index;
    }

    // 5. Conflict detection and reporting.
    auto detectedConflicts = ConflictDetector::detect(plan.operations);
    plan.conflicts         = std::move(detectedConflicts);

    std::unordered_set<std::string> reportedCycles;
    for (auto const& problem : resolution.problems) {
        if (problem.kind != DependencyProblem::Kind::Cycle) {
            continue;
        }
        if (!reportedCycles.insert(problem.dependant.value()).second) {
            continue;
        }
        Conflict conflict;
        conflict.kind        = ConflictKind::OrderCycle;
        conflict.description = problem.message;
        conflict.involvedMods.push_back(problem.dependant);
        for (auto const& id : problem.cyclePath) {
            conflict.involvedMods.push_back(id);
        }
        plan.conflicts.push_back(std::move(conflict));
    }

    // 6. Diagnostic record.
    diagnostic::recordStage6PlanBuilt(
        contextId,
        plan.operations.size(),
        plan.modOrder.size(),
        plan.dependencyProblems.size(),
        plan.conflicts.size()
    );

    return plan;
}

} // namespace dearoreui::transform
