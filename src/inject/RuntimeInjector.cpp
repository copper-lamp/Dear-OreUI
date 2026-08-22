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
: mLogger(logger),
  mBridge(bridge) {}

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
    // Stage 8-A: JS->C++ runs over the vanilla facet protocol. The page
    // triggers engine.trigger("facet:request", "dearoreui", id, {params}),
    // which the game's PRE-REGISTERED native facet:request handler dispatches
    // into the "dearoreui" facet the mod registered into the view's
    // IFacetRegistry (no engine binding registration involved — those crash
    // this client). Responses come back through the page bus
    // (window.__DearOreUI__.bus.push) driven by C++ ExecuteScript.
    stream << "    var hasFacet = (typeof engine !== 'undefined') && engine && engine.trigger;\n";
    stream << "    window.__DearOreUI__.bus = window.__DearOreUI__.bus || (function() {\n";
    stream << "        var pending = {};\n";
    stream << "        return {\n";
    stream << "            on: function(id, cb) { pending[id] = cb; },\n";
    stream << "            off: function(id) { delete pending[id]; },\n";
    stream << "            push: function(id, jsonString) {\n";
    stream << "                var cb = pending[id];\n";
    stream << "                if (!cb) return;\n";
    stream << "                delete pending[id];\n";
    stream << "                var resp = null;\n";
    stream << "                try { resp = JSON.parse(jsonString); } catch (e) { resp = { type: 'response', id: id, "
              "error: 1, payload: 'bad response json' }; }\n";
    stream << "                cb(resp);\n";
    stream << "            }\n";
    stream << "        };\n";
    stream << "    })();\n";
    stream << "    function dearOreUiFacetTrigger(params) {\n";
    stream << "        try {\n";
    stream << "            if (!hasFacet) return false;\n";
    stream << "            engine.trigger('facet:request', 'dearoreui', 'dearoreui', { params: params });\n";
    stream << "            return true;\n";
    stream << "        } catch (e) { return false; }\n";
    stream << "    }\n";
    stream << "    window.__DearOreUI__.ipc = {\n";
    stream << "        isAvailable: function() { return hasFacet; },\n";
    stream << "        callHost: function(method, args) {\n";
    stream << "            return new Promise(function(resolve, reject) {\n";
    stream << "                try {\n";
    stream << "                    if (!hasFacet) throw new Error('HostBridgeUnavailable');\n";
    stream << "                    var id = (window.__DearOreUI__.nextRequestId = (window.__DearOreUI__.nextRequestId "
              "|| 0) + 1);\n";
    stream << "                    var request = {\n";
    stream << "                        type: 'request',\n";
    stream << "                        id: id,\n";
    stream << "                        ctx: Number(window.__DearOreUI__.contextId || 0),\n";
    stream << "                        method: method,\n";
    stream << "                        payload: (typeof args === 'string' ? args : JSON.stringify(args || {}))\n";
    stream << "                    };\n";
    stream << "                    var timer = setTimeout(function() { window.__DearOreUI__.bus.off(id); reject(new "
              "Error('HostCallTimeout')); }, 5000);\n";
    stream << "                    window.__DearOreUI__.bus.on(id, function(resp) { clearTimeout(timer); "
              "resolve(resp); });\n";
    stream << "                    if (!dearOreUiFacetTrigger(JSON.stringify(request))) {\n";
    stream << "                        clearTimeout(timer);\n";
    stream << "                        window.__DearOreUI__.bus.off(id);\n";
    stream << "                        throw new Error('FacetUnavailable');\n";
    stream << "                    }\n";
    stream << "                } catch (e) { reject(e); }\n";
    stream << "            });\n";
    stream << "        },\n";
    stream << "        report: function(msg) {\n";
    stream << "            try {\n";
    stream << "                if (window.__DearOreUI__ && window.__DearOreUI__.silent) return;\n";
    stream << "                dearOreUiFacetTrigger((typeof msg === 'string') ? msg : JSON.stringify(msg));\n";
    stream << "            } catch (e) {}\n";
    stream << "        },\n";
    stream << "        send: function(msg) { return this.report(msg); }\n";
    stream << "    };\n";
    stream << "    window.DearOreUI = window.DearOreUI || {\n";
    stream << "        call: function(method, args) { return window.__DearOreUI__.ipc.callHost(method, args); },\n";
    stream << "        report: function(msg) { return window.__DearOreUI__.ipc.report(msg); }\n";
    stream << "    };\n";
    stream << "    if (typeof console !== 'undefined' && console.log) {\n";
    stream << "        console.log('[DearOreUI] stage8 runtime injected, contextId="
           << escapeJsString(std::to_string(id.value())) << ", bridge=" << (bridgeAvailable ? "true" : "false")
           << ", facet=' + (hasFacet ? 'yes' : 'no') + '');\n";
    stream << "    }\n";
    // Stage 8-A final contract: the facet channel is SINGLE-SHOT per view.
    // The first facet:request dispatch on a view terminates that page's JS
    // context (verified rounds 3-6), so the one dispatch slot must be reserved
    // for the Mod's business call. silent=true mutes the diagnostic reports
    // (bootstrap_executed, mount chain, ...) so they never consume the slot;
    // Mod code fires the single request via DearOreUI.call(...) from a user
    // event handler. Set window.__DearOreUI__.silent = false to re-enable
    // reports for debugging.
    stream << "    window.__DearOreUI__.silent = true;\n";
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

