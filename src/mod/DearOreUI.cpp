#include "mod/MyMod.h"

#include "runtime/Runtime.h"

#include "ll/api/mod/RegisterHelper.h"

namespace dearoreui {

DearOreUI& DearOreUI::getInstance() {
    static DearOreUI instance;
    return instance;
}

bool DearOreUI::load() {
    getSelf().getLogger().debug("Loading...");

    runtime::RuntimeConfig config;
    config.dataDirectory = getSelf().getDataDir();
    mRuntime             = std::make_unique<runtime::Runtime>(std::move(config));

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
