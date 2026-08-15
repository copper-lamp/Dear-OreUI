#pragma once

#include <string>
#include <filesystem>

namespace dearoreui::diagnostic {

// Installs a vectored exception handler that records the faulting address
// (module + RVA) to <dataDirectory>/crash/crash-last.txt on ANY unhandled
// exception in the process (including cohtml engine threads). The handler
// does not swallow the exception — it only logs, so the game's own crash
// handling is untouched.
//
// Use: when the client still crashes despite the diagnostics stream showing
// nothing, this file tells us exactly WHICH function crashed, which is the
// only reliable way to stop guessing at lifecycle/hook interactions.
void installCrashProbe(std::filesystem::path const& dataDirectory);

// Removes the handler (call on mod disable).
void uninstallCrashProbe();

} // namespace dearoreui::diagnostic