// Serializes one UI spec as a JS object literal (the `specs` array entry).
// Shared by the full bootstrap script and the body-only follow-up scripts.
// `bodyOverride` (optional) replaces the spec's own body — used when a large
// UI is injected in chunks.
void appendUiSpec(
    std::ostream&                       stream,
    ui::OverlaySpec const&              spec,
    std::vector<render::DomNode> const* bodyOverride = nullptr
) {
    // Stage 8: parse the Mod-provided htmlBody into a DomNode forest and
    // serialize it as a compact JS node array. The bootstrap renderer
    // (dearOreUiBuildDom) builds the DOM through CSSOM only, because cohtml
    // drops style="" attributes injected via innerHTML.
    //
    // M8.1.2: component-registered UIs carry a pre-rendered DomNode forest
    // (spec.domNodes) with per-state cssText (stateStyles) that the htmlBody
    // round-trip cannot represent; prefer it over parsing htmlBody.
    auto domNodes = bodyOverride != nullptr
                      ? *bodyOverride
                      : (spec.domNodes.empty() ? render::parseHtmlFragment(spec.htmlBody) : spec.domNodes);
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
    stream << "        }\n";
}

// Splits a UI body into chunks so each injected script stays under cohtml's
// ExecuteScript size limit. A single container node with many children (e.g.
// the showcase root panel) is split by children; otherwise the body is one
// chunk. Returns groups of the root's children (NOT wrapped in the root) so
// the first chunk mounts the root and later chunks append into it.
std::vector<std::vector<render::DomNode>>
chunkUiBody(std::vector<render::DomNode> const& body, std::size_t maxChunkNodes) {
    std::vector<std::vector<render::DomNode>> chunks;
    if (body.size() == 1 && body[0].children.size() > maxChunkNodes) {
        auto const& root = body[0];
        for (std::size_t i = 0; i < root.children.size(); i += maxChunkNodes) {
            std::vector<render::DomNode> group;
            auto const                   end = std::min(i + maxChunkNodes, root.children.size());
            for (std::size_t j = i; j < end; ++j) {
                group.push_back(root.children[j]);
            }
            chunks.push_back(std::move(group));
        }
    } else {
        chunks.push_back(body);
    }
    return chunks;
}

} // namespace

