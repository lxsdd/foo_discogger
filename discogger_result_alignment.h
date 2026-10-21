#pragma once
#include <cstddef>

// Generator approval flags have one row per enabled mapping; tag values and
// originals may each be collapsed to one shared value. Reject stale shapes
// before any in-memory mutation or physical write is scheduled.
namespace discogger_result_alignment {
// Discogs coordinates are signed in the native mapping structure. Do not
// convert negative or stale indices to unsigned array offsets.
inline bool valid_track_location(int disc, int track,
    std::size_t disc_count, std::size_t track_count) noexcept {
    return disc >= 0 && track >= 0 &&
        static_cast<std::size_t>(disc) < disc_count &&
        static_cast<std::size_t>(track) < track_count;
}
inline bool valid_result_shape(std::size_t enabled_tracks,
    std::size_t approval_rows, std::size_t values, std::size_t originals) noexcept {
    return enabled_tracks != 0 && approval_rows == enabled_tracks &&
        (values == 1 || values == enabled_tracks) &&
        (originals == 1 || originals == enabled_tracks);
}
} // namespace discogger_result_alignment
