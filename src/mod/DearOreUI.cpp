#include "mod/MyMod.h"

#include "runtime/Runtime.h"

#include "ll/api/mod/RegisterHelper.h"

#include <cstdlib>
#include <filesystem>

namespace dearoreui {

namespace {

[[nodiscard]] std::filesystem::path detectMinecraftDirectory() {
    if (auto const* env = std::getenv("DEAROREUI_MC_PATH"); env != nullptr && *env != '\0') {
        return env;
    }
    // Stage 4 does not rely on an unverified default installation path.
    // The FileSystemSourceReader will report partial=true when the base directory is missing.
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