api::Result<InjectionReport> RuntimeInjector::injectUi(api::ContextId id, ui::UiMountPlan const& plan) {
    InjectionReport report;
    report.contextId = id;

    report.hostBridgeAvailable = mBridge.isAvailable();

    std::size_t mountedCount{0};
    for (auto const& item : plan.items) {
        if (item.decision == ui::UiMountDecision::Mount) {
            ++mountedCount;
        }
    }
    report.uiCount = mountedCount;

    // M8.1.2: cohtml ExecuteScript silently drops large scripts (verified: a
    // ~47KB combined bootstrap never executed, while a ~9KB one did). Inject
    // the machinery ONCE as a small script, then ONE small body-only script
    // per UI that reuses it. No single ExecuteScript exceeds the limit, and a
    // failure in one UI cannot take down the others.
    std::size_t submitted{0};
    auto        submitScript = [&](std::string const& script) -> bool {
        if (mBridge.isAvailable()) {
            auto sendResult = mBridge.sendScript(id, script);
            if (sendResult.isErr()) {
                report.errors.push_back(sendResult.error());
                report.success = false;
                return false;
            }
            report.injectedScripts.emplace_back("oreui://__dearoreui__/stage7-ui-bootstrap.js");
        } else {
            report.injectedScripts.emplace_back("oreui://__dearoreui__/stage7-ui-bootstrap.js?submitted=false");
        }
        ++submitted;
        return true;
    };

    // 1) Machinery script (no spec) — must run before any body-only script.
    {
        auto machineryScript = generateUiBootstrapScript(id, ui::UiMountItem{});
        if (machineryScript.empty() || !submitScript(machineryScript)) {
            report.success = false;
            return report;
        }
        mLogger.info("inject", "ui_machinery_generated")
            .withContext(id)
            .withField("script_length", std::to_string(machineryScript.size()))
            .emit();
    }

    // 2) One body-only script per UI. Large bodies (single container with many
    // children, e.g. the showcase root panel) are injected in chunks: the
    // first chunk mounts the container with the root + first group, the rest
    // append the remaining children into the root — each ExecuteScript stays
    // under cohtml's silent size limit.
    for (auto const& item : plan.items) {
        if (item.decision != ui::UiMountDecision::Mount) {
            continue;
        }
        auto body   = item.spec.domNodes.empty() ? render::parseHtmlFragment(item.spec.htmlBody) : item.spec.domNodes;
        auto chunks = chunkUiBody(body, 4);
        for (std::size_t chunkIndex = 0; chunkIndex < chunks.size(); ++chunkIndex) {
            std::string script;
            if (chunkIndex == 0) {
                // First chunk: mount the root with the first group of children.
                std::vector<render::DomNode> mountBody;
                if (body.size() == 1 && chunks.size() > 1) {
                    render::DomNode root = body[0];
                    root.children        = chunks[0];
                    mountBody.push_back(std::move(root));
                } else {
                    mountBody = chunks[0];
                }
                script = generateUiBodyScript(id, item, &mountBody);
            } else {
                // Later chunks: append the remaining children into the root.
                script = generateUiAppendScript(id, item.spec.containerId, chunks[chunkIndex]);
            }
            if (script.empty() || !submitScript(script)) {
                report.success = false;
                return report;
            }
            mLogger.info("inject", "ui_bootstrap_generated")
                .withContext(id)
                .withField("script_length", std::to_string(script.size()))
                .withField("ui_id", item.spec.uiId)
                .withField("chunk", std::to_string(chunkIndex))
                .withField("host_bridge_available", mBridge.isAvailable() ? "true" : "false")
                .emit();
        }
    }

    report.success = true;

    mLogger.info("inject", "ui_bootstrap_batch")
        .withContext(id)
        .withField("ui_count", std::to_string(report.uiCount))
        .withField("submitted", std::to_string(submitted))
        .emit();

    return report;
}

