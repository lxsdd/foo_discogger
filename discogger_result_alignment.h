#pragma once
#include <cstddef>

// Generator approval flags have one row per enabled mapping; tag values and
// originals may each be collapsed to one shared value. Reject stale shapes
// before any in-memory mutation or physical write is scheduled.
namespace discogger_result_alignment {
inline bool valid_result_shape(std::size_t enabled_tracks,
    std::size_t approval_rows, std::size_t values, std::size_t originals) noexcept {
    return enabled_tracks != 0 && approval_rows == enabled_tracks &&
        (values == 1 || values == enabled_tracks) &&
        (originals == 1 || originals == enabled_tracks);
}
} // namespace discogger_result_alignment
