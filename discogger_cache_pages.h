#pragma once
#include <cstddef>
#include <string_view>
#include <vector>

namespace discogger_cache {
inline constexpr std::size_t max_cached_pages = 10000;

// Discogs API pagination is untrusted network data. Reject impossible cursors,
// oversized page ranges and metadata changes across pages instead of returning
// an incomplete artist, search or collection result.
inline bool valid_online_page(std::size_t requested_page, std::size_t returned_page,
                              std::size_t pages, std::size_t per_page, std::size_t items,
                              std::size_t first_pages, std::size_t first_per_page,
                              std::size_t first_items) noexcept {
    if (requested_page == 0 || requested_page > max_cached_pages ||
        returned_page != requested_page || pages > max_cached_pages ||
        per_page == 0 || per_page > 100) return false;
    // Discogs may report zero pages for an empty first-page result.
    if (items == 0) {
        if (pages > 1 || requested_page != 1) return false;
    } else {
        // Reject partial/contradictory API pagination before any result can
        // be published to the GUI or accepted as a complete disk cache.
        // Subtraction avoids overflow when items is untrusted SIZE_MAX.
        if (pages == 0 || pages != 1 + (items - 1) / per_page)
            return false;
    }
    if (pages != 0 && requested_page > pages) return false;
    if (requested_page > 1 &&
        (pages != first_pages || per_page != first_per_page ||
         items != first_items)) return false;
    return true;
}

// Disk page-N is zero-based; Discogs pagination.page is one-based.
inline bool parse_cache_page_folder(std::string_view name, std::size_t& index) noexcept {
    constexpr std::string_view prefix = "page-";
    if (name.substr(0, prefix.size()) != prefix || name.size() == prefix.size()) return false;
    std::size_t value = 0;
    for (std::size_t i = prefix.size(); i < name.size(); ++i) {
        const char c = name[i];
        if (c < '0' || c > '9') return false;
        const auto digit = static_cast<std::size_t>(c - '0');
        if (value > (max_cached_pages - 1 - digit) / 10) return false;
        value = value * 10 + digit;
    }
    if (name.size() > prefix.size() + 1 && name[prefix.size()] == '0') return false;
    index = value;
    return true;
}

inline bool complete_cache_page_sequence(const std::vector<std::size_t>& sorted_indexes) noexcept {
    if (sorted_indexes.empty() || sorted_indexes.size() > max_cached_pages) return false;
    for (std::size_t i = 0; i < sorted_indexes.size(); ++i)
        if (sorted_indexes[i] != i) return false;
    return true;
}

inline bool valid_cache_pagination(std::size_t folder_index, std::size_t folder_count,
                                   std::size_t api_page, std::size_t api_pages,
                                   std::size_t api_per_page, std::size_t api_items) noexcept {
    if (!folder_count || folder_count > max_cached_pages || folder_index >= folder_count) return false;
    if (api_pages != folder_count || api_page != folder_index + 1) return false;
    if (api_per_page == 0 || api_per_page > 100) return false;
    if (api_items == 0) return folder_count == 1;
    return 1 + (api_items - 1) / api_per_page == folder_count;
}

inline bool valid_cache_payload_count(std::size_t folder_index, std::size_t folder_count,
                                      std::size_t api_per_page, std::size_t api_items,
                                      std::size_t actual_items) noexcept {
    if (!valid_cache_pagination(folder_index, folder_count, folder_index + 1,
                                folder_count, api_per_page, api_items)) return false;
    if (folder_index > api_items / api_per_page) return false;
    const std::size_t offset = folder_index * api_per_page;
    const std::size_t remaining = api_items - offset;
    const std::size_t expected = remaining < api_per_page ? remaining : api_per_page;
    return actual_items == expected;
}
}
