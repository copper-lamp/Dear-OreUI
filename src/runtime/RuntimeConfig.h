#pragma once

#include <filesystem>

namespace dearoreui::runtime {

struct RuntimeConfig {
    std::filesystem::path dataDirectory;
    std::filesystem::path minecraftDirectory;
    bool                  enableStage0Compatibility = true;
    bool                  enableFileDiagnostics     = true;
    bool                  enableHooks               = true; // Allow tests to disable LeviLamina hook calls.
    bool                  enableDemoOverlay         = false; // Real-client verification overlay (Stage 7.1).
    // Stage 8 crash isolation: disable_inject=1 skips the ExecuteScript flush
    // and BindCall registration in CoherentHostBridge (keeps every hook).
    // disable_bindcall=1 skips ONLY the BindCall registration, keeping the
    // ExecuteScript flush — separates the script path from the binding path.
    bool                  disableInject             = false;
    bool                  disableBindCall           = false;
    // B1c diagnostic: unbind immediately after BindCall registration (view is
    // stable at that point). Distinguishes "registration itself" from
    // "holding a binding across page teardown" as the crash trigger.
    bool                  debugUnbindImmediately    = false;
};

} // namespace dearoreui::runtime