std::string RuntimeInjector::generateUiBootstrapScript(api::ContextId id, ui::UiMountItem const& item) const {
    static_cast<void>(item);
    std::ostringstream stream;
    stream << "(function(){\n";
    stream << "    window.__DearOreUI__ = window.__DearOreUI__ || {};\n";
    stream << "    window.__DearOreUI__.ui = window.__DearOreUI__.ui || {};\n";
    stream << "    window.__DearOreUI__.ui.executed = true;\n";
    stream << "    window.__DearOreUI__.ui.contextId = \"" << std::to_string(id.value()) << "\";\n";
    // M8.1.2 size fix: the machinery script carries NO spec. Every UI is
    // injected as a small body-only follow-up (generateUiBodyScript) that
    // pushes its spec and mounts it, so no single ExecuteScript exceeds
    // cohtml's silent size limit regardless of plan ordering.
    stream << "    window.__DearOreUI__.ui.specs = [];\n";
    stream << "    window.__DearOreUI__.ui.debug = [];\n";
    stream << "    window.__DearOreUI__.ui.dbg = function(msg) {\n";
    stream << "        window.__DearOreUI__.ui.debug.push(msg);\n";
    stream << "        try { if (window.__DearOreUI__ && window.__DearOreUI__.ipc) "
              "window.__DearOreUI__.ipc.report('dbg:' + msg); } catch (e) {}\n";
    stream << "    };\n";
    // Stage 8: universal renderer — build the Mod's DOM tree purely through
    // CSSOM. cohtml's HTML parser DROPS style="" attributes entirely
    // (innerHTML + getAttribute('style') both failed in Stage 7.1 probes), so
    // every element is created via createElement and styled via
    // element.style.cssText, which the disk probe proved works.
    // The spec.body array was produced by the C++ DomScriptSerializer from the
    // Mod's htmlBody (stage 8 replaces the old hard-coded 'demo' branch).
    stream << "    function dearOreUiBuildDom(parent, nodes) {\n";
    stream << "        var last = null;\n";
    stream << "        for (var i = 0; i < nodes.length; i++) {\n";
    stream << "            var n = nodes[i];\n";
    stream << "            var el = document.createElement(n.t || 'div');\n";
    stream << "            if (n.s) {\n";
    stream << "                try { el.style.cssText = n.s; } catch (e) { window.__DearOreUI__.ui.dbg('style_err:' + "
              "(e && e.message)); }\n";
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
    stream << "            last = el;\n";
    // M8.1.2: interactive components carry per-state cssText (st:). Store them
    // on the element and wire hover/pressed/focused events so the bootstrap can
    // swap element.style.cssText on interaction. Wiring is wrapped in try/catch
    // and runs AFTER appendChild: cohtml throws for unsupported event types
    // (verified: mouseenter aborts the whole body build), so a wiring failure
    // must never prevent the page from rendering.
    stream << "            if (n.st) {\n";
    stream << "                el.__dearOreUiStates = {};\n";
    stream << "                el.__dearOreUiBase = n.b || '';\n";
    stream << "                for (var k = 0; k < n.st.length; k++) {\n";
    stream << "                    el.__dearOreUiStates[n.st[k][0]] = n.st[k][1];\n";
    stream << "                }\n";
    stream << "                try { dearOreUiWireState(el); } catch (e) {}\n";
    stream << "            }\n";
    stream << "        }\n";
    stream << "        return last;\n";
    stream << "    }\n";
    // M8.1.2: runtime state switching. dearOreUiSetState swaps the element's
    // full cssText to the requested state's texture; dearOreUiWireState binds
    // mouse/focus events. Disabled elements (aria-disabled) never switch. This
    // is pure DOM work — no facet dispatch, so it never consumes the single
    // JS->C++ request slot per view.
    stream << "    function dearOreUiSetState(el, state) {\n";
    stream << "        if (!el || !el.__dearOreUiStates) return;\n";
    stream << "        if (el.__dearOreUiDisabled) return;\n";
    stream << "        var css = el.__dearOreUiStates[state];\n";
    stream << "        if (css === undefined) css = el.__dearOreUiStates['default'];\n";
    stream << "        if (css === undefined) return;\n";
    stream << "        try { el.style.cssText = (el.__dearOreUiBase || '') + css; } catch (e) {}\n";
    stream << "        el.__dearOreUiState = state;\n";
    stream << "    }\n";
    // Safe event binding: cohtml throws for unsupported event types, so each
    // binding is isolated and hover falls back to mouseover/mouseout when
    // mouseenter/mouseleave are unavailable.
    stream << "    function dearOreUiOn(el, type, fn) {\n";
    stream << "        try { el.addEventListener(type, fn); return true; } catch (e) { return false; }\n";
    stream << "    }\n";
    stream << "    function dearOreUiWireState(el) {\n";
    stream << "        if (!el || !el.__dearOreUiStates) return;\n";
    stream << "        el.__dearOreUiDisabled = (el.getAttribute('aria-disabled') === 'true');\n";
    stream << "        if (!dearOreUiOn(el, 'mouseenter', function() {\n";
    stream << "            if (el.__dearOreUiDisabled) return;\n";
    stream << "            dearOreUiSetState(el, 'hovered');\n";
    stream << "        })) {\n";
    stream << "            dearOreUiOn(el, 'mouseover', function() {\n";
    stream << "                if (el.__dearOreUiDisabled) return;\n";
    stream << "                dearOreUiSetState(el, 'hovered');\n";
    stream << "            });\n";
    stream << "        }\n";
    stream << "        if (!dearOreUiOn(el, 'mouseleave', function() {\n";
    stream << "            if (el.__dearOreUiDisabled) return;\n";
    stream << "            dearOreUiSetState(el, el.__dearOreUiFocused ? 'focused' : 'default');\n";
    stream << "        })) {\n";
    stream << "            dearOreUiOn(el, 'mouseout', function() {\n";
    stream << "                if (el.__dearOreUiDisabled) return;\n";
    stream << "                dearOreUiSetState(el, el.__dearOreUiFocused ? 'focused' : 'default');\n";
    stream << "            });\n";
    stream << "        }\n";
    stream << "        dearOreUiOn(el, 'mousedown', function() {\n";
    stream << "            if (el.__dearOreUiDisabled) return;\n";
    stream << "            dearOreUiSetState(el, el.__dearOreUiFocused ? 'pressedFocused' : 'pressed');\n";
    stream << "        });\n";
    stream << "        dearOreUiOn(el, 'mouseup', function() {\n";
    stream << "            if (el.__dearOreUiDisabled) return;\n";
    stream << "            dearOreUiSetState(el, el.__dearOreUiFocused ? 'focused' : 'hovered');\n";
    stream << "        });\n";
    stream << "        dearOreUiOn(el, 'focus', function() {\n";
    stream << "            el.__dearOreUiFocused = true;\n";
    stream << "            if (el.__dearOreUiDisabled) return;\n";
    stream << "            dearOreUiSetState(el, 'focused');\n";
    stream << "        });\n";
    stream << "        dearOreUiOn(el, 'blur', function() {\n";
    stream << "            el.__dearOreUiFocused = false;\n";
    stream << "            if (el.__dearOreUiDisabled) return;\n";
    stream << "            dearOreUiSetState(el, 'default');\n";
    stream << "        });\n";
    stream << "    }\n";
    // M8.1.2: programmatic state API for Mods (e.g. keyboard-navigation focus).
    stream << "    window.__DearOreUI__.ui.setState = function(elOrId, state) {\n";
    stream << "        var el = (typeof elOrId === 'string') ? document.getElementById(elOrId) : elOrId;\n";
    stream << "        if (el) dearOreUiSetState(el, state);\n";
    stream << "    };\n";
    stream << "    window.__DearOreUI__.ui.getState = function(elOrId) {\n";
    stream << "        var el = (typeof elOrId === 'string') ? document.getElementById(elOrId) : elOrId;\n";
    stream << "        return el ? (el.__dearOreUiState || 'default') : null;\n";
    stream << "    };\n";
    stream << "    window.__DearOreUI__.ui.mount = function(spec) {\n";
    stream << "        var container = document.getElementById(spec.containerId);\n";
    stream << "        if (!container) {\n";
    stream << "            container = document.createElement('div');\n";
    stream << "            container.id = spec.containerId;\n";
    // Stage 7.1 fix: use CSSOM (style.cssText) instead of setAttribute/setting
    // individual style.* properties — cohtml is inconsistent about reflecting
    // the style attribute, and the disk probe proved cssText works.
    // The container must itself be full-screen: cohtml resolves width:100% of
    // a position:fixed element against its nearest positioned ancestor, not
    // the viewport, so a nested 100% layer inside an unsized container
    // collapses to 0x0 (verified: layer_rect w=0,h=0). Use inset (top/right/
    // bottom/left:0) instead of width/height:100% so the container always
    // spans the viewport regardless of ancestor sizing.
    stream << "            try {\n";
    stream << "                container.style.cssText =\n";
    stream << "                    'position:fixed;top:0;left:0;right:0;bottom:0;' +\n";
    stream << "                    'z-index:2147483647;' +\n";
    stream << "                    'pointer-events:' + (spec.pointerEvents ? 'auto' : 'none') + ';';\n";
    stream << "            } catch (e) {}\n";
    stream << "            var parent = document.body || document.documentElement;\n";
    stream << "            if (parent) {\n";
    stream << "                parent.appendChild(container);\n";
    stream << "            } else {\n";
    stream << "                throw new Error('no document.body or documentElement');\n";
    stream << "            }\n";
    stream << "        }\n";
    // Stage 8: clear previous content, then build the Mod's DOM tree with the
    // universal CSSOM renderer (replaces the former 'demo' special case).
    stream << "        container.innerHTML = '';\n";
    stream << "        try {\n";
    stream << "            container.__dearOreUiRoot = dearOreUiBuildDom(container, spec.body);\n";
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
    // M8.1.2 size fix: append nodes to an already-mounted container. Large
    // UIs are injected in chunks (each ExecuteScript stays under cohtml's
    // silent size limit); the first chunk mounts the container, the rest
    // append into it.
    //
    // Stage 8.1.5: the append TARGET is captured at mount time
    // (container.__dearOreUiRoot) instead of container.firstElementChild —
    // that property is unverified on this cohtml build, and if it is missing
    // the appends silently land in the container as siblings UNDER the
    // full-screen panel (invisible). The mount-captured element is exact.
    stream << "    window.__DearOreUI__.ui.appendTo = function(containerId, nodes) {\n";
    stream << "        var container = document.getElementById(containerId);\n";
    stream << "        if (!container) return false;\n";
    // Chunked UIs: the first chunk mounts the container with the root panel;
    // later chunks append into that root so the layout stays a single panel
    // instead of stacked duplicates.
    stream << "        var target = container.__dearOreUiRoot || container.firstElementChild || container;\n";
    stream << "        try { dearOreUiBuildDom(target, nodes); } catch (e) { return false; }\n";
    stream << "        container.__dearOreUiAppended = (container.__dearOreUiAppended || 0) + 1;\n";
    stream << "        return true;\n";
    stream << "    };\n";
    stream << "    window.__DearOreUI__.ui.unmount = function(containerId) {\n";
    stream << "        var container = document.getElementById(containerId);\n";
    stream << "        if (container && container.parentNode) {\n";
    stream << "            container.parentNode.removeChild(container);\n";
    stream << "        }\n";
    stream << "    };\n";
    stream << "    window.__DearOreUI__.ui.report = function(msg) {\n";
    stream << "        try { if (window.__DearOreUI__ && window.__DearOreUI__.ipc) "
              "window.__DearOreUI__.ipc.report(msg); } catch (e) {}\n";
    stream << "    };\n";
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
    stream << "                if (window.console && console.log) console.log('[DearOreUI] mounted ' + "
              "spec.containerId);\n";
    stream << "            } catch (e) {\n";
    stream << "                window.__DearOreUI__.ui.report('mount_error:' + spec.containerId + ':' + (e && "
              "e.message));\n";
    stream << "                if (window.console && console.error) console.error('[DearOreUI] mount failed: ' + (e && "
              "e.message));\n";
    stream << "            }\n";
    stream << "        }\n";
    stream << "    };\n";
    stream << "    function dearOreUiReadyMount() {\n";
    stream << "        if (document.body) {\n";
    stream << "            window.__DearOreUI__.ui.mountAll();\n";
    stream << "            window.__DearOreUI__.ui.mounted = true;\n";
    stream << "            window.__DearOreUI__.ui.report('mount_all_done');\n";
    stream << "            return true;\n";
    stream << "        }\n";
    stream << "        return false;\n";
    stream << "    }\n";
    stream << "    if (!dearOreUiReadyMount()) {\n";
    stream << "        var dearOreUiIntervalId = setInterval(function() {\n";
    stream << "            if (dearOreUiReadyMount()) {\n";
    stream << "                clearInterval(dearOreUiIntervalId);\n";
    stream << "            }\n";
    stream << "        }, 100);\n";
    stream << "        document.addEventListener('DOMContentLoaded', function() {\n";
    stream << "            if (dearOreUiReadyMount()) {\n";
    stream << "                clearInterval(dearOreUiIntervalId);\n";
    stream << "            }\n";
    stream << "        });\n";
    stream << "        setTimeout(function() { clearInterval(dearOreUiIntervalId); }, 10000);\n";
    stream << "    }\n";
    stream << "})();\n";
    return stream.str();
}

