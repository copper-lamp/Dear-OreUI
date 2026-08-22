#pragma once

#include "api/types/Id.h"
#include "ui/OverlaySpec.h"
#include "ui/UiState.h"

#include <memory>

namespace dearoreui::ui {

class MountHandle {
public:
    MountHandle(api::RegistrationHandle handle, OverlaySpec spec) : mHandle(handle), mSpec(std::move(spec)) {}

    [[nodiscard]] api::RegistrationHandle handle() const { return mHandle; }
    [[nodiscard]] OverlaySpec const&      spec() const { return mSpec; }
    [[nodiscard]] UiStateMachine&         stateMachine() { return mStateMachine; }
    [[nodiscard]] UiStateMachine const&   stateMachine() const { return mStateMachine; }

    [[nodiscard]] bool isTerminal() const {
        auto state = mStateMachine.state();
        return state == UiState::Removed || state == UiState::Failed;
    }

private:
    api::RegistrationHandle mHandle;
    OverlaySpec             mSpec;
    UiStateMachine          mStateMachine;
};

} // namespace dearoreui::ui
