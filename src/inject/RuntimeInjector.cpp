#include "inject/RuntimeInjector.h"

#include "api/types/Error.h"
#include "diagnostic/Stage5IpcTelemetry.h"
#include "resource/ResourceUri.h"

#include <sstream>

namespace dearoreui::inject {

namespace {

[[nodiscard]] std::string escapeJsString(std::string_view value) {
    std::ostringstream stream;
    for (char c : value) {
        switch (c) {
        case '\\':
            stream << "\\\\";
            break;
        case '"':
            stream << "\\\"";
            break;
        case '\n':
            stream << "\\n";
            break;
        case '\r':
            stream << "\\r";
            break;
        default:
            stream << c;
            break;
        }
    }
    return stream.str();
}

} // namespace

RuntimeInjector::RuntimeInjector(diagnostic::DiagnosticLogger& logger, ipc::IHostBridge& bridge)
    : mLogger(logger), mBridge(bridge) {}

api::Result<InjectionReport> RuntimeInjector::inject(api::ContextId id, resource::IResourceIndex const& index) {
    InjectionReport report;
    report.contextId = id;

    auto scripts = index.listForPage(api::PageScope::Any);
    for (auto const& location : scripts) {
        if (location.uri.starts_with("oreui://script/")) {
            report.injectedScripts.push_back(location.uri);
        } else if (location.uri.starts_with("oreui://style/")) {
            report.injectedStyleSheets.push_back(location.uri);
        }
    }

    auto runtimeScript = generateRuntimeScript(id, index);
    if (runtimeScript.empty()) {
        report.errors.push_back(api::Error{api::ErrorCode::InternalError, "failed to generate runtime script"});
        report.success = false;
        return report;
    }

    report.hostBridgeAvailable = mBridge.isAvailable();

    diagnostic::recordStage5BridgeState(
        id,
        mBridge.isAvailable(),
        mBridge.isAvailable() ? "CoherentHostBridge" : "NullHostBridge"
    );

    if (mBridge.isAvailable()) {
        auto sendResult = mBridge.sendScript(id, runtimeScript);
        if (sendResult.isErr()) {
            report.errors.push_back(sendResult.error());
            report.success = false;
            return report;
        }
        report.injectedScripts.emplace_back("oreui://__dearoreui__/stage5-runtime.js");
    } else {
        // Stage 5 runtime script is generated even when the bridge is unavailable.
        // The script contains a safe stub that reports HostBridgeUnavailable.
        report.injectedScripts.emplace_back("oreui://__dearoreui__/stage5-runtime.js?submitted=false");
    }

    report.success = true;

    mLogger.info("inject", "runtime_script_generated")
        .withContext(id)
        .withField("script_length", std::to_string(runtimeScript.size()))
        .withField("script_count", std::to_string(report.injectedScripts.size()))
        .withField("stylesheet_count", std::to_string(report.injectedStyleSheets.size()))
        .withField("host_bridge_available", mBridge.isAvailable() ? "true" : "false")
        .emit();

    return report;
}

std::string RuntimeInjector::generateRuntimeScript(api::ContextId id, resource::IResourceIndex const& index) const {
    static_cast<void>(index);

    std::ostringstream stream;
    stream << "(function(){\n";
    stream << "    window.__DearOreUI__ = window.__DearOreUI__ || {};\n";
    stream << "    window.__DearOreUI__.protocolVersion = 1;\n";
    stream << "    window.__DearOreUI__.stage = 5;\n";
    stream << "    window.__DearOreUI__.contextId = \"" << std::to_string(id.value()) << "\";\n";
    stream << "    window.__DearOreUI__.ipc = {\n";
    stream << "        isAvailable: function() { return false; },\n";
    stream << "        callHost: function(method, args) {\n";
    stream << "            return new Promise(function(resolve, reject) {\n";
    stream << "                var err = new Error('HostBridgeUnavailable');\n";
    stream << "                err.code = 'HostBridgeUnavailable';\n";
    stream << "                reject(err);\n";
    stream << "            });\n";
    stream << "        }\n";
    stream << "    };\n";
    stream << "    if (typeof console !== 'undefined' && console.log) {\n";
    stream << "        console.log('[DearOreUI] stage5 runtime injected, contextId="
           << escapeJsString(std::to_string(id.value())) << ", bridge=false');\n";
    stream << "    }\n";
    stream << "})();\n";
    return stream.str();
}

} // namespace dearoreui::inject
