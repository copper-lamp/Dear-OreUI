#include "mod/MyMod.h"

#include "runtime/Runtime.h"

#include "ll/api/mod/RegisterHelper.h"

#include <cstdlib>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#endif

namespace dearoreui {

namespace {

[[nodiscard]] std::filesystem::path getCurrentModuleDirectory() {
#ifdef _WIN32
    wchar_t buffer[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, buffer, MAX_PATH) == 0) {
        return {};
    }
    return std::filesystem::path(buffer).parent_path();
#else
    return {};
#endif
}

[[nodiscard]] bool hasHbuiRoot(std::filesystem::path const& root) {
    std::error_code code;
    return std::filesystem::is_regular_file(root / "data" / "gui" / "dist" / "hbui" / "index.html", code);
}

[[nodiscard]] std::filesystem::path detectMinecraftDirectory() {
    if (auto const* env = std::getenv("DEAROREUI_MC_PATH"); env != nullptr && *env != '\0') {
        return env;
    }

    // Try to infer from the mod DLL location: <instance>/mods/DearOreUI.dll
    // The game root is typically <instance>, containing data/gui/dist/hbui.
    auto moduleDir = getCurrentModuleDirectory();
    for (int i = 0; i < 4 && !moduleDir.empty(); ++i) {
        if (hasHbuiRoot(moduleDir)) {
            return moduleDir;
        }
        auto parent = moduleDir.parent_path();
        if (parent == moduleDir) {
            break;
        }
        moduleDir = std::move(parent);
    }

    return {};
}

} // namespace

DearOreUI& DearOreUI::getInstance() {
    static DearOreUI instance;
    return instance;
}

bool DearOreUI::load() {
    getSelf().getLogger().debug("Loading...");

    runtime::RuntimeConfig config;
    config.dataDirectory      = getSelf().getDataDir();
    config.minecraftDirectory = detectMinecraftDirectory();
    mRuntime                  = std::make_unique<runtime::Runtime>(std::move(config));

    return mRuntime->initialize();
}

bool DearOreUI::enable() {
    getSelf().getLogger().debug("Enabling...");
    return mRuntime && mRuntime->enable();
}

bool DearOreUI::disable() {
    getSelf().getLogger().debug("Disabling...");
    return mRuntime && mRuntime->disable();
}

} // namespace dearoreui

LL_REGISTER_MOD(dearoreui::DearOreUI, dearoreui::DearOreUI::getInstance());
