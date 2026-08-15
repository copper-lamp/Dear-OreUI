#include "inject/RuntimeInjector.h"

#include "api/types/Error.h"
#include "diagnostic/Stage5IpcTelemetry.h"
#include "diagnostic/Stage7UiTelemetry.h"
#include "render/DomScriptSerializer.h"
#include "render/HtmlDomParser.h"
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

    bool const bridgeAvailable = mBridge.isAvailable();

    std::ostringstream stream;
    stream << "(function(){\n";
    stream << "    window.__DearOreUI__ = window.__DearOreUI__ || {};\n";
    stream << "    window.__DearOreUI__.protocolVersion = 1;\n";
    stream << "    window.__DearOreUI__.stage = 8;\n";
    stream << "    window.__DearOreUI__.contextId = \"" << std::to_string(id.value()) << "\";\n";
    if (bridgeAvailable) {
        // Stage 8 (H1 event channel): JS->C++ reports go through the native
        // RegisterForEvent handler via engine.trigger("dearoreui_report",
        // json). The event channel has no synchronous return value, so
        // callHost sends fire-and-forget and resolves null; synchronous
        // request/response is delivered later via ExecuteScript callbacks.
        stream << "    window.__DearOreUI__.ipc = {\n";
        stream << "        isAvailable: function() { return true; },\n";
        stream << "        callHost: function(method, args) {\n";
        stream << "            return new Promise(function(resolve, reject) {\n";
        stream << "                try {\n";
        stream << "                    if (typeof engine === 'undefined' || !engine.trigger) {\n";
        stream << "                        throw new Error('HostBridgeUnavailable');\n";
        stream << "                    }\n";
        stream << "                    var request = {\n";
        stream << "                        type: 'request',\n";
        stream << "                        id: (window.__DearOreUI__.nextRequestId = (window.__DearOreUI__.nextRequestId || 0) + 1),\n";
        stream << "                        ctx: Number(window.__DearOreUI__.contextId || 0),\n";
        stream << "                        method: method,\n";
        stream << "                        payload: (typeof args === 'string' ? args : JSON.stringify(args || {}))\n";
        stream << "                    };\n";
        stream << "                    engine.trigger('dearoreui_report', JSON.stringify(request));\n";
        stream << "                    resolve(null);\n";
        stream << "                } catch (e) {\n";
        stream << "                    reject(e);\n";
        stream << "                }\n";
        stream << "            });\n";
        stream << "        }\n";
        stream << "    };\n";
    } else {
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
    }
    stream << "    function dearOreUiReport(msg) {\n";
    stream << "        try { if (typeof engine !== 'undefined' && engine.trigger) engine.trigger('dearoreui_report', msg); } catch (e) {}\n";
    stream << "    }\n";
    stream << "    if (typeof console !== 'undefined' && console.log) {\n";
    stream << "        console.log('[DearOreUI] stage8 runtime injected, contextId="
           << escapeJsString(std::to_string(id.value())) << ", bridge="
           << (bridgeAvailable ? "true" : "false") << "');\n";
    stream << "    }\n";
    stream << "    dearOreUiReport('runtime_executed:context=' + "
           << escapeJsString(std::to_string(id.value())) << ");\n";
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
    stream << "    window.__DearOreUI__.ui.executed = true;\n";
    stream << "    window.__DearOreUI__.ui.contextId = \"" << std::to_string(id.value()) << "\";\n";
    stream << "    window.__DearOreUI__.ui.specs = [\n";

    for (auto const& item : plan.items) {
        if (item.decision != ui::UiMountDecision::Mount) {
            continue;
        }
        auto const& spec = item.spec;
        // Stage 8: parse the Mod-provided htmlBody into a DomNode forest and
        // serialize it as a compact JS node array. The bootstrap renderer
        // (dearOreUiBuildDom) builds the DOM through CSSOM only, because cohtml
        // drops style="" attributes injected via innerHTML.
        auto domNodes = render::parseHtmlFragment(spec.htmlBody);
        stream << "        {\n";
        stream << "            containerId: \"" << escapeJsString(spec.containerId) << "\",\n";
        stream << "            modNamespace: \"" << escapeJsString(spec.modNamespace) << "\",\n";
        stream << "            uiId: \"" << escapeJsString(spec.uiId) << "\",\n";
        stream << "            kind: \"" << escapeJsString(std::string(api::uiKindName(spec.kind))) << "\",\n";
        stream << "            anchorStyle: \"" << escapeJsString(anchorStyle(spec.anchor)) << "\",\n";
        stream << "            pointerEvents: " << (spec.pointerEvents ? "true" : "false") << ",\n";
        stream << "            body: " << render::serializeDomForest(domNodes) << ",\n";
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
    stream << "    window.__DearOreUI__.ui.debug = [];\n";
    stream << "    window.__DearOreUI__.ui.dbg = function(msg) {\n";
    stream << "        window.__DearOreUI__.ui.debug.push(msg);\n";
    stream << "        try { if (typeof engine !== 'undefined' && engine.trigger) engine.trigger('dearoreui_report', 'dbg:' + msg); } catch (e) {}\n";
    stream << "    };\n";
    // Stage 8: universal renderer — build the Mod's DOM tree purely through
    // CSSOM. cohtml's HTML parser DROPS style="" attributes entirely
    // (innerHTML + getAttribute('style') both failed in Stage 7.1 probes), so
    // every element is created via createElement and styled via
    // element.style.cssText, which the disk probe proved works.
    // The spec.body array was produced by the C++ DomScriptSerializer from the
    // Mod's htmlBody (stage 8 replaces the old hard-coded 'demo' branch).
    stream << "    function dearOreUiBuildDom(parent, nodes) {\n";
    stream << "        for (var i = 0; i < nodes.length; i++) {\n";
    stream << "            var n = nodes[i];\n";
    stream << "            var el = document.createElement(n.t || 'div');\n";
    stream << "            if (n.s) {\n";
    stream << "                try { el.style.cssText = n.s; } catch (e) { window.__DearOreUI__.ui.dbg('style_err:' + (e && e.message)); }\n";
    stream << "            }\n";
    stream << "            if (n.a) {\n";
    stream << "                for (var j = 0; j < n.a.length; j++) {\n";
    stream << "                    try { el.setAttribute(n.a[j][0], n.a[j][1]); } catch (e) {}\n";
    stream << "                }\n";
    stream << "            }\n";
    stream << "            if (n.x) {\n";
    stream << "                el.textContent = n.x;\n";
    stream << "            }\n";
    stream << "            if (n.c && n.c.length) {\n";
    stream << "                dearOreUiBuildDom(el, n.c);\n";
    stream << "            }\n";
    stream << "            parent.appendChild(el);\n";
    stream << "        }\n";
    stream << "    }\n";
    stream << "    window.__DearOreUI__.ui.mount = function(spec) {\n";
    stream << "        window.__DearOreUI__.ui.dbg('mount_enter:' + spec.containerId);\n";
    stream << "        var container = document.getElementById(spec.containerId);\n";
    stream << "        window.__DearOreUI__.ui.dbg('container_exists=' + !!container);\n";
    stream << "        if (!container) {\n";
    stream << "            container = document.createElement('div');\n";
    stream << "            container.id = spec.containerId;\n";
    // Stage 7.1 fix: use CSSOM (style.cssText) instead of setAttribute/setting
    // individual style.* properties — cohtml is inconsistent about reflecting
    // the style attribute, and the disk probe proved cssText works.
    // The container must itself be full-screen: cohtml resolves width:100% of
    // a position:fixed element against its nearest positioned ancestor, not
    // the viewport, so a nested 100% layer inside an unsized container
    // collapses to 0x0 (verified: layer_rect w=0,h=0).
    stream << "            try {\n";
    stream << "                container.style.cssText =\n";
    stream << "                    'position:fixed;top:0;left:0;width:100%;height:100%;' +\n";
    stream << "                    'z-index:2147483647;' +\n";
    stream << "                    'pointer-events:' + (spec.pointerEvents ? 'auto' : 'none') + ';';\n";
    stream << "                window.__DearOreUI__.ui.dbg('container_style_ok');\n";
    stream << "            } catch (e) { window.__DearOreUI__.ui.dbg('container_style_err:' + (e && e.message)); }\n";
    stream << "            var parent = document.body || document.documentElement;\n";
    stream << "            window.__DearOreUI__.ui.dbg('parent=' + (parent ? parent.tagName : 'null'));\n";
    stream << "            if (parent) {\n";
    stream << "                parent.appendChild(container);\n";
    stream << "                window.__DearOreUI__.ui.dbg('container_appended');\n";
    stream << "            } else {\n";
    stream << "                throw new Error('no document.body or documentElement');\n";
    stream << "            }\n";
    stream << "        }\n";
    // Stage 8: clear previous content, then build the Mod's DOM tree with the
    // universal CSSOM renderer (replaces the former 'demo' special case).
    stream << "        container.innerHTML = '';\n";
    stream << "        try {\n";
    stream << "            dearOreUiBuildDom(container, spec.body);\n";
    stream << "            window.__DearOreUI__.ui.dbg('body_built:' + spec.containerId);\n";
    stream << "        } catch (e) {\n";
    stream << "            window.__DearOreUI__.ui.dbg('body_build_err:' + (e && e.message));\n";
    stream << "            window.__DearOreUI__.ui.report('mount_error:' + (e && e.message));\n";
    stream << "        }\n";
    stream << "        for (var i = 0; i < spec.styles.length; i++) {\n";
    stream << "            var link = document.createElement('link');\n";
    stream << "            link.rel = 'stylesheet';\n";
    stream << "            link.href = spec.styles[i];\n";
    stream << "            if (document.head) document.head.appendChild(link);\n";
    stream << "        }\n";
    stream << "        for (var i = 0; i < spec.scripts.length; i++) {\n";
    stream << "            var script = document.createElement('script');\n";
    stream << "            script.src = spec.scripts[i];\n";
    stream << "            script.async = true;\n";
    stream << "            if (document.body) document.body.appendChild(script);\n";
    stream << "        }\n";
    stream << "    };\n";
    stream << "    window.__DearOreUI__.ui.unmount = function(containerId) {\n";
    stream << "        var container = document.getElementById(containerId);\n";
    stream << "        if (container && container.parentNode) {\n";
    stream << "            container.parentNode.removeChild(container);\n";
    stream << "        }\n";
    stream << "    };\n";
    stream << "    window.__DearOreUI__.ui.report = function(msg) {\n";
    stream << "        try { if (typeof engine !== 'undefined' && engine.call) engine.call('dearoreui_report', msg); } catch (e) {}\n";
    stream << "    };\n";
    stream << "    window.__DearOreUI__.ui.report('bootstrap_executed');\n";
    // Stage 7.1 fix: defer mounting until the document body exists. ExecuteScript
    // can run while the Coherent document is still loading; appending to a
    // missing body would silently fail inside the engine. Poll with setInterval
    // instead of a single setTimeout, and log readyState so we can tell whether
    // the script actually ran.
    stream << "    window.__DearOreUI__.ui.mountAll = function() {\n";
    stream << "        for (var i = 0; i < window.__DearOreUI__.ui.specs.length; i++) {\n";
    stream << "            var spec = window.__DearOreUI__.ui.specs[i];\n";
    stream << "            try {\n";
    stream << "                window.__DearOreUI__.ui.mount(spec);\n";
    stream << "                window.__DearOreUI__.ui.report('mounted:' + spec.containerId);\n";
    stream << "                if (window.console && console.log) console.log('[DearOreUI] mounted ' + spec.containerId);\n";
    stream << "            } catch (e) {\n";
    stream << "                window.__DearOreUI__.ui.report('mount_error:' + spec.containerId + ':' + (e && e.message));\n";
    stream << "                if (window.console && console.error) console.error('[DearOreUI] mount failed: ' + (e && e.message));\n";
    stream << "            }\n";
    stream << "        }\n";
    stream << "    };\n";
    stream << "    function dearOreUiLogReadyState(label) {\n";
    stream << "        if (window.console && console.log) {\n";
    stream << "            console.log('[DearOreUI] ' + (label || 'check') + ' readyState=' + document.readyState + ' body=' + !!document.body);\n";
    stream << "        }\n";
    stream << "    }\n";
    stream << "    function dearOreUiReadyMount() {\n";
    stream << "        if (document.body) {\n";
    stream << "            window.__DearOreUI__.ui.mountAll();\n";
    stream << "            window.__DearOreUI__.ui.mounted = true;\n";
    stream << "            window.__DearOreUI__.ui.report('mount_all_done');\n";
    stream << "            return true;\n";
    stream << "        }\n";
    stream << "        return false;\n";
    stream << "    }\n";
    stream << "    dearOreUiLogReadyState('start');\n";
    stream << "    if (!dearOreUiReadyMount()) {\n";
    stream << "        var dearOreUiIntervalId = setInterval(function() {\n";
    stream << "            dearOreUiLogReadyState('poll');\n";
    stream << "            if (dearOreUiReadyMount()) {\n";
    stream << "                clearInterval(dearOreUiIntervalId);\n";
    stream << "            }\n";
    stream << "        }, 100);\n";
    stream << "        document.addEventListener('DOMContentLoaded', function() {\n";
    stream << "            dearOreUiLogReadyState('domready');\n";
    stream << "            if (dearOreUiReadyMount()) {\n";
    stream << "                clearInterval(dearOreUiIntervalId);\n";
    stream << "            }\n";
    stream << "        });\n";
    stream << "        setTimeout(function() { clearInterval(dearOreUiIntervalId); }, 10000);\n";
    stream << "    }\n";
    stream << "})();\n";
    return stream.str();
}

} // namespace dearoreui::inject
