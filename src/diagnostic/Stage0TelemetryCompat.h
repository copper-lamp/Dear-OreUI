#pragma once

#include <filesystem>

namespace dearoreui::diagnostic {

void initializeStage0FileSink(std::filesystem::path dataDirectory);
void startStage0Session();
void recordStage0(std::string_view event, std::string_view fields = {});
void resetStage0Session();

} // namespace dearoreui::diagnostic
