#include "mod/MyMod.h"

#include "ll/api/mod/RegisterHelper.h"

namespace dearoreui {

DearOreUI& DearOreUI::getInstance() {
    static DearOreUI instance;
    return instance;
}

bool DearOreUI::load() {
    getSelf().getLogger().debug("Loading...");
    return true;
}

bool DearOreUI::enable() {
    getSelf().getLogger().debug("Enabling...");
    return true;
}

bool DearOreUI::disable() {
    getSelf().getLogger().debug("Disabling...");
    return true;
}

} // namespace dearoreui

LL_REGISTER_MOD(dearoreui::DearOreUI, dearoreui::DearOreUI::getInstance());