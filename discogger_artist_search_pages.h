#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>
#include "discogger_release_id.h"
#include "discogger_search_fallback.h"

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

// Only byte-exact (ASCII case-insensitive) names enter the native exact
// bucket. Stripped Discogs aliases, folded Latin accents and whole-word
// partials remain review-only: Artist (2) is not Artist (3).
inline constexpr std::size_t artist_search_rank_count = 5;
inline std::size_t rank_artist_search_hit(
    std::string_view query, std::string_view stripped_query,
    std::string_view title, std::string_view stripped_title) {
    const auto exact_query = ascii_lower(trim_copy(query));
    const auto exact_title = ascii_lower(trim_copy(title));
    if (!exact_query.empty() && exact_title == exact_query) return 0;

    const auto base_query = ascii_lower(trim_copy(stripped_query));
    const auto base_title = ascii_lower(trim_copy(stripped_title));
    if (!base_query.empty() && base_title == base_query) return 1;

    const auto folded_query = fold_title_words(query);
    const auto folded_title = fold_title_words(title);
    if (folded_query.size() < 3) return 4;
    if (folded_title == folded_query) return 2;

    // A substring within a different artist word (Matt vs Matthew) is
    // not a matching artist. Only whole-word spans rank as partial hints.
    auto at = folded_title.find(folded_query);
    while (at != std::string::npos) {
        const auto end = at + folded_query.size();
        if ((at == 0 || folded_title[at - 1] == ' ') &&
            (end == folded_title.size() || folded_title[end] == ' '))
            return 3;
        at = folded_title.find(folded_query, at + 1);
    }
    return 4;
}

inline bool continue_artist_search(std::size_t page, std::size_t pages,
                                   bool exact_name_found) noexcept {
    return page > 0 && page < pages &&
           page < max_artist_search_pages && !exact_name_found;
}
} // namespace discogger_search
