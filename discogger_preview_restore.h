#pragma once
#include <cstddef>
#include <utility>

// The original (physical, read-only) values are either collapsed into one
// value shared across tracks, or have one value for each mapped track.
// Never access a PFC array by treating its pointer as an array of containers.
namespace discogger_preview {
// PFC preview values can be shared (one item) or per visible track.
// Zero-length and stale mismatched shapes must not be indexed.
inline std::size_t safe_value_index(std::size_t row, std::size_t value_count,
                                    std::size_t track_count) noexcept {
    if (row >= track_count || value_count == 0 ||
        (value_count != 1 && value_count != track_count))
        return static_cast<std::size_t>(-1);
    return value_count == 1 ? 0 : row;
}

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
// Release::load_preview skips populated search titles. Force a preview
// refresh without discarding the last valid title on abort, HTTP error,
// partial parsing or an empty successful response. No media/cache writes.
template <typename Title, typename Load>
void refresh_search_preview_title(Title& title, Load&& load) {
    Title previous(title);
    title = "";
    try {
        std::forward<Load>(load)();
    } catch (...) {
        title = std::move(previous);
        throw;
    }
    if (title.length() == 0) title = std::move(previous);
}
} // namespace discogger_preview
