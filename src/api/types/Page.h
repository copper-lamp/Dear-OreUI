#pragma once

#include "api/types/Id.h"

#include <optional>
#include <string>

namespace dearoreui::api {

enum class PageScope {
    Any,
    MainMenu,
    PlayScreen,
    Settings,
    Pause,
    InGame,
    Custom,
};

struct RouterLocationSnapshot {
    std::string path;
    std::string query;
    std::string fragment;

    [[nodiscard]] std::string toString() const {
        if (query.empty() && fragment.empty()) return path;
        if (fragment.empty()) return path + "?" + query;
        if (query.empty()) return path + "#" + fragment;
        return path + "?" + query + "#" + fragment;
    }

    [[nodiscard]] bool operator==(RouterLocationSnapshot const& other) const {
        return path == other.path && query == other.query && fragment == other.fragment;
    }

    [[nodiscard]] bool operator!=(RouterLocationSnapshot const& other) const { return !(*this == other); }
};

struct PageInfo {
    PageId                                id;
    PageScope                             scope{PageScope::Custom};
    std::optional<RouterLocationSnapshot> location;
};

} // namespace dearoreui::api
