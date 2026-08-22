#include "diagnostic/Stage7UiTelemetry.h"

#include "diagnostic/DiagnosticLogger.h"

namespace dearoreui::diagnostic {

namespace {

[[nodiscard]] std::string scopeName(api::PageScope scope) {
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

} // namespace

void recordStage7UiRegistered(
    api::ModId       modId,
    std::string_view modNamespace,
    std::string_view uiId,
    api::UiKind      kind
) {
    auto& logger = globalLogger();
    logger.info("ui", "registered")
        .withMod(modId)
        .withField("namespace", std::string(modNamespace))
        .withField("ui_id", std::string(uiId))
        .withField("kind", std::string(api::uiKindName(kind)))
        .emit();
}

void recordStage7UiUnregistered(api::RegistrationHandle handle, std::string_view modNamespace, std::string_view uiId) {
    auto& logger = globalLogger();
    logger.info("ui", "unregistered")
        .withField("handle", std::to_string(handle.value()))
        .withField("namespace", std::string(modNamespace))
        .withField("ui_id", std::string(uiId))
        .emit();
}

void recordStage7UiPlanned(
    api::ContextId contextId,
    api::PageScope scope,
    std::size_t    mounted,
    std::size_t    skipped,
    std::size_t    blocked
) {
    auto& logger = globalLogger();
    logger.info("ui", "planned")
        .withContext(contextId)
        .withField("scope", scopeName(scope))
        .withField("mounted", std::to_string(mounted))
        .withField("skipped", std::to_string(skipped))
        .withField("blocked", std::to_string(blocked))
        .emit();
}

void recordStage7UiMounted(
    api::ContextId   contextId,
    std::string_view modNamespace,
    std::string_view uiId,
    std::string_view containerId
) {
    auto& logger = globalLogger();
    logger.info("ui", "mounted")
        .withContext(contextId)
        .withField("namespace", std::string(modNamespace))
        .withField("ui_id", std::string(uiId))
        .withField("container_id", std::string(containerId))
        .emit();
}

void recordStage7UiFailed(
    api::ContextId   contextId,
    std::string_view modNamespace,
    std::string_view uiId,
    std::string_view reason
) {
    auto& logger = globalLogger();
    logger.warning("ui", "failed")
        .withContext(contextId)
        .withField("namespace", std::string(modNamespace))
        .withField("ui_id", std::string(uiId))
        .withField("reason", std::string(reason))
        .emit();
}

void recordStage7UiUnmounted(api::ContextId contextId, std::string_view modNamespace, std::string_view uiId) {
    globalLogger()
        .info("ui", "unmounted")
        .withContext(contextId)
        .withField("namespace", std::string{modNamespace})
        .withField("ui_id", std::string{uiId})
        .emit();
}

void recordStage7JsReport(std::string_view report) {
    globalLogger().info("js", "report").withField("report", std::string{report}).emit();
}

} // namespace dearoreui::diagnostic
