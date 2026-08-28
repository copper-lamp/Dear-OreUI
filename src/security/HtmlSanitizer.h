#pragma once

#include "api/types/Result.h"

#include <string>
#include <string_view>

namespace dearoreui::security {

// Validates external mod-provided htmlBody before it enters the injection
// pipeline (S2: XSS / uri-out-of-bounds hardening). Fail-closed: any
// disallowed tag, event attribute, dangerous URL scheme, or malformed
// oreui:// reference rejects the whole fragment.
class HtmlSanitizer {
public:
    [[nodiscard]] static api::Result<void> validate(std::string_view htmlBody);
};

} // namespace dearoreui::security
