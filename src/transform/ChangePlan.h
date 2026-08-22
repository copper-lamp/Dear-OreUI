#pragma once

#include "api/manifest/Dependency.h"
#include "api/manifest/ResourceManifest.h"
#include "api/manifest/UiManifest.h"
#include "api/types/Error.h"
#include "api/types/Id.h"
#include "api/types/Page.h"
#include "transform/Conflict.h"
#include "transform/DependencyProblem.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace dearoreui::transform {

enum class ChangeOperationKind { AddScript, AddStyleSheet, AddResource, AddUi };

enum class ChangeOperationStatus {
    Pending,
    Applied,
    SkippedDependencyMissing,
    SkippedVersionMismatch,
    SkippedDisabled,
    BlockedConflict,
    BlockedPermissionDenied,
    BlockedNotSupported,
};

struct ChangeOperation {
    api::RegistrationHandle        handle;
    api::ModId                     owner;
    std::string                    modNamespace;
    std::string                    path;
    std::string                    fingerprint;
    api::ResourceKind              resourceKind{api::ResourceKind::Binary};
    ChangeOperationKind            kind{ChangeOperationKind::AddScript};
    std::size_t                    orderIndex{0};
    ChangeOperationStatus          status{ChangeOperationStatus::Pending};
    api::Error                     error;
    std::string                    content;
    std::vector<api::PageScope>    pageScopes;
    std::vector<std::string>       declaredConflicts;
    std::vector<api::Dependency>   dependencies;
    bool                           versionConstrained{false};
    std::optional<api::UiManifest> uiManifest;
};

struct ChangePlan {
    api::ContextId                 contextId;
    api::PageScope                 pageScope{api::PageScope::Any};
    std::vector<ChangeOperation>   operations;
    std::vector<api::ModId>        modOrder;
    std::vector<DependencyProblem> dependencyProblems;
    std::vector<Conflict>          conflicts;
};

struct ChangeReport {
    api::ContextId               contextId;
    bool                         success{true};
    std::size_t                  applied{0};
    std::size_t                  skipped{0};
    std::size_t                  blocked{0};
    std::vector<ChangeOperation> operations;
    std::vector<api::Error>      errors;
};

} // namespace dearoreui::transform
