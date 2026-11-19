#pragma once
#include <cstddef>
namespace discogger_ui {
// ArtistList is an explicit row-selection request, unlike Undef/on-init.
// A completed worker cannot reselect an artist after selection was cleared,
// or after the pending selection was replaced/consumed.
inline bool accept_artist_list_completion(std::size_t completed,
                                          std::size_t selected,
                                          std::size_t pending,
                                          std::size_t invalid) noexcept {
    return completed != invalid && selected == completed && pending == completed;
}
// ListView_RedrawItems takes an actual visible row, never the no-selection
// sentinel returned by GetFirstSelected or a row removed during refresh.
// A prior role is a ListView row, not a one-based artist role ID.
// Keep the invalid sentinel when the role no longer exists after refresh.
inline std::size_t previous_artist_redraw_row(std::size_t found_index,
                                              std::size_t count) noexcept {
    return found_index < count ? found_index : static_cast<std::size_t>(-1);
}
inline bool valid_artist_redraw_row(std::size_t row, std::size_t count) noexcept {
    return row < count;
}
}
