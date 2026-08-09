#pragma once

#include <string_view>

namespace dearoreui::diagnostic {

void startStage0Session();
void recordStage0(std::string_view event, std::string_view fields = {});
void resetStage0Session();

}
