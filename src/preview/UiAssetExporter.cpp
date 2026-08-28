#include "preview/UiAssetExporter.h"

#include "render/DomScriptSerializer.h"

#include <cctype>
#include <cstdio>
#include <exception>
#include <fstream>

namespace dearoreui::preview {

namespace {

[[nodiscard]] std::string_view pageScopeName(api::PageScope scope) {
    switch (scope) {
    case api::PageScope::Any:
        return "any";
    case api::PageScope::MainMenu:
        return "main_menu";
    case api::PageScope::PlayScreen:
        return "play_screen";
    case api::PageScope::Settings:
        return "settings";
    case api::PageScope::Pause:
        return "pause";
    case api::PageScope::InGame:
        return "in_game";
    case api::PageScope::Custom:
        return "custom";
    }
    return "unknown";
}

[[nodiscard]] std::string_view anchorName(api::UiAnchor anchor) {
    switch (anchor) {
    case api::UiAnchor::TopLeft:
        return "top_left";
    case api::UiAnchor::TopCenter:
        return "top_center";
    case api::UiAnchor::TopRight:
        return "top_right";
    case api::UiAnchor::CenterLeft:
        return "center_left";
    case api::UiAnchor::Center:
        return "center";
    case api::UiAnchor::CenterRight:
        return "center_right";
    case api::UiAnchor::BottomLeft:
        return "bottom_left";
    case api::UiAnchor::BottomCenter:
        return "bottom_center";
    case api::UiAnchor::BottomRight:
        return "bottom_right";
    case api::UiAnchor::FullScreen:
        return "full_screen";
    }
    return "unknown";
}

// Escapes a string for inclusion inside a JSON string literal.
[[nodiscard]] std::string jsonEscape(std::string_view value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (char c : value) {
        switch (c) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\b':
            out += "\\b";
            break;
        case '\f':
            out += "\\f";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(c));
                out += buf;
            } else {
                out += c;
            }
            break;
        }
    }
    return out;
}

// Concatenates the text of every <script> node in the forest (recursive) —
// these carry the page script (e.g. the calendar logic) that the preview must
// inject after the shim/runtime.
void collectPageScript(api::DomNode const& node, std::string& out) {
    std::string tag = node.tag;
    for (auto& c : tag) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (tag == "script") {
        out += node.text;
        return;
    }
    for (auto const& child : node.children) {
        collectPageScript(child, out);
    }
}

} // namespace

UiAssetExporter::UiAssetExporter(diagnostic::DiagnosticLogger& logger) : mLogger(logger) {}

UiPreviewAsset UiAssetExporter::toAsset(registry::UiEntry const& entry) {
    UiPreviewAsset asset;
    asset.entry        = entry.owner.value() + "." + entry.manifest.id;
    asset.title        = entry.manifest.id;
    asset.kind         = std::string(api::uiKindName(entry.manifest.kind));
    asset.fingerprint  = entry.manifest.fingerprint;
    asset.containerId  = entry.manifest.containerId;
    asset.htmlBody     = entry.htmlBody;
    asset.domScript    = render::serializeDomForest(entry.domNodes);
    asset.anchor       = std::string(anchorName(entry.manifest.anchor));
    for (auto scope : entry.manifest.pageScopes) {
        asset.pageScopes.push_back(std::string(pageScopeName(scope)));
    }
    for (auto const& node : entry.domNodes) {
        collectPageScript(node, asset.pageScript);
    }
    return asset;
}

bool UiAssetExporter::exportUiEntries(
    std::vector<registry::UiEntry> const& entries,
    std::filesystem::path const&          outDir
) {
    try {
        std::error_code ec;
        std::filesystem::create_directories(outDir, ec);
        if (ec) {
            mLogger.warning("preview", "export_mkdir_failed").withMessage(ec.message()).emit();
            return false;
        }

        std::string json;
        json += "[\n";
        bool first = true;
        for (auto const& entry : entries) {
            auto asset = toAsset(entry);
            if (!first) json += ",\n";
            first = false;
            json += "  {\n";
            json += "    \"entry\": \"" + jsonEscape(asset.entry) + "\",\n";
            json += "    \"title\": \"" + jsonEscape(asset.title) + "\",\n";
            json += "    \"kind\": \"" + jsonEscape(asset.kind) + "\",\n";
            json += "    \"anchor\": \"" + jsonEscape(asset.anchor) + "\",\n";

            json += "    \"pageScopes\": [";
            bool sf = true;
            for (auto const& s : asset.pageScopes) {
                if (!sf) json += ", ";
                sf = false;
                json += "\"" + jsonEscape(s) + "\"";
            }
            json += "],\n";

            json += "    \"containerId\": \"" + jsonEscape(asset.containerId) + "\",\n";
            json += "    \"fingerprint\": \"" + jsonEscape(asset.fingerprint) + "\",\n";
            json += "    \"htmlBody\": \"" + jsonEscape(asset.htmlBody) + "\",\n";
            json += "    \"domScript\": \"" + jsonEscape(asset.domScript) + "\",\n";
            json += "    \"pageScript\": \"" + jsonEscape(asset.pageScript) + "\"\n";
            json += "  }";
        }
        json += "\n]\n";

        auto manifestPath = outDir / "uiAssets.json";
        std::ofstream out(manifestPath);
        if (!out) {
            mLogger.warning("preview", "export_open_failed").withField("path", manifestPath.string()).emit();
            return false;
        }
        out << json;
        out.close();

        mLogger.info("preview", "export_written")
            .withField("path", manifestPath.string())
            .withField("entries", std::to_string(entries.size()))
            .withField("bytes", std::to_string(json.size()))
            .emit();
        return true;
    } catch (std::exception const& ex) {
        mLogger.warning("preview", "export_exception").withMessage(ex.what()).emit();
        return false;
    }
}

} // namespace dearoreui::preview