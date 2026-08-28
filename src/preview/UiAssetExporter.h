#pragma once

#include "diagnostic/DiagnosticLogger.h"
#include "registry/RegistryEntry.h"

#include <filesystem>
#include <string>
#include <vector>

namespace dearoreui::preview {

// One previewable UI asset, sourced directly from the runtime registry. The
// registered UIs are the single source of truth ("自动识别"): any mod UI that
// registers through DearOreUIApi is automatically captured here with zero mod
// changes. The App offline preview consumes this record to rebuild the same
// DOM + page script that would be injected into the real client.
struct UiPreviewAsset {
    std::string              entry;        // owner + "." + ui id
    std::string              title;        // ui id (display label)
    std::string              kind;         // uiKindName
    std::vector<std::string> pageScopes;   // scope names
    std::string              anchor;       // anchor name
    std::string              containerId;
    std::string              fingerprint;
    std::string              htmlBody;     // round-tripped body (diagnostic)
    // domScript is a JS expression consumable by the bootstrap renderer
    // (dearOreUiBuildDom): e.g. "[{t:'div',s:'...',c:[...]}]".
    std::string domScript;
    // Concatenated text of all <script> nodes in the UI body — the page script
    // (e.g. the calendar logic). Injected by the preview after the shim/runtime.
    std::string pageScript;
};

// Exports the currently registered UI entries into an offline-preview manifest.
//
// HARD CONSTRAINT: this is a read-only, best-effort observer. Every write is
// wrapped so failures surface only as a warning log and NEVER change the
// register/inject result (真机回归 = 不变).
class UiAssetExporter {
public:
    UiAssetExporter(diagnostic::DiagnosticLogger& logger);

    // Serializes each UiEntry into a UiPreviewAsset (pure; no IO).
    [[nodiscard]] static UiPreviewAsset toAsset(registry::UiEntry const& entry);

    // Best-effort export of `entries` to <outDir>/uiAssets.json. On failure
    // logs a warning and returns false; never throws.
    [[nodiscard]] bool exportUiEntries(std::vector<registry::UiEntry> const& entries, std::filesystem::path const& outDir);

private:
    diagnostic::DiagnosticLogger& mLogger;
};

} // namespace dearoreui::preview