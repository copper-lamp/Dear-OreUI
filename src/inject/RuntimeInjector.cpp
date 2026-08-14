#include "inject/RuntimeInjector.h"

#include "api/types/Error.h"
#include "diagnostic/Stage5IpcTelemetry.h"
#include "diagnostic/Stage7UiTelemetry.h"
#include "resource/ResourceUri.h"
#include "ui/UiManifest.h"

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

namespace {

[[nodiscard]] std::string anchorStyle(api::UiAnchor anchor) {
    switch (anchor) {
    case api::UiAnchor::TopLeft:
        return "top:0;left:0;";
    case api::UiAnchor::TopCenter:
        return "top:0;left:50%;transform:translateX(-50%);";
    case api::UiAnchor::TopRight:
        return "top:0;right:0;";
    case api::UiAnchor::CenterLeft:
        return "top:50%;left:0;transform:translateY(-50%);";
    case api::UiAnchor::Center:
        return "top:50%;left:50%;transform:translate(-50%,-50%);";
    case api::UiAnchor::CenterRight:
        return "top:50%;right:0;transform:translateY(-50%);";
    case api::UiAnchor::BottomLeft:
        return "bottom:0;left:0;";
    case api::UiAnchor::BottomCenter:
        return "bottom:0;left:50%;transform:translateX(-50%);";
    case api::UiAnchor::BottomRight:
        return "bottom:0;right:0;";
    case api::UiAnchor::FullScreen:
        return "top:0;left:0;width:100%;height:100%;";
    }
    return "top:0;left:0;";
}

[[nodiscard]] std::string oreUri(std::string_view scheme, std::string_view ns, std::string_view path) {
    return std::string{"oreui://"} + std::string{scheme} + "/" + std::string{ns} + "/" + std::string{path};
}

} // namespace

api::Result<InjectionReport> RuntimeInjector::injectUi(api::ContextId id, ui::UiMountPlan const& plan) {
    InjectionReport report;
    report.contextId = id;

    auto bootstrapScript = generateUiBootstrapScript(id, plan);
    if (bootstrapScript.empty()) {
        report.errors.push_back(api::Error{api::ErrorCode::InternalError, "failed to generate ui bootstrap script"});
        report.success = false;
        return report;
    }

    report.hostBridgeAvailable = mBridge.isAvailable();

    std::size_t mountedCount{0};
    for (auto const& item : plan.items) {
        if (item.decision == ui::UiMountDecision::Mount) {
            ++mountedCount;
        }
    }
    report.uiCount = mountedCount;

    if (mBridge.isAvailable()) {
        auto sendResult = mBridge.sendScript(id, bootstrapScript);
        if (sendResult.isErr()) {
            report.errors.push_back(sendResult.error());
            report.success = false;
            return report;
        }
        report.injectedScripts.emplace_back("oreui://__dearoreui__/stage7-ui-bootstrap.js");
    } else {
        report.injectedScripts.emplace_back("oreui://__dearoreui__/stage7-ui-bootstrap.js?submitted=false");
    }

    report.success = true;

    mLogger.info("inject", "ui_bootstrap_generated")
        .withContext(id)
        .withField("script_length", std::to_string(bootstrapScript.size()))
        .withField("ui_count", std::to_string(report.uiCount))
        .withField("host_bridge_available", mBridge.isAvailable() ? "true" : "false")
        .emit();

    return report;
}

std::string RuntimeInjector::generateUiBootstrapScript(api::ContextId id, ui::UiMountPlan const& plan) const {
    std::ostringstream stream;
    stream << "(function(){\n";
    stream << "    window.__DearOreUI__ = window.__DearOreUI__ || {};\n";
    stream << "    window.__DearOreUI__.ui = window.__DearOreUI__.ui || {};\n";
    stream << "    window.__DearOreUI__.ui.contextId = \"" << std::to_string(id.value()) << "\";\n";
    stream << "    window.__DearOreUI__.ui.specs = [\n";

    for (auto const& item : plan.items) {
        if (item.decision != ui::UiMountDecision::Mount) {
            continue;
        }
        auto const& spec = item.spec;
        stream << "        {\n";
        stream << "            containerId: \"" << escapeJsString(spec.containerId) << "\",\n";
        stream << "            modNamespace: \"" << escapeJsString(spec.modNamespace) << "\",\n";
        stream << "            uiId: \"" << escapeJsString(spec.uiId) << "\",\n";
        stream << "            kind: \"" << escapeJsString(std::string(api::uiKindName(spec.kind))) << "\",\n";
        stream << "            htmlBody: \"" << escapeJsString(spec.htmlBody) << "\",\n";
        stream << "            anchorStyle: \"" << escapeJsString(anchorStyle(spec.anchor)) << "\",\n";
        stream << "            pointerEvents: " << (spec.pointerEvents ? "true" : "false") << ",\n";
        stream << "            scripts: [";
        for (std::size_t index = 0; index < spec.scripts.size(); ++index) {
            if (index > 0) {
                stream << ",";
            }
            stream << "\"" << escapeJsString(oreUri("script", spec.modNamespace, spec.scripts[index])) << "\"";
        }
        stream << "],\n";
        stream << "            styles: [";
        for (std::size_t index = 0; index < spec.styles.size(); ++index) {
            if (index > 0) {
                stream << ",";
            }
            stream << "\"" << escapeJsString(oreUri("style", spec.modNamespace, spec.styles[index])) << "\"";
        }
        stream << "]\n";
        stream << "        },\n";
    }

    stream << "    ];\n";
    stream << "    window.__DearOreUI__.ui.mount = function(spec) {\n";
    stream << "        var container = document.getElementById(spec.containerId);\n";
    stream << "        if (!container) {\n";
    stream << "            container = document.createElement('div');\n";
    stream << "            container.id = spec.containerId;\n";
    stream << "            container.style.position = 'fixed';\n";
    stream << "            container.style.zIndex = '9999';\n";
    stream << "            container.style.pointerEvents = spec.pointerEvents ? 'auto' : 'none';\n";
    stream << "            container.setAttribute('style', container.getAttribute('style') + spec.anchorStyle);\n";
    stream << "            document.body.appendChild(container);\n";
    stream << "        }\n";
    stream << "        container.innerHTML = spec.htmlBody;\n";
    stream << "        for (var i = 0; i < spec.styles.length; i++) {\n";
    stream << "            var link = document.createElement('link');\n";
    stream << "            link.rel = 'stylesheet';\n";
    stream << "            link.href = spec.styles[i];\n";
    stream << "            document.head.appendChild(link);\n";
    stream << "        }\n";
    stream << "        for (var i = 0; i < spec.scripts.length; i++) {\n";
    stream << "            var script = document.createElement('script');\n";
    stream << "            script.src = spec.scripts[i];\n";
    stream << "            script.async = true;\n";
    stream << "            document.body.appendChild(script);\n";
    stream << "        }\n";
    stream << "    };\n";
    stream << "    window.__DearOreUI__.ui.unmount = function(containerId) {\n";
    stream << "        var container = document.getElementById(containerId);\n";
    stream << "        if (container && container.parentNode) {\n";
    stream << "            container.parentNode.removeChild(container);\n";
    stream << "        }\n";
    stream << "    };\n";
    stream << "    window.__DearOreUI__.ui.specs.forEach(function(spec) {\n";
    stream << "        window.__DearOreUI__.ui.mount(spec);\n";
    stream << "    });\n";
    stream << "})();\n";
    return stream.str();
}

} // namespace dearoreui::inject
