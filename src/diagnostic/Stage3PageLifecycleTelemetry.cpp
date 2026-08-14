#include "diagnostic/Stage3PageLifecycleTelemetry.h"

#include "diagnostic/DiagnosticLogger.h"
#include "diagnostic/Stage0FileSink.h"

#include <memory>
#include <string>

namespace dearoreui::diagnostic {

namespace {

std::shared_ptr<Stage0FileSink>& stage3Sink() {
    static std::shared_ptr<Stage0FileSink> value;
    return value;
}

std::string scopeName(api::PageScope scope) {
    switch (scope) {
    case api::PageScope::Any:
        return "Any";
    case api::PageScope::MainMenu:
        return "MainMenu";
    case api::PageScope::PlayScreen:
        return "PlayScreen";
    case api::PageScope::Settings:
        return "Settings";
    case api::PageScope::Pause:
        return "Pause";
    case api::PageScope::InGame:
        return "InGame";
    case api::PageScope::Custom:
        return "Custom";
    }
    return "Unknown";
}

} // namespace

void initializeStage3FileSink(const std::filesystem::path& dataDirectory, std::string const& sessionId) {
    auto& logger = globalLogger();
    auto& sink   = stage3Sink();
    if (!sink) {
        sink = std::make_shared<Stage0FileSink>(
            dataDirectory / "telemetry" / "stage3-page-lifecycle.txt",
            sessionId
        );
        logger.addSink(sink);
    }
}

void recordStage3PageCreated(api::ContextId id, api::PageInfo const& info, std::string_view url) {
    auto& logger = globalLogger();
    logger.info("stage3", "page_created")
        .withContext(id)
        .withPage(info.id)
        .withField("scope", scopeName(info.scope))
        .withField("url", std::string(url))
        .emit();
}

void recordStage3PageDestroyed(api::ContextId id, api::PageInfo const& info) {
    auto& logger = globalLogger();
    logger.info("stage3", "page_destroyed")
        .withContext(id)
        .withPage(info.id)
        .withField("scope", scopeName(info.scope))
        .emit();
}

} // namespace dearoreui::diagnostic
