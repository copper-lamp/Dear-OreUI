#include "hook/Stage5CoherentProbe.h"

#include "diagnostic/Stage5IpcTelemetry.h"

#include <cstdint>
#include <sstream>

namespace dearoreui::hook {

Stage5CoherentProbe::Stage5CoherentProbe(diagnostic::DiagnosticLogger& logger) : mLogger(logger) {}

void Stage5CoherentProbe::onViewInitialized(void* gamefaceView) {
    mExecuteScriptFound = true;
    mSummary            = "coherent View::initialize hit; executeScript available";

    diagnostic::recordStage5ViewInitialized(
        reinterpret_cast<std::uintptr_t>(gamefaceView),
        mExecuteScriptFound.load()
    );
}

void Stage5CoherentProbe::onSceneCreated(std::string_view url, void* sceneOrView) {
    std::ostringstream stream;
    stream << "url=" << url << ";view=" << reinterpret_cast<std::uintptr_t>(sceneOrView);

    mLogger.info("stage5", "scene_created_probe")
        .withField("url", std::string{url})
        .withField("view_ptr", std::to_string(reinterpret_cast<std::uintptr_t>(sceneOrView)))
        .emit();

    scanLoadedModules();
}

void Stage5CoherentProbe::scanLoadedModules() {
    // Stage 5 stub: real symbol/module scanning is deferred until telemetry
    // identifies the exact Coherent JS execution entry points. Keeping this
    // read-only prevents crashes from unverified ABI assumptions.
    // A positive hit from onViewInitialized is preserved.
    if (!mExecuteScriptFound) {
        mSummary = "stage5 stub: Coherent symbols not located";
    }
    mJsToNativeCallbackFound = false;

    diagnostic::recordStage5BridgeProbed(
        api::ContextId{},
        mExecuteScriptFound.load(),
        mJsToNativeCallbackFound.load(),
        mSummary
    );
}

bool Stage5CoherentProbe::foundExecuteScript() const { return mExecuteScriptFound.load(); }

bool Stage5CoherentProbe::foundJsToNativeCallback() const { return mJsToNativeCallbackFound.load(); }

std::string Stage5CoherentProbe::summary() const { return mSummary; }

} // namespace dearoreui::hook
