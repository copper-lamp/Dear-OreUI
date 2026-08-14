#include "diagnostic/DirectFileLog.h"

#include <fstream>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#endif

namespace dearoreui::diagnostic {

namespace {

std::mutex& directLogMutex() {
    static std::mutex value;
    return value;
}

void createParentDirectories(std::filesystem::path const& path) {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
}

} // namespace

void appendLineToFile(std::filesystem::path const& path, std::string_view line) {
    std::lock_guard lock(directLogMutex());
    createParentDirectories(path);

    auto const content = std::string(line) + '\n';

#ifdef _WIN32
    auto const pathWide = path.wstring();
    HANDLE file = CreateFileW(
        pathWide.c_str(),
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    if (file != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER distance{};
        distance.QuadPart = 0;
        SetFilePointerEx(file, distance, nullptr, FILE_END);

        DWORD written = 0;
        WriteFile(file, content.data(), static_cast<DWORD>(content.size()), &written, nullptr);
        CloseHandle(file);
        return;
    }
#endif

    std::ofstream output(path, std::ios::out | std::ios::app | std::ios::binary);
    if (output.is_open()) {
        output << content;
    }
}

} // namespace dearoreui::diagnostic
