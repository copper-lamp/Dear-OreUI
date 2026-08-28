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

// D1: JS 侧将页面异常/console 输出按前缀桥回，这里按前缀路由日志级别。
// 批量报告以 '\n' 分隔（每行自带前缀），逐行路由；未知前缀保持 info，
// 兼容既有 dbg:/mounted:/runtime_executed: 等遥测。
void recordStage7JsReport(std::string_view report) {
    if (report.empty()) {
        return;
    }
    auto& logger = globalLogger();
    std::size_t pos = 0;
    while (pos <= report.size()) {
        auto const end  = report.find('\n', pos);
        auto const line = report.substr(pos, end == std::string_view::npos ? std::string_view::npos : end - pos);
        auto const text = std::string{line};
        if (text.rfind("js_error:", 0) == 0 || text.rfind("js_console:error:", 0) == 0) {
            logger.error("js", "report").withField("report", text).emit();
        } else if (text.rfind("js_console:warn:", 0) == 0) {
            logger.warning("js", "report").withField("report", text).emit();
        } else if (text.rfind("js_console:debug:", 0) == 0) {
            logger.debug("js", "report").withField("report", text).emit();
        } else {
            logger.info("js", "report").withField("report", text).emit();
        }
        if (end == std::string_view::npos) {
            break;
        }
        pos = end + 1;
    }
}

} // namespace dearoreui::diagnostic
