#pragma once
#include <cstddef>
#include <limits>
#include <utility>

// Read-only tree label accounting; a cached release may be absent or partial.
namespace discogger_tree {
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
} // namespace discogger_tree
