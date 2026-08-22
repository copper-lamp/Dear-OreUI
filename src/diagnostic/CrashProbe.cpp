#include "diagnostic/CrashProbe.h"

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <system_error>

namespace dearoreui::diagnostic {

namespace {

std::atomic<LONG> gProbeBusy{0};
char              gCrashPath[MAX_PATH]{};
// Writes a single formatted line to the crash file. Uses only raw Win32
// (no std::ofstream, no locks) because the handler may run on a thread whose
// CRT state is already corrupted.
void rawWriteCrashLine(char const* line) {
    if (gCrashPath[0] == '\0') return;
    HANDLE file =
        CreateFileA(gCrashPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    SetFilePointer(file, 0, nullptr, FILE_END);
    DWORD written = 0;
    WriteFile(file, line, static_cast<DWORD>(strlen(line)), &written, nullptr);
    CloseHandle(file);
}

void rawAppendHex(char* buf, std::uint64_t value) {
    static constexpr char kHex[] = "0123456789abcdef";
    char                  tmp[20];
    int                   n = 0;
    do {
        tmp[n++]   = kHex[value & 0xF];
        value    >>= 4;
    } while (value != 0);
    for (int i = 0; i < n; ++i) buf[i] = tmp[n - 1 - i];
    buf[n] = '\0';
}

// Writes one module+RVA line per stack frame (up to 64 frames). Uses
// CaptureStackBackTrace which is safe inside a VEH handler. 24 frames was too
// short: the msxml6 XML-parse crash had its caller (game code) truncated.
void writeBacktrace() {
    void*  frames[64]{};
    USHORT count = CaptureStackBackTrace(0, 64, frames, nullptr);
    if (count == 0) return;
    for (USHORT i = 0; i < count; ++i) {
        void* address = frames[i];
        if (address == nullptr) continue;
        HMODULE module = nullptr;
        if (!GetModuleHandleExA(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                static_cast<char const*>(address),
                &module
            )
            || module == nullptr) {
            char unknown[128];
            int  n = std::snprintf(unknown, sizeof(unknown), "\n  #%u 0x%p", i, address);
            if (n > 0 && n < static_cast<int>(sizeof(unknown))) rawWriteCrashLine(unknown);
            continue;
        }
        char modName[MAX_PATH]{};
        GetModuleFileNameA(module, modName, MAX_PATH);
        std::uintptr_t base = reinterpret_cast<std::uintptr_t>(module);
        std::uintptr_t rva  = reinterpret_cast<std::uintptr_t>(address) - base;
        char           detail[512];
        int            n = std::snprintf(
            detail,
            sizeof(detail),
            "\n  #%u %s+0x%llx",
            i,
            modName[0] != '\0' ? modName : "(unknown module)",
            static_cast<unsigned long long>(rva)
        );
        if (n > 0 && n < static_cast<int>(sizeof(detail))) rawWriteCrashLine(detail);
    }
}

// Finds the module containing addr and writes "<code> at <module>+<rva>".
void describeCrash(DWORD code, void* address) {
    char line[512];
    int  len = std::snprintf(
        line,
        sizeof(line),
        "\n[crash] code=0x%08x address=0x%p thread=0x%08x",
        code,
        address,
        GetCurrentThreadId()
    );
    if (len < 0 || len >= static_cast<int>(sizeof(line))) return;
    rawWriteCrashLine(line);

    HMODULE module = nullptr;
    if (GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            static_cast<char const*>(address),
            &module
        )
        && module != nullptr) {
        char           modName[MAX_PATH]{};
        DWORD          modLen = GetModuleFileNameA(module, modName, MAX_PATH);
        std::uintptr_t base   = reinterpret_cast<std::uintptr_t>(module);
        std::uintptr_t rva    = reinterpret_cast<std::uintptr_t>(address) - base;
        char           detail[512];
        int            n = std::snprintf(
            detail,
            sizeof(detail),
            " in %s+0x%llx",
            modLen > 0 ? modName : "(unknown module)",
            static_cast<unsigned long long>(rva)
        );
        if (n > 0 && n < static_cast<int>(sizeof(detail))) {
            rawWriteCrashLine(detail);
        }
    }
    rawWriteCrashLine("\n[stack]");
    writeBacktrace();
}

LONG WINAPI crashProbeHandler(EXCEPTION_POINTERS* pointers) {
    // Only log once per process to keep the probe tiny and re-entrancy safe.
    // The handler never swallows exceptions — it only records, so the game's
    // own crash handling is untouched (swallowing STATUS_ORIGINATE_ERROR was
    // tried in Stage 8 and did not help; the error path re-raises).
    if (gProbeBusy.exchange(1) != 0) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (pointers != nullptr && pointers->ExceptionRecord != nullptr) {
        describeCrash(pointers->ExceptionRecord->ExceptionCode, pointers->ExceptionRecord->ExceptionAddress);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

} // namespace

void installCrashProbe(std::filesystem::path const& dataDirectory) {
    auto            crashDir = dataDirectory / "crash";
    std::error_code ec;
    std::filesystem::create_directories(crashDir, ec);
    auto filePath = (crashDir / "crash-last.txt").string();
    if (filePath.size() >= MAX_PATH) return;
    std::strncpy(gCrashPath, filePath.c_str(), MAX_PATH - 1);
    gCrashPath[MAX_PATH - 1] = '\0';

    // Write a session marker so we can tell whether the probe was active.
    rawWriteCrashLine("[crash] probe installed");
    char marker[128];
    int  markerLen = std::snprintf(marker, sizeof(marker), "\n[crash] main_thread=0x%08x", GetCurrentThreadId());
    if (markerLen > 0 && markerLen < static_cast<int>(sizeof(marker))) {
        rawWriteCrashLine(marker);
    }
    AddVectoredExceptionHandler(1, crashProbeHandler);
}

void uninstallCrashProbe() {
    RemoveVectoredExceptionHandler(crashProbeHandler);
    gCrashPath[0] = '\0';
}

} // namespace dearoreui::diagnostic
