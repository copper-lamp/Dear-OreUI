#pragma once

#include <filesystem>
#include <string_view>

namespace dearoreui::diagnostic {

// Appends a line to a file without using the global logger or diagnostic sinks.
// Intended for low-level lifetime events where the logger may already be destroyed.
void appendLineToFile(std::filesystem::path const& path, std::string_view line);

} // namespace dearoreui::diagnostic
