#pragma once

#include <filesystem>
#include <string>

namespace dearoreui::diagnostic {

void initializeStage0FileSink(const std::filesystem::path& dataDirectory);
void startStage0Session();
void recordStage0(std::string_view event, std::string_view fields = {});
void resetStage0Session();

[[nodiscard]] std::string const& currentStage0SessionId();

} // namespace dearoreui::diagnostic
