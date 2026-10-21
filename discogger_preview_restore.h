#pragma once
#include <cstddef>
#include <utility>

// The original (physical, read-only) values are either collapsed into one
// value shared across tracks, or have one value for each mapped track.
// Never access a PFC array by treating its pointer as an array of containers.
namespace discogger_preview {
template <typename Values>
bool restore_original_values(Values& current, const Values& original,
                             std::size_t displayed_track_count) {
    const std::size_t n = original.get_count();
    if (displayed_track_count == 0 || n == 0 ||
        (n != 1 && n != displayed_track_count))
        return false;
    // Build before replacing: an allocation/copy exception leaves the
    // current, unapproved preview unchanged. No physical metadata I/O.
    Values replacement;
    for (std::size_t i = 0; i < n; ++i)
        replacement.append_single(original[i]);
    current = std::move(replacement);
    return true;
}
} // namespace discogger_preview
