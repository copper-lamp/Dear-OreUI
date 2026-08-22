#pragma once

#include "api/types/Event.h"

namespace dearoreui::api {

class IEventApi {
public:
    virtual ~IEventApi() = default;
    [[nodiscard]] virtual Result<EventPublishResult> publishEvent(EventPublishOptions options) = 0;
};

} // namespace dearoreui::api
