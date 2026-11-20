#include "../discogger_artist_selection_completion.h"
#include <cassert>
#include <cstddef>
int main() {
    constexpr std::size_t invalid = static_cast<std::size_t>(-1);
    using discogger_ui::accept_artist_list_completion;
    using discogger_ui::valid_artist_redraw_row;
    using discogger_ui::previous_artist_redraw_row;
    // Role positions are distinct from zero-based visible ListView rows.
    assert(previous_artist_redraw_row(0, 3) == 0);
    assert(previous_artist_redraw_row(1, 3) == 1);
    assert(previous_artist_redraw_row(2, 3) == 2);
    assert(previous_artist_redraw_row(3, 3) == invalid);
    assert(previous_artist_redraw_row(invalid, 3) == invalid);
    assert(previous_artist_redraw_row(0, 0) == invalid);
    assert(valid_artist_redraw_row(previous_artist_redraw_row(2, 3), 3));
    assert(!valid_artist_redraw_row(previous_artist_redraw_row(2, 2), 2));
    // Native ListView redraw must not receive a no-selection or stale index.
    assert(!valid_artist_redraw_row(invalid, 0));
    assert(!valid_artist_redraw_row(invalid, 3));
    assert(!valid_artist_redraw_row(0, 0));
    assert(valid_artist_redraw_row(0, 3));
    assert(valid_artist_redraw_row(2, 3));
    assert(!valid_artist_redraw_row(3, 3));
    // A different current preview must not suppress a matching visible row.
    assert(discogger_ui::artist_role_identity_matches(34, 34, invalid));
    assert(!discogger_ui::artist_role_identity_matches(12, 34, invalid));
    assert(!discogger_ui::artist_role_identity_matches(invalid, 34, invalid));
    assert(!discogger_ui::artist_role_identity_matches(34, invalid, invalid));
    // Initial selection, and explicit refresh of that same selected artist.
    assert(accept_artist_list_completion(12,12,12,invalid));
    assert(accept_artist_list_completion(12,12,12,invalid));
    // User cleared the row while the original worker was in flight.
    assert(!accept_artist_list_completion(12,invalid,invalid,invalid));
    assert(!accept_artist_list_completion(12,invalid,12,invalid));
    // Selection B replaced A, then a late A completion arrives.
    assert(!accept_artist_list_completion(12,34,34,invalid));
    assert(!accept_artist_list_completion(12,34,invalid,invalid));
    // Reentrant completion consumed the pending request; duplicates are stale.
    assert(!accept_artist_list_completion(12,12,invalid,invalid));
    assert(!accept_artist_list_completion(invalid,invalid,invalid,invalid));
    assert(!accept_artist_list_completion(34,12,34,invalid));
    // Current B response still succeeds after rejecting stale A.
    assert(accept_artist_list_completion(34,34,34,invalid));
}
