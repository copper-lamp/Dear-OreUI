#include "diagnostic/Stage0TelemetryCompat.h"

#include "diagnostic/DiagnosticLogger.h"
#include "diagnostic/Stage0FileSink.h"

#include <chrono>
#include <memory>
#include <string>

namespace dearoreui::diagnostic {

namespace {

std::string& sessionId() {
    static std::string value;
    return value;
}

std::shared_ptr<Stage0FileSink>& stage0Sink() {
    static std::shared_ptr<Stage0FileSink> value;
    return value;
}

std::string generateSessionId() {
    auto const now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    );
    return std::to_string(now.count());
}

struct ParsedFields {
    std::string eventName = "record";
    std::vector<DiagnosticField> fields;
};

ParsedFields parseFields(std::string_view fields) {
    ParsedFields result;
    std::size_t start = 0;
    while (start <= fields.size()) {
        auto end   = fields.find('\t', start);
        auto field = fields.substr(start, end == std::string_view::npos ? fields.size() - start : end - start);
        auto separator = field.find('=');
        if (separator == std::string_view::npos) {
            result.fields.emplace_back(std::string{field}, "");
        } else {
            auto key   = std::string{field.substr(0, separator)};
            auto value = std::string{field.substr(separator + 1)};
            if (key == "event") {
                result.eventName = value;
            } else {
                result.fields.emplace_back(std::move(key), std::move(value));
            }
        }
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    return result;
}

} // namespace

void initializeStage0FileSink(std::filesystem::path dataDirectory) {
    auto& logger = globalLogger();
    auto& sink   = stage0Sink();
    if (!sink) {
        sink = std::make_shared<Stage0FileSink>(
            std::move(dataDirectory) / "telemetry" / "stage0-oreui.txt",
            sessionId()
        );
        logger.addSink(sink);
    }
}

void startStage0Session() {
    sessionId() = generateSessionId();
    if (auto const& sink = stage0Sink()) {
        sink->setSessionId(sessionId());
    }
    recordStage0("session", "id=" + sessionId() + "\tclient=win-x64\tlevilamina=26.10.x");
}

void recordStage0(std::string_view event, std::string_view fields) {
    auto& logger = globalLogger();
    auto parsed  = parseFields(fields);
    auto builder = logger.info(std::string{event}, parsed.eventName);
    for (auto const& field : parsed.fields) {
        builder.withField(field.key, field.value);
    }
    builder.emit();
}

void resetStage0Session() {
    auto& logger = globalLogger();
    logger.info("session", "reset").emit();
    sessionId().clear();
}

} // namespace dearoreui::diagnostic
