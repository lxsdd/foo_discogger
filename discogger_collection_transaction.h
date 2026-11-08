#pragma once
#include <cstddef>
#include <stdexcept>
#include <string_view>

// Build complete Discogs collection snapshots privately. Caller owns publication.
namespace discogger_collection {
// Discogs collection folder resource_url is untrusted remote JSON. Never use
// it to forward an authenticated API fetch to a different origin or path.
inline bool valid_folder_resource_url(std::string_view url) {
    constexpr std::string_view origin = "https://api.discogs.com/users/";
    constexpr std::string_view segment = "/collection/folders/";
    if (url.size() > 2048 || url.substr(0, origin.size()) != origin) return false;
    const auto path = url.substr(origin.size());
    const auto at = path.find(segment);
    if (at == std::string_view::npos || at == 0) return false;
    const auto user = path.substr(0, at);
    const auto folder = path.substr(at + segment.size());
    if (folder.empty() || folder.size() > 20) return false;
    for (std::size_t i = 0; i < user.size(); ++i) {
        const char c = user[i];
        const bool unreserved = (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '.' || c == '_' || c == '~';
        if (unreserved) continue;
        if (c != '%' || i + 2 >= user.size()) return false;
        const auto hex = [](char h) { return (h >= '0' && h <= '9') ||
            (h >= 'a' && h <= 'f') || (h >= 'A' && h <= 'F'); };
        if (!hex(user[i + 1]) || !hex(user[i + 2])) return false;
        i += 2;
    }
    for (char c : folder) if (c < '0' || c > '9') return false;
    return true;
}

template <typename Stage, typename Parse, typename Check>
Stage stage_collection_pages(std::size_t count, Parse&& parse, Check&& check) {
    if (!count) throw std::invalid_argument("Empty collection release page set");
    Stage staged{};
    for (std::size_t page = 0; page < count; ++page) {
        check();
        parse(staged, page);
    }
    check();
    return staged;
}
} // namespace discogger_collection
