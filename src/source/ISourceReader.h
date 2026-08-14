#pragma once

#include "api/types/Page.h"
#include "api/types/Result.h"
#include "source/PageSourceSnapshot.h"

namespace dearoreui::source {

class ISourceReader {
public:
    virtual ~ISourceReader() = default;

    [[nodiscard]] virtual api::Result<PageSourceSnapshot> capture(api::PageInfo const& page) = 0;
};

} // namespace dearoreui::source
