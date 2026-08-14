#include "transform/DependencyResolver.h"

#include "api/types/Version.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace dearoreui::transform {

using DependencyProblemKind = DependencyProblem::Kind;

bool DependencyResolver::parseTerm(std::string_view term, RangeOp& op, api::Version& version) {
    if (term.starts_with(">=")) {
        op = RangeOp::GreaterEqual;
        term.remove_prefix(2);
    } else if (term.starts_with("<=")) {
        op = RangeOp::LessEqual;
        term.remove_prefix(2);
    } else if (term.starts_with("==")) {
        op = RangeOp::Exact;
        term.remove_prefix(2);
    } else if (term.starts_with(">")) {
        op = RangeOp::Greater;
        term.remove_prefix(1);
    } else if (term.starts_with("<")) {
        op = RangeOp::Less;
        term.remove_prefix(1);
    } else if (term.starts_with("~")) {
        op = RangeOp::Compatible;
        term.remove_prefix(1);
    } else if (term.starts_with("^")) {
        op = RangeOp::Compatible;
        term.remove_prefix(1);
    } else if (term.starts_with("=")) {
        op = RangeOp::Exact;
        term.remove_prefix(1);
    } else {
        op = RangeOp::Exact;
    }

    while (!term.empty() && std::isspace(static_cast<unsigned char>(term.front()))) {
        term.remove_prefix(1);
    }
    if (term.empty()) {
        return false;
    }

    auto parsed = api::Version::parse(term);
    if (parsed.isErr()) {
        return false;
    }
    version = parsed.value();
    return true;
}

bool DependencyResolver::rangeSyntaxValid(std::string_view range) {
    if (range.empty()) {
        return true;
    }

    std::istringstream stream{std::string{range}};
    std::string        token;
    while (stream >> token) {
        if (!token.empty() && token.back() == ',') {
            token.pop_back();
        }
        if (token.empty()) {
            continue;
        }
        RangeOp       op;
        api::Version  version;
        if (!parseTerm(token, op, version)) {
            return false;
        }
    }
    return true;
}

bool DependencyResolver::satisfiesRange(std::string_view range, api::Version const& version) {
    if (range.empty()) {
        return true;
    }
    if (version == api::Version{}) {
        // Target mod does not declare a version; the constraint cannot be verified.
        return false;
    }

    std::istringstream stream{std::string{range}};
    std::string        token;
    while (stream >> token) {
        if (!token.empty() && token.back() == ',') {
            token.pop_back();
        }
        if (token.empty()) {
            continue;
        }
        RangeOp      op;
        api::Version required;
        if (!parseTerm(token, op, required)) {
            return false;
        }
        switch (op) {
        case RangeOp::Exact:
            if (!(version == required)) return false;
            break;
        case RangeOp::GreaterEqual:
            if (!(version >= required)) return false;
            break;
        case RangeOp::Greater:
            if (!(version > required)) return false;
            break;
        case RangeOp::LessEqual:
            if (!(version <= required)) return false;
            break;
        case RangeOp::Less:
            if (!(version < required)) return false;
            break;
        case RangeOp::Compatible:
            if (!version.satisfies(required)) return false;
            break;
        }
    }
    return true;
}

namespace {

[[nodiscard]] DependencyProblem makeProblem(
    DependencyProblemKind kind,
    api::ModId            dependant,
    std::string           dependencyNamespace,
    std::string           versionRange,
    std::string           message
) {
    DependencyProblem problem;
    problem.kind               = kind;
    problem.dependant          = std::move(dependant);
    problem.dependencyNamespace = std::move(dependencyNamespace);
    problem.versionRange        = std::move(versionRange);
    problem.message             = std::move(message);
    return problem;
}

[[nodiscard]] std::vector<api::ModId> findCyclePath(
    api::ModId                                            start,
    std::unordered_map<api::ModId, std::vector<api::ModId>> const& dependsOn,
    std::unordered_set<api::ModId> const&                         confined
) {
    std::vector<api::ModId>       path;
    std::unordered_set<api::ModId> seen;
    api::ModId                     current = std::move(start);
    for (std::size_t step = 0; step < confined.size() + 1; ++step) {
        if (!seen.insert(current).second) {
            auto iterator = std::find(path.begin(), path.end(), current);
            if (iterator != path.end()) {
                path.erase(path.begin(), iterator);
            }
            path.push_back(current);
            break;
        }
        path.push_back(current);
        auto found = dependsOn.find(current);
        if (found == dependsOn.end() || found->second.empty()) {
            break;
        }
        current = found->second.front();
    }
    return path;
}

} // namespace

