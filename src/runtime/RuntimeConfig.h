#pragma once

#include <filesystem>

namespace dearoreui::runtime {

struct RuntimeConfig {
    std::filesystem::path dataDirectory;
    std::filesystem::path minecraftDirectory;
    bool                  enableStage0Compatibility = true;
    bool                  enableFileDiagnostics     = true;
    bool                  enableHooks               = true; // Allow tests to disable LeviLamina hook calls.
};

} // namespace dearoreui::runtime
