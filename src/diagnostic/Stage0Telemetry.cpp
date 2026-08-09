#include "diagnostic/Stage0Telemetry.h"

#include "mod/MyMod.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>
#include <system_error>

namespace dearoreui::diagnostic {

namespace {

std::mutex& telemetryMutex() {
    static std::mutex value;
    return value;
}

std::string& sessionId() {
    static std::string value;
    return value;
}

bool& writeFailureReported() {
    static bool value{};
    return value;
}

std::filesystem::path telemetryPath() {
    return DearOreUI::getInstance().getSelf().getDataDir() / "telemetry" / "stage0-oreui.txt";
}

std::string encode(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (char character : value) {
        switch (character) {
        case '\\':
            result += "\\\\";
            break;
        case '\t':
            result += "\\t";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        default:
            result += character;
            break;
        }
    }
    return result;
}

std::string encodeFields(std::string_view fields) {
    std::string result;
    std::size_t start = 0;
    while (start <= fields.size()) {
        auto end = fields.find('\t', start);
        auto field = fields.substr(start, end == std::string_view::npos ? fields.size() - start : end - start);
        auto separator = field.find('=');
        if (!result.empty()) result += '\t';
        if (separator == std::string_view::npos) {
            result += encode(field);
        } else {
            result += encode(field.substr(0, separator));
            result += '=';
            result += encode(field.substr(separator + 1));
        }
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    return result;
}

void append(std::string_view line) {
    std::lock_guard lock(telemetryMutex());
    auto const path = telemetryPath();
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
        if (!writeFailureReported()) {
            writeFailureReported() = true;
            DearOreUI::getInstance().getSelf().getLogger().warn("Unable to create Stage 0 telemetry directory");
        }
        return;
    }

    std::ofstream output(path, std::ios::out | std::ios::app | std::ios::binary);
    if (!output.is_open()) {
        if (!writeFailureReported()) {
            writeFailureReported() = true;
            DearOreUI::getInstance().getSelf().getLogger().warn("Unable to open Stage 0 telemetry file");
        }
        return;
    }

    output << line << '\n';
    output.flush();
    if (!output && !writeFailureReported()) {
        writeFailureReported() = true;
        DearOreUI::getInstance().getSelf().getLogger().warn("Unable to write Stage 0 telemetry file");
    }
}

}

void startStage0Session() {
    auto const now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    );
    sessionId() = std::to_string(now.count());
    writeFailureReported() = false;
    recordStage0("session", "id=" + sessionId() + "\tclient=win-x64\tlevilamina=26.10.x");
}

void recordStage0(std::string_view event, std::string_view fields) {
    std::string line(encode(event));
    if (!sessionId().empty()) line += "\tsession_id=" + encode(sessionId());
    if (!fields.empty()) {
        line += '\t';
        line += encodeFields(fields);
    }
    append(line);
}

void resetStage0Session() {
    sessionId().clear();
}

}
