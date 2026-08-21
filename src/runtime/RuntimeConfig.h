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
    bool                  enableComponentShowcase   = false; // Stage 8.1: full component library showcase page.
};

} // namespace dearoreui::runtime