// Body-only follow-up script for UIs after the first one. It reuses the
// machinery already injected by the bootstrap script (window.__DearOreUI__.ui),
// so each additional UI stays small — cohtml ExecuteScript silently drops
// large scripts, so the machinery must not be duplicated per UI.
// `bodyOverride` (optional) replaces the spec's body for chunked injection.
std::string RuntimeInjector::generateUiBodyScript(
    api::ContextId                      id,
    ui::UiMountItem const&              item,
    std::vector<render::DomNode> const* bodyOverride
) const {
    static_cast<void>(id);
    std::ostringstream stream;
    stream << "(function(){\n";
    stream << "    var ui = window.__DearOreUI__ && window.__DearOreUI__.ui;\n";
    stream << "    if (!ui || !ui.mount) return;\n";
    stream << "    var spec = ";
    appendUiSpec(stream, item.spec, bodyOverride);
    stream << "    ;\n";
    stream << "    ui.specs.push(spec);\n";
    stream << "    function dearOreUiBodyMount() {\n";
    stream << "        if (!document.body) return false;\n";
    stream << "        try { ui.mount(spec); } catch (e) { return false; }\n";
    stream << "        return !!document.getElementById(spec.containerId);\n";
    stream << "    }\n";
    stream << "    if (!dearOreUiBodyMount()) {\n";
    stream << "        var iv = setInterval(function() {\n";
    stream << "            if (dearOreUiBodyMount()) clearInterval(iv);\n";
    stream << "        }, 100);\n";
    stream << "        setTimeout(function() { clearInterval(iv); }, 10000);\n";
    stream << "    }\n";
    stream << "})();\n";
    return stream.str();
}

