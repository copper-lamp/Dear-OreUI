#include "component/ThemeTokens.h"

namespace dearoreui::component {

ThemeTokens const& defaultThemeTokens() {
    static ThemeTokens const tokens;
    return tokens;
}

} // namespace dearoreui::component
