#include "transform/PageTransformer.h"

#include "diagnostic/Stage6TransformTelemetry.h"

#include <chrono>
#include <utility>

namespace dearoreui::transform {

namespace {

[[nodiscard]] registry::ScriptEntry
toScriptEntry(ChangeOperation const& op, api::PageScope scope, std::chrono::system_clock::time_point at) {
    registry::ScriptEntry entry;
    entry.handle                 = op.handle;
    entry.owner                  = op.owner;
    entry.manifest.modNamespace  = op.modNamespace;
    entry.manifest.path          = op.path;
    entry.manifest.fingerprint   = op.fingerprint;
    entry.manifest.pageScopes    = {scope};
    entry.manifest.source        = op.content;
    entry.source                 = op.content;
    entry.registeredAt           = at;
    return entry;
}

[[nodiscard]] registry::StyleSheetEntry
toStyleSheetEntry(ChangeOperation const& op, api::PageScope scope, std::chrono::system_clock::time_point at) {
    registry::StyleSheetEntry entry;
    entry.handle                 = op.handle;
    entry.owner                  = op.owner;
    entry.manifest.modNamespace  = op.modNamespace;
    entry.manifest.path          = op.path;
    entry.manifest.fingerprint   = op.fingerprint;
    entry.manifest.pageScopes    = {scope};
    entry.manifest.source        = op.content;
    entry.source                 = op.content;
    entry.registeredAt           = at;
    return entry;
}

[[nodiscard]] registry::ResourceEntry
toResourceEntry(ChangeOperation const& op, api::PageScope scope, std::chrono::system_clock::time_point at) {
    registry::ResourceEntry entry;
    entry.handle                = op.handle;
    entry.owner                 = op.owner;
    entry.manifest.modNamespace = op.modNamespace;
    entry.manifest.path         = op.path;
    entry.manifest.kind         = op.resourceKind;
    entry.manifest.fingerprint  = op.fingerprint;
    entry.manifest.pageScopes   = {scope};
    entry.payload               = op.content;
    entry.registeredAt          = at;
    return entry;
}

[[nodiscard]] registry::UiEntry
toUiEntry(ChangeOperation const& op, api::PageScope scope, std::chrono::system_clock::time_point at) {
    registry::UiEntry entry;
    entry.handle    = op.handle;
    entry.owner     = op.owner;
    entry.manifest  = op.uiManifest.value_or(api::UiManifest{});
    entry.manifest.modNamespace = op.modNamespace;
    if (!op.path.empty()) {
        entry.manifest.id = op.path;
    }
    entry.manifest.pageScopes = {scope};
    entry.htmlBody            = op.content;
    entry.registeredAt        = at;
    return entry;
}

} // namespace

TransformedPage PageTransformer::transform(ChangePlan const& plan, source::PageSourceSnapshot const& snapshot) const {
    TransformedPage page;
    page.contextId = plan.contextId;

    for (auto const& operation : plan.operations) {
        if (operation.status != ChangeOperationStatus::Pending) {
            page.report.operations.push_back(operation);
            continue;
        }

        auto applied = operation;
        applied.status = ChangeOperationStatus::Applied;

        switch (applied.kind) {
        case ChangeOperationKind::AddScript:
            page.scripts.push_back(toScriptEntry(applied, plan.pageScope, snapshot.capturedAt));
            break;
        case ChangeOperationKind::AddStyleSheet:
            page.styles.push_back(toStyleSheetEntry(applied, plan.pageScope, snapshot.capturedAt));
            break;
        case ChangeOperationKind::AddResource:
            page.resources.push_back(toResourceEntry(applied, plan.pageScope, snapshot.capturedAt));
            break;
        case ChangeOperationKind::AddUi:
            page.uiEntries.push_back(toUiEntry(applied, plan.pageScope, snapshot.capturedAt));
            break;
        }
        page.report.operations.push_back(std::move(applied));
    }

    for (auto const& operation : page.report.operations) {
        switch (operation.status) {
        case ChangeOperationStatus::Applied:
            ++page.report.applied;
            break;
        case ChangeOperationStatus::SkippedDependencyMissing:
        case ChangeOperationStatus::SkippedVersionMismatch:
        case ChangeOperationStatus::SkippedDisabled:
            ++page.report.skipped;
            break;
        case ChangeOperationStatus::BlockedConflict:
        case ChangeOperationStatus::BlockedPermissionDenied:
        case ChangeOperationStatus::BlockedNotSupported:
            ++page.report.blocked;
            break;
        default:
            break;
        }
    }

    page.report.contextId = plan.contextId;
    page.report.success   = page.report.errors.empty();

    diagnostic::recordStage6ReportSubmitted(
        page.report.contextId,
        page.report.applied,
        page.report.skipped,
        page.report.blocked,
        page.report.success
    );

    return page;
}

} // namespace dearoreui::transform
