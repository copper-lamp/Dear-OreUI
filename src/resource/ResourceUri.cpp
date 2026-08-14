#include "resource/ResourceUri.h"

#include "api/manifest/ManifestValidator.h"
#include "api/types/Error.h"

#include <algorithm>
#include <sstream>

namespace dearoreui::resource {

namespace {

[[nodiscard]] constexpr std::string_view schemePrefix() { return "oreui://"; }

[[nodiscard]] ResourceUriScheme parseScheme(std::string_view scheme) {
    if (scheme == "resource") return ResourceUriScheme::Resource;
    if (scheme == "script") return ResourceUriScheme::Script;
    if (scheme == "style") return ResourceUriScheme::Style;
    return ResourceUriScheme::Unknown;
}

[[nodiscard]] std::string_view schemeName(ResourceUriScheme scheme) {
    switch (scheme) {
    case ResourceUriScheme::Resource:
        return "resource";
    case ResourceUriScheme::Script:
        return "script";
    case ResourceUriScheme::Style:
        return "style";
    default:
        return "unknown";
    }
}

} // namespace

api::Result<ResourceUri> ResourceUri::parse(std::string_view uri) {
    if (!uri.starts_with(schemePrefix())) {
        return api::Error{api::ErrorCode::InvalidFormat, "URI must start with oreui://"};
    }

    auto remainder = uri.substr(schemePrefix().size());
    auto slash     = remainder.find('/');
    if (slash == std::string_view::npos || slash == 0) {
        return api::Error{api::ErrorCode::InvalidFormat, "URI missing scheme segment"};
    }

    auto schemeStr = remainder.substr(0, slash);
    auto scheme    = parseScheme(schemeStr);
    if (scheme == ResourceUriScheme::Unknown) {
        return api::Error{api::ErrorCode::InvalidFormat, "URI scheme is not recognized"};
    }

    auto afterScheme = remainder.substr(slash + 1);
    slash            = afterScheme.find('/');
    if (slash == std::string_view::npos || slash == 0) {
        return api::Error{api::ErrorCode::InvalidFormat, "URI missing namespace segment"};
    }

    auto ns   = std::string(afterScheme.substr(0, slash));
    auto path = std::string(afterScheme.substr(slash + 1));

    if (!api::ManifestValidator::isValidNamespace(ns)) {
        return api::Error{api::ErrorCode::InvalidArgument, "URI namespace is invalid"};
    }

    if (path.empty() || !api::ManifestValidator::isValidPath(path)) {
        return api::Error{api::ErrorCode::InvalidArgument, "URI path is invalid"};
    }

    ResourceUri result;
    result.scheme       = scheme;
    result.modNamespace = std::move(ns);
    result.path         = std::move(path);
    return result;
}

std::string ResourceUri::toString() const {
    std::ostringstream stream;
    stream << "oreui://" << schemeName(scheme) << "/" << modNamespace << "/" << path;
    return stream.str();
}

bool ResourceUri::operator==(ResourceUri const& other) const {
    return scheme == other.scheme && modNamespace == other.modNamespace && path == other.path;
}

bool ResourceUri::operator!=(ResourceUri const& other) const { return !(*this == other); }

} // namespace dearoreui::resource
