#pragma once

#include "api/types/Result.h"
#include "api/manifest/ModManifest.h"
#include "api/manifest/ResourceManifest.h"
#include "api/manifest/ScriptManifest.h"
#include "api/manifest/StyleSheetManifest.h"

#include <cstdint>
#include <string>

namespace dearoreui::api {

constexpr std::uint32_t DearOreUIProtocolVersion = 1;

class ManifestValidator {
public:
    [[nodiscard]] static Result<void> validate(ModManifest const& manifest);
    [[nodiscard]] static Result<void> validate(ResourceManifest const& manifest);
    [[nodiscard]] static Result<void> validate(ScriptManifest const& manifest);
    [[nodiscard]] static Result<void> validate(StyleSheetManifest const& manifest);

    [[nodiscard]] static bool isValidId(std::string_view id);
    [[nodiscard]] static bool isValidNamespace(std::string_view ns);
    [[nodiscard]] static bool isValidPath(std::string_view path);

private:
    [[nodiscard]] static Result<void> validateCommonResource(
        std::string_view typeName,
        std::string const& modNamespace,
        std::string const& path
    );
};

} // namespace dearoreui::api