// Append-only script for chunked UIs: appends a node forest into an
// already-mounted container (created by the first chunk's mount script).
//
// Stage 8.1.5 cleanup: the numbered marker chips served their purpose (they
// proved all 12 append chunks execute and led to the DomScriptSerializer
// `]b:` fix); they are removed from the shipped scripts. The mount-captured
// append target (container.__dearOreUiRoot) and the invisible
// __dearOreUiAppended counter in appendTo remain.
std::string RuntimeInjector::generateUiAppendScript(
    api::ContextId                      id,
    std::string const&                  containerId,
    std::vector<render::DomNode> const& nodes
) const {
    static_cast<void>(id);
    std::ostringstream stream;
    stream << "(function(){\n";
    stream << "    var ui = window.__DearOreUI__ && window.__DearOreUI__.ui;\n";
    stream << "    if (!ui || !ui.appendTo) return;\n";
    stream << "    var nodes = " << render::serializeDomForest(nodes) << ";\n";
    stream << "    function dearOreUiAppend() {\n";
    stream << "        if (!document.body) return false;\n";
    stream << "        try { return ui.appendTo(\"" << escapeJsString(containerId)
           << "\", nodes); } catch (e) { return false; }\n";
    stream << "    }\n";
    stream << "    if (!dearOreUiAppend()) {\n";
    stream << "        var iv = setInterval(function() {\n";
    stream << "            if (dearOreUiAppend()) clearInterval(iv);\n";
    stream << "        }, 100);\n";
    stream << "        setTimeout(function() { clearInterval(iv); }, 10000);\n";
    stream << "    }\n";
    stream << "})();\n";
    return stream.str();
}

} // namespace dearoreui::inject
