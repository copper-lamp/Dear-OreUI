#pragma once

#include "diagnostic/DiagnosticLogger.h"

#include <atomic>
#include <string>
#include <string_view>

namespace dearoreui::hook {

class Stage5CoherentProbe {
public:
    explicit Stage5CoherentProbe(diagnostic::DiagnosticLogger& logger);

    // Called from SceneProvider::createScene read-only hook.
    void onSceneCreated(std::string_view url, void* sceneOrView);

    // Called from the OreUI::View::initialize hook once a real gameface view
    // (cohtml::View*) has been captured; executeScript becomes available.
    void onViewInitialized(void* gamefaceView);

    // Scans loaded modules for Coherent/JS execution symbols.
    // Stage 5 stub always reports not found; real implementation is filled
    // after telemetry identifies the correct entry points.
    void scanLoadedModules();

    [[nodiscard]] bool        foundExecuteScript() const;
    [[nodiscard]] bool        foundJsToNativeCallback() const;
    [[nodiscard]] std::string summary() const;

private:
    diagnostic::DiagnosticLogger& mLogger;
    std::atomic<bool>             mExecuteScriptFound{false};
    std::atomic<bool>             mJsToNativeCallbackFound{false};
    std::string                   mSummary{"not probed"};
};

} // namespace dearoreui::hook
