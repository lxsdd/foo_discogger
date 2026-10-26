#pragma once
#include <cstddef>
#include <limits>
#include <utility>

// Read-only tree label accounting; a cached release may be absent or partial.
namespace discogger_tree {
// Release-tree rows may outlive a cache rebuild. Index and row-type
// validation is independent of the foobar GUI and tested on both architectures.
enum class row_kind { master, subrelease, standalone, invalid };
inline bool valid_row_indices(row_kind kind, std::size_t master_index,
                              std::size_t release_index, std::size_t master_count,
                              std::size_t subrelease_count, std::size_t release_count) noexcept {
    switch (kind) {
    case row_kind::master: return master_index < master_count;
    case row_kind::subrelease:
        return master_index < master_count && release_index < subrelease_count;
    case row_kind::standalone: return release_index < release_count;
    default: return false;
    }
}

template <typename ReleasePtr>
std::pair<std::size_t, std::size_t> count_cached_tracks(const ReleasePtr& release) {
    if (!release) return {0, 0};
    std::size_t disc_count = 0, track_count = 0;
    for (std::size_t i = 0; i < release->discs.get_count(); ++i) {
        const auto& disc = release->discs[i];
        if (!disc) continue;
        const auto tracks = static_cast<std::size_t>(disc->tracks.size());
        if (tracks > (std::numeric_limits<std::size_t>::max)() - track_count)
            return {0, 0};
        ++disc_count;
        track_count += tracks;
    }
    return {disc_count, track_count};
}
// A cache entry can outlive an interrupted release-tree rebuild and contain
// no display columns. Never dereference begin() on an empty cached row.
// On a cache miss/invalid row, preserve caller outputs for safe regeneration.
template<typename Cache, typename Text, typename Row>
bool copy_cached_display_row(const Cache* cache, int key, Text& text, Row& row) {
    if (!cache) return false;
    const auto it = cache->find(key);
    if (it == cache->end() || it->second.first.col_data_list.empty()) return false;
    text = it->second.first.col_data_list.begin()->second;
    row = it->second.first;
    return true;
}
} // namespace discogger_tree
