#include "inject/RuntimeInjector.h"

#include "api/types/Error.h"
#include "asset/BuiltinAssets.h"
#include "diagnostic/Stage5IpcTelemetry.h"
#include "diagnostic/Stage7UiTelemetry.h"
#include "render/DomScriptSerializer.h"
#include "render/HtmlDomParser.h"
#include "resource/ResourceUri.h"
#include "ui/UiManifest.h"

#include <sstream>

namespace dearoreui::inject {

namespace {

// cohtml silently drops large ExecuteScript payloads (M8.1.2 verified: a ~47KB
// combined bootstrap never executed, a ~9KB one did). Child groups larger than
// this are split into separate append scripts.
inline constexpr std::size_t kMaxChunkNodes = 4;

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

// Replaces every occurrence of `from` in `s`. Used to splice the runtime
// values (contextId / bridge availability) into the static asset machinery.
[[nodiscard]] std::string replaceAll(std::string s, std::string_view from, std::string_view to) {
    if (from.empty()) return s;
    std::size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
    return s;
}

} // namespace

RuntimeInjector::RuntimeInjector(diagnostic::DiagnosticLogger& logger, ipc::IHostBridge& bridge)
: mLogger(logger),
  mBridge(bridge) {}

std::string RuntimeInjector::generateRuntimeScriptForTest(api::ContextId id, resource::IResourceIndex const& index) const {
    return generateRuntimeScript(id, index);
}

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

    // M1 资产化: the stage-8 runtime machinery is a single canonical asset
    // (assets/stage5-runtime.js). Only the per-page contextId and the bridge
    // availability (affects the diagnostic log line) are dynamic; splice them
    // into the asset. Behavior is byte-identical to the former inline string.
    std::string script(asset::stage5RuntimeJs());
    script = replaceAll(std::move(script), "__DEAROREUI_CTX__", std::to_string(id.value()));
    script = replaceAll(std::move(script), "__DEAROREUI_BRIDGE__", mBridge.isAvailable() ? "true" : "false");
    return script;
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
    std::vector<api::DomNode> const* bodyOverride = nullptr
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
std::vector<std::vector<api::DomNode>>
chunkUiBody(std::vector<api::DomNode> const& body, std::size_t maxChunkNodes) {
    std::vector<std::vector<api::DomNode>> chunks;
    if (body.size() == 1 && body[0].children.size() > maxChunkNodes) {
        auto const& root = body[0];
        for (std::size_t i = 0; i < root.children.size(); i += maxChunkNodes) {
            std::vector<api::DomNode> group;
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

// Collects page <script> node text (depth-first, any depth) and REMOVES the
// nodes from the forest: page scripts never become DOM <script> elements
// (the engine does not execute them) and never run inside the builder. The
// text is executed through the native ExecuteScript channel after the UI's
// container is mounted (see generateUiBodyScript), the same verified channel
// the bootstrap machinery uses.
std::string extractScriptNodes(std::vector<api::DomNode>& nodes) {
    std::string out;
    for (auto it = nodes.begin(); it != nodes.end();) {
        if (it->tag == "script") {
            out += it->text;
            it = nodes.erase(it);
        } else {
            out += extractScriptNodes(it->children);
            ++it;
        }
    }
    return out;
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
    // unified renderer (renderUiScripts) emits a mount script for the root +
    // first child group, then one append script per remaining group — each
    // ExecuteScript stays under cohtml's silent size limit. This loop only
    // submits the ordered script stream.
    for (auto const& item : plan.items) {
        if (item.decision != ui::UiMountDecision::Mount) {
            continue;
        }
        auto scripts = renderUiScripts(id, item);
        for (std::size_t chunkIndex = 0; chunkIndex < scripts.size(); ++chunkIndex) {
            auto const& script = scripts[chunkIndex];
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

    // M1 资产化: the UI-machinery is the canonical asset (assets/stage7-ui-bootstrap.js).
    // Only the contextId echoed into __DearOreUI__.ui.contextId is dynamic;
    // splice it into the asset. Behavior is byte-identical to the former inline
    // machinery script.
    std::string script(asset::stage7UiBootstrapJs());
    return replaceAll(std::move(script), "__DEAROREUI_CTX__", std::to_string(id.value()));
}

// Body-only follow-up script for UIs after the first one. It reuses the
// machinery already injected by the bootstrap script (window.__DearOreUI__.ui),
// so each additional UI stays small — cohtml ExecuteScript silently drops
// large scripts, so the machinery must not be duplicated per UI.
// `bodyOverride` (optional) replaces the spec's body for chunked injection.
// `pageScripts` (optional) is the UI's <script> node text, executed through
// this same ExecuteScript channel once the container is mounted.
std::string RuntimeInjector::generateUiBodyScript(
    api::ContextId                      id,
    ui::UiMountItem const&              item,
    std::vector<api::DomNode> const* bodyOverride,
    std::string const&                  pageScripts
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
    stream << "        var container = document.getElementById(spec.containerId);\n";
    stream << "        if (!container) return false;\n";
    // Page scripts run once per container, right after mount. Their text was
    // extracted by the C++ injector; running it here keeps it on the native
    // ExecuteScript channel (same one the bootstrap uses), avoids the DOM
    // <script> elements the engine ignores, and the crash from eval.
    if (!pageScripts.empty()) {
        stream << "        if (!container.__dearOreUiPageScriptsRun) {\n";
        stream << "            container.__dearOreUiPageScriptsRun = true;\n";
        stream << "            try {\n";
        stream << pageScripts;
        stream << "\n            } catch (e) {\n";
        stream << "                try { ui.report('page_script_err:' + (e && e.message)); } catch (e2) {}\n";
        stream << "            }\n";
        stream << "        }\n";
    }
    stream << "        return true;\n";
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
    std::vector<api::DomNode> const& nodes
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

// Unified UI renderer (T2): the single place that answers "how is ONE UI
// injected?". It resolves the body (domNodes preferred, htmlBody parsed
// otherwise), extracts the page <script> text, splits the body into chunks
// under kMaxChunkNodes, then emits the ordered script sequence:
//   [0]      mount script (container root + first child group + pageScripts),
//   [1..]    append scripts (remaining child groups appended into the root).
// injectUi only submits the returned stream; it makes no chunking choices.
std::vector<std::string> RuntimeInjector::renderUiScripts(
    api::ContextId         id,
    ui::UiMountItem const& item
) const {
    auto body = item.spec.domNodes.empty() ? render::parseHtmlFragment(item.spec.htmlBody) : item.spec.domNodes;
    // Page scripts are pulled out of the mounted DOM and re-attached to the
    // first (mount) script, which runs them after the container exists.
    std::string pageScripts = extractScriptNodes(body);
    auto        chunks      = chunkUiBody(body, kMaxChunkNodes);

    std::vector<std::string> scripts;
    scripts.reserve(chunks.size());
    for (std::size_t chunkIndex = 0; chunkIndex < chunks.size(); ++chunkIndex) {
        if (chunkIndex == 0) {
            // First chunk: mount the root with the first group of children.
            std::vector<api::DomNode> mountBody;
            if (body.size() == 1 && chunks.size() > 1) {
                api::DomNode root = body[0];
                root.children        = chunks[0];
                mountBody.push_back(std::move(root));
            } else {
                mountBody = chunks[0];
            }
            scripts.push_back(generateUiBodyScript(id, item, &mountBody, pageScripts));
        } else {
            // Later chunks: append the remaining children into the root.
            scripts.push_back(generateUiAppendScript(id, item.spec.containerId, chunks[chunkIndex]));
        }
    }
    return scripts;
}

} // namespace dearoreui::inject
