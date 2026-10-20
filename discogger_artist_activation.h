#pragma once

#include <cstddef>

// Pure preflight for a genuine list-row double-click. Blank space, stale
// selection, and out-of-bounds notifications must never load a release.
namespace discogger_ui {
inline bool should_load_artist_on_double_click(
    int clicked_row, std::size_t selected_row, std::size_t artist_count) {
    return clicked_row >= 0 &&
        selected_row != static_cast<std::size_t>(-1) &&
        static_cast<std::size_t>(clicked_row) == selected_row &&
        selected_row < artist_count;
}
}
