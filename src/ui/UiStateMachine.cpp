#include "ui/UiState.h"

namespace dearoreui::ui {

namespace {

[[nodiscard]] bool canTransition(UiState from, UiState to) {
    switch (from) {
    case UiState::Registered:
        return to == UiState::Mounting;
    case UiState::Mounting:
        return to == UiState::Mounted || to == UiState::Failed;
    case UiState::Mounted:
        return to == UiState::Unmounting;
    case UiState::Unmounting:
        return to == UiState::Removed;
    case UiState::Failed:
        return to == UiState::Removed || to == UiState::Unmounting;
    case UiState::Removed:
        return false;
    }
    return false;
}

} // namespace

api::Result<UiState> UiStateMachine::transitionTo(UiState target) {
    if (!canTransition(mState, target)) {
        return api::Error{
            api::ErrorCode::InvalidState,
            "invalid ui state transition from " + std::string(uiStateName(mState)) + " to "
                + std::string(uiStateName(target))
        };
    }
    mState = target;
    return mState;
}

api::Result<UiState> UiStateMachine::mount() { return transitionTo(UiState::Mounting); }

api::Result<UiState> UiStateMachine::markMounted() { return transitionTo(UiState::Mounted); }

api::Result<UiState> UiStateMachine::markFailed(std::string reason) {
    mFailureReason = std::move(reason);
    return transitionTo(UiState::Failed);
}

api::Result<UiState> UiStateMachine::unmount() {
    if (mState == UiState::Failed) {
        return transitionTo(UiState::Removed);
    }
    return transitionTo(UiState::Unmounting);
}

api::Result<UiState> UiStateMachine::markRemoved() { return transitionTo(UiState::Removed); }

} // namespace dearoreui::ui
