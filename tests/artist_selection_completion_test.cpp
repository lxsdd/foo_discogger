#include "../discogger_artist_selection_completion.h"
#include <cassert>
#include <cstddef>
int main() {
    constexpr std::size_t invalid = static_cast<std::size_t>(-1);
    using discogger_ui::accept_artist_list_completion;
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
