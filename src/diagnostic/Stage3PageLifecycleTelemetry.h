#pragma once

#include "api/types/Id.h"
#include "api/types/Page.h"

#include <filesystem>
#include <string_view>

namespace dearoreui::diagnostic {

void initializeStage3FileSink(std::filesystem::path dataDirectory, std::string const& sessionId);
void recordStage3PageCreated(api::ContextId id, api::PageInfo const& info, std::string_view url);
void recordStage3PageDestroyed(api::ContextId id, api::PageInfo const& info);

} // namespace dearoreui::diagnostic
