#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>
#include "discogger_release_id.h"

// Bounded, review-only artist discovery. The network layer validates each
// page's complete pagination and item count before staging any row.
// This helper never approves an identity or writes media metadata.
namespace discogger_search {
inline constexpr std::size_t max_artist_search_pages = 5;
inline constexpr std::size_t max_artist_search_hits = max_artist_search_pages * 100;

struct artist_search_hit {
    std::string id;
    std::string title;
};

inline bool stage_artist_search_hit(std::vector<artist_search_hit>& hits,
                                    std::unordered_set<std::string>& seen,
                                    std::string_view id, std::string_view title) {
    const std::string canonical_id = canonical_release_id(id);
    if (canonical_id.empty() || title.empty() ||
        hits.size() >= max_artist_search_hits) return false;
    const auto inserted = seen.emplace(canonical_id);
    if (!inserted.second) return false;
    try {
        hits.push_back({canonical_id, std::string(title)});
    } catch (...) {
        seen.erase(inserted.first);
        throw;
    }
    return true;
}

inline bool continue_artist_search(std::size_t page, std::size_t pages,
                                   bool exact_name_found) noexcept {
    return page > 0 && page < pages &&
           page < max_artist_search_pages && !exact_name_found;
}
} // namespace discogger_search