DependencyResolution DependencyResolver::resolve(std::vector<registry::ModRecord> const& records) {
    DependencyResolution resolution;

    std::unordered_map<api::ModId, registry::ModRecord const*>             byId;
    std::unordered_map<std::string, api::ModId>                            idByNamespace;
    for (auto const& record : records) {
        byId.emplace(record.manifest.id, &record);
        idByNamespace.emplace(record.manifest.modNamespace, record.manifest.id);
    }

    // Edge direction: dependency mod -> dependant mod.
    std::unordered_map<api::ModId, std::vector<api::ModId>> dependantOf;
    // Reverse edge for cycle-path walking: dependant mod -> dependency mods.
    std::unordered_map<api::ModId, std::vector<api::ModId>> dependsOn;
    std::unordered_set<api::ModId>                          excludedByProblem;

    for (auto const& record : records) {
        auto const& id = record.manifest.id;
        for (auto const& dep : record.manifest.dependencies) {
            auto found = idByNamespace.find(dep.modNamespace);
            if (found == idByNamespace.end()) {
                if (!dep.optional) {
                    resolution.problems.push_back(makeProblem(
                        DependencyProblemKind::Missing,
                        id,
                        dep.modNamespace,
                        dep.versionRange,
                        "required dependency namespace is not registered: " + dep.modNamespace
                    ));
                    excludedByProblem.insert(id);
                }
                continue;
            }

            auto depId = found->second;
            if (depId == id) {
                resolution.problems.push_back(makeProblem(
                    DependencyProblemKind::Cycle,
                    id,
                    dep.modNamespace,
                    dep.versionRange,
                    "mod depends on itself: " + id.value()
                ));
                excludedByProblem.insert(id);
                continue;
            }

            if (!rangeSyntaxValid(dep.versionRange)) {
                resolution.problems.push_back(makeProblem(
                    DependencyProblemKind::InvalidRange,
                    id,
                    dep.modNamespace,
                    dep.versionRange,
                    "dependency version range is invalid: " + dep.versionRange
                ));
                excludedByProblem.insert(id);
                continue;
            }

            auto const* depRecord = byId.at(depId);
            if (!satisfiesRange(dep.versionRange, depRecord->manifest.modVersion)) {
                resolution.problems.push_back(makeProblem(
                    DependencyProblemKind::VersionMismatch,
                    id,
                    dep.modNamespace,
                    dep.versionRange,
                    "dependency version range not satisfied by " + depId.value() + ": " + dep.versionRange
                ));
                excludedByProblem.insert(id);
                continue;
            }

            dependantOf[depId].push_back(id);
            dependsOn[id].push_back(depId);
        }
    }

    // Transitively exclude dependants of excluded mods.
    std::vector<api::ModId>      queue(excludedByProblem.begin(), excludedByProblem.end());
    std::unordered_set<api::ModId> excluded = excludedByProblem;
    while (!queue.empty()) {
        auto id = queue.back();
        queue.pop_back();
        auto found = dependantOf.find(id);
        if (found == dependantOf.end()) {
            continue;
        }
        for (const auto& dependant : found->second) {
            if (excluded.insert(dependant).second) {
                queue.push_back(dependant);
            }
        }
    }

    // Kahn topological sort over the remaining (non-excluded) mods.
    std::unordered_map<api::ModId, std::size_t> inDegree;
    for (auto const& record : records) {
        if (excluded.count(record.manifest.id) == 0) {
            inDegree.emplace(record.manifest.id, 0);
        }
    }
    for (auto const& [depId, dependants] : dependantOf) {
        if (excluded.count(depId) != 0) {
            continue;
        }
        for (const auto& dependant : dependants) {
            if (excluded.count(dependant) == 0) {
                ++inDegree[dependant];
            }
        }
    }

    std::set<api::ModId> ready;
    for (auto const& [id, degree] : inDegree) {
        if (degree == 0) {
            ready.insert(id);
        }
    }

    while (!ready.empty()) {
        auto id = *ready.begin();
        ready.erase(ready.begin());
        resolution.order.push_back(id);

        auto found = dependantOf.find(id);
        if (found == dependantOf.end()) {
            continue;
        }
        for (const auto& dependant : found->second) {
            if (excluded.count(dependant) != 0) {
                continue;
            }
            auto degreeIterator = inDegree.find(dependant);
            if (degreeIterator == inDegree.end()) {
                continue;
            }
            if (--degreeIterator->second == 0) {
                ready.insert(dependant);
            }
        }
    }

    // Remaining nodes form dependency cycles; report and exclude them.
    for (auto const& [id, degree] : inDegree) {
        if (degree > 0) {
            auto problem = makeProblem(
                DependencyProblemKind::Cycle,
                id,
                "",
                "",
                "dependency cycle detected involving mod: " + id.value()
            );
            problem.cyclePath = findCyclePath(id, dependsOn, excluded);
            resolution.problems.push_back(std::move(problem));
            excluded.insert(id);
        }
    }

    return resolution;
}

} // namespace dearoreui::transform
