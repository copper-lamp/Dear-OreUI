#include "mod/MyMod.h"

#include "diagnostic/Stage0Telemetry.h"
#include "hook/Stage0OreUIHooks.h"

#include "ll/api/mod/RegisterHelper.h"

namespace dearoreui {

DearOreUI& DearOreUI::getInstance() {
    static DearOreUI instance;
    return instance;
}

bool DearOreUI::load() {
    getSelf().getLogger().debug("Loading...");
    diagnostic::startStage0Session();
    diagnostic::recordStage0("lifecycle", "event=load");
    return true;
}

bool DearOreUI::enable() {
    getSelf().getLogger().debug("Enabling...");
    diagnostic::recordStage0("lifecycle", "event=enable");
    return hook::installStage0OreUIHooks();
}

bool DearOreUI::disable() {
    getSelf().getLogger().debug("Disabling...");
    diagnostic::recordStage0("lifecycle", "event=disable");
    auto result = hook::uninstallStage0OreUIHooks();
    diagnostic::resetStage0Session();
    return result;
}

} // namespace dearoreui

LL_REGISTER_MOD(dearoreui::DearOreUI, dearoreui::DearOreUI::getInstance());
