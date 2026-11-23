#pragma once
#include <cstddef>
#include <limits>
#include <algorithm>
#include <vector>

// A Discogs row without a corresponding physical file must never inherit
// the preceding row's file index, including after list reorder/crop.
namespace discogger_matching {
// A failed owner-data lookup does not initialize its output iterator.
// Never dereference it after a stale row callback or variant mismatch.
inline bool valid_presenter_row(std::size_t result) noexcept {
    return result != static_cast<std::size_t>(-1);
}

// Reject stale artwork callbacks before dereferencing a presenter iterator.
inline bool valid_artwork_row(std::size_t row, std::size_t count,
                              std::size_t lookup_result) noexcept {
    return row < count && valid_presenter_row(lookup_result);
}

// The same validity gate applies to owner-data text and action callbacks.
// In particular, a refresh may invalidate a row between display and click.
// Compare the ordered physical selection, not merely its count. Keeping the
// order stable avoids resetting manual mapping edits on ordinary redraws.
template<typename Handle>
inline bool same_file_selection(const std::vector<Handle>& previous,
                                const std::vector<Handle>& current) {
    return previous.size() == current.size() &&
        std::equal(previous.begin(), previous.end(), current.begin());
}
inline int mapping_file_index(std::size_t row, std::size_t file_count,
                              std::size_t original_index, std::size_t invalid_index) noexcept {
    if (row >= file_count || original_index == invalid_index ||
        original_index > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
        return -1;
    return static_cast<int>(original_index);
}
} // namespace discogger_matching
