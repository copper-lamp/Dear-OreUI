#include "ui/MountManager.h"

#include "diagnostic/Stage7UiTelemetry.h"

namespace dearoreui::ui {

MountManager::MountManager(IMountHost& host) : mHost(host) {}

api::Result<UiMountPlan> MountManager::mountPage(api::ContextId contextId, UiMountPlan plan) {
    std::lock_guard lock{mMutex};

    auto& handles = mActiveMounts[contextId];

    for (auto& item : plan.items) {
        if (item.decision != UiMountDecision::Mount) {
            continue;
        }

        // Idempotency: do not mount the same UI handle twice in the same context.
        bool alreadyMounted = false;
        for (auto const& existing : handles) {
            if (existing.handle().value() == item.handle.value()) {
                alreadyMounted = true;
                break;
            }
        }
        if (alreadyMounted) {
            item.decision = UiMountDecision::Skip;
            item.reason   = "already mounted in context";
            ++plan.skipped;
            --plan.mounted;
            continue;
        }

        MountHandle handle{item.handle, item.spec};
        auto        stateResult = handle.stateMachine().mount();
        if (stateResult.isErr()) {
            item.decision = UiMountDecision::Blocked;
            item.reason   = stateResult.error().message;
            static_cast<void>(handle.stateMachine().markFailed(stateResult.error().message));
            plan.errors.push_back(stateResult.error());
            ++plan.blocked;
            --plan.mounted;
            diagnostic::recordStage7UiFailed(contextId, item.spec.modNamespace, item.spec.uiId, stateResult.error().message);
            continue;
        }

        auto hostResult = mHost.createContainer(contextId, item.spec);
        if (hostResult.isErr()) {
            item.decision = UiMountDecision::Blocked;
            item.reason   = hostResult.error().message;
            static_cast<void>(handle.stateMachine().markFailed(hostResult.error().message));
            plan.errors.push_back(hostResult.error());
            ++plan.blocked;
            --plan.mounted;
            diagnostic::recordStage7UiFailed(contextId, item.spec.modNamespace, item.spec.uiId, hostResult.error().message);
            continue;
        }

        static_cast<void>(handle.stateMachine().markMounted());
        diagnostic::recordStage7UiMounted(contextId, item.spec.modNamespace, item.spec.uiId, item.spec.containerId);
        handles.push_back(std::move(handle));
    }

    plan.success = plan.errors.empty();
    return plan;
}

api::Result<void> MountManager::unmountPage(api::ContextId contextId) {
    std::lock_guard lock{mMutex};

    auto iterator = mActiveMounts.find(contextId);
    if (iterator == mActiveMounts.end()) {
        return api::Result<void>::success();
    }

    for (auto& handle : iterator->second) {
        auto& stateMachine = handle.stateMachine();
        if (stateMachine.state() == UiState::Mounted) {
            static_cast<void>(stateMachine.unmount());
            auto removeResult = mHost.removeContainer(contextId, handle.spec().containerId);
            if (removeResult.isErr()) {
                // Log but continue cleanup; the container may already be gone.
                diagnostic::recordStage7UiFailed(
                    contextId,
                    handle.spec().modNamespace,
                    handle.spec().uiId,
                    removeResult.error().message
                );
            }
        }
        static_cast<void>(stateMachine.markRemoved());
        diagnostic::recordStage7UiUnmounted(contextId, handle.spec().modNamespace, handle.spec().uiId);
    }

    mActiveMounts.erase(iterator);
    return api::Result<void>::success();
}

bool MountManager::hasMounted(api::ContextId contextId, api::RegistrationHandle handle) const {
    std::lock_guard lock{mMutex};
    auto            iterator = mActiveMounts.find(contextId);
    if (iterator == mActiveMounts.end()) {
        return false;
    }
    for (auto const& existing : iterator->second) {
        if (existing.handle().value() == handle.value()) {
            return true;
        }
    }
    return false;
}

std::size_t MountManager::activeMountCount(api::ContextId contextId) const {
    std::lock_guard lock{mMutex};
    auto            iterator = mActiveMounts.find(contextId);
    if (iterator == mActiveMounts.end()) {
        return 0;
    }
    return iterator->second.size();
}

std::size_t MountManager::totalActiveCount() const {
    std::lock_guard lock{mMutex};
    std::size_t     total{0};
    for (auto const& [contextId, handles] : mActiveMounts) {
        static_cast<void>(contextId);
        total += handles.size();
    }
    return total;
}

} // namespace dearoreui::ui
