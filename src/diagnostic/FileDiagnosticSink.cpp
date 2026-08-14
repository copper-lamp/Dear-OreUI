#include "diagnostic/FileDiagnosticSink.h"

#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace dearoreui::diagnostic {

namespace {

std::string escapeJson(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (char character : value) {
        switch (character) {
        case '"':
            result += "\\\"";
            break;
        case '\\':
            result += "\\\\";
            break;
        case '\b':
            result += "\\b";
            break;
        case '\f':
            result += "\\f";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            if (static_cast<unsigned char>(character) < 0x20) {
                std::ostringstream output;
                output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                       << static_cast<int>(static_cast<unsigned char>(character));
                result += output.str();
            } else {
                result += character;
            }
            break;
        }
    }
    return result;
}

std::string formatTimestamp(std::chrono::system_clock::time_point const& timestamp) {
    auto const time     = std::chrono::system_clock::to_time_t(timestamp);
    auto const millis   = std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()).count() % 1000;
    std::tm localTime{};
    localtime_s(&localTime, &time);
    std::ostringstream output;
    output << std::put_time(&localTime, "%Y-%m-%dT%H:%M:%S") << '.' << std::setw(3) << std::setfill('0') << millis
           << std::put_time(&localTime, "%z");
    return output.str();
}

} // namespace

FileDiagnosticSink::FileDiagnosticSink(std::filesystem::path path) : mPath(std::move(path)) {
    std::error_code error;
    std::filesystem::create_directories(mPath.parent_path(), error);
}

void FileDiagnosticSink::consume(DiagnosticEvent const& event) {
    std::ostringstream output;
    output << "{"
           << "\"id\":" << event.id.value() << ","
           << "\"timestamp\":\"" << escapeJson(formatTimestamp(event.timestamp)) << "\","
           << "\"severity\":\"" << escapeJson(severityName(event.severity)) << "\","
           << "\"category\":\"" << escapeJson(event.category) << "\","
           << "\"event\":\"" << escapeJson(event.event) << "\"";

    if (event.contextId) output << ",\"context_id\":" << event.contextId->value();
    if (event.modId) output << ",\"mod_id\":\"" << escapeJson(event.modId->value()) << "\"";
    if (event.pageId) output << ",\"page_id\":\"" << escapeJson(event.pageId->value()) << "\"";
    if (event.errorCode) output << ",\"error_code\":" << static_cast<int>(*event.errorCode);

    if (!event.fields.empty()) {
        output << ",\"fields\":{";
        bool first = true;
        for (auto const& field : event.fields) {
            if (!first) output << ",";
            first = false;
            output << "\"" << escapeJson(field.key) << "\":\"" << escapeJson(field.value) << "\"";
        }
        output << "}";
    }

    if (!event.message.empty()) {
        output << ",\"message\":\"" << escapeJson(event.message) << "\"";
    }

    output << "}";
    auto const line = output.str() + '\n';

    std::lock_guard lock(mMutex);

#ifdef _WIN32
    auto const pathWide = mPath.wstring();
    HANDLE file = CreateFileW(
        pathWide.c_str(),
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    if (file == INVALID_HANDLE_VALUE) {
        // Fallback: try the standard stream path if the Windows API path fails.
        if (!mOutput.is_open()) {
            mOutput.open(mPath, std::ios::out | std::ios::app | std::ios::binary);
        }
        if (mOutput.is_open()) {
            mOutput << line;
        }
        return;
    }

    // Ensure new content is appended at the end of the file.
    LARGE_INTEGER distance{};
    distance.QuadPart = 0;
    SetFilePointerEx(file, distance, nullptr, FILE_END);

    DWORD written = 0;
    WriteFile(file, line.data(), static_cast<DWORD>(line.size()), &written, nullptr);
    CloseHandle(file);
#else
    if (!mOutput.is_open()) {
        mOutput.open(mPath, std::ios::out | std::ios::app | std::ios::binary);
    }
    if (mOutput.is_open()) {
        mOutput << line;
    }
#endif
}

void FileDiagnosticSink::flush() {
    std::lock_guard lock(mMutex);
#ifndef _WIN32
    if (mOutput.is_open()) mOutput.flush();
#endif
}

} // namespace dearoreui::diagnostic
