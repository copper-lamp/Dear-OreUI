#pragma once

#include "api/types/Error.h"
#include "api/types/Result.h"

#include <string>

namespace dearoreui::ui {

enum class UiState {
    Registered,
    Mounting,
    Mounted,
    Unmounting,
    Failed,
    Removed,
};

[[nodiscard]] constexpr std::string_view uiStateName(UiState state) {
    switch (state) {
    case UiState::Registered:
        return "registered";
    case UiState::Mounting:
        return "mounting";
    case UiState::Mounted:
        return "mounted";
    case UiState::Unmounting:
        return "unmounting";
    case UiState::Failed:
        return "failed";
    case UiState::Removed:
        return "removed";
    }
    return "unknown";
}

class UiStateMachine {
public:
    [[nodiscard]] UiState state() const { return mState; }

    [[nodiscard]] api::Result<UiState> transitionTo(UiState target);

    [[nodiscard]] api::Result<UiState> mount();
    [[nodiscard]] api::Result<UiState> markMounted();
    [[nodiscard]] api::Result<UiState> markFailed(std::string reason);
    [[nodiscard]] api::Result<UiState> unmount();
    [[nodiscard]] api::Result<UiState> markRemoved();

    [[nodiscard]] std::string const& failureReason() const { return mFailureReason; }

private:
    UiState     mState{UiState::Registered};
    std::string mFailureReason;
};

} // namespace dearoreui::ui
