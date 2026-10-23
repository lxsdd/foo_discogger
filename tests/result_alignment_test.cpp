#include "../discogger_result_alignment.h"
#include "../discogger_number_match.h"
#include <cassert>
int main() {
    using discogger_result_alignment::valid_result_shape;
    using discogger_result_alignment::valid_track_location;
    assert(valid_track_location(0, 0, 1, 1));
    assert(valid_track_location(1, 2, 3, 4));
    assert(!valid_track_location(-1, 0, 2, 2));
    assert(!valid_track_location(0, -1, 2, 2));
    assert(!valid_track_location(2, 0, 2, 2));
    assert(!valid_track_location(0, 2, 2, 2));
    assert(!valid_track_location(0, 0, 0, 1));
    assert(!valid_track_location(0, 0, 1, 0));
    assert(!valid_track_location(2147483647, 0, 1, 1));
    assert(valid_result_shape(1, 1, 1, 1));
    assert(valid_result_shape(3, 3, 1, 1));
    assert(valid_result_shape(3, 3, 3, 3));
    assert(valid_result_shape(3, 3, 1, 3));
    assert(valid_result_shape(3, 3, 3, 1));
    assert(!valid_result_shape(3, 2, 1, 1));
    assert(!valid_result_shape(3, 4, 3, 3));
    assert(!valid_result_shape(3, 3, 2, 1));
    assert(!valid_result_shape(3, 3, 3, 2));
    assert(!valid_result_shape(3, 3, 0, 1));
    assert(!valid_result_shape(0, 0, 1, 1));
    // P1: track numbers without DISCNUMBER must never silently pick a
    // first match from two discs, or from conflicting global/local numbering.
    using discogger_number_matching::unique_number_match;
    unique_number_match single;
    single.consider(1, 2, 1, 1, 1, 1, 0, 0);
    single.consider(1, 2, 1, 2, 2, 2, 0, 1);
    assert(single.unique() && !single.ambiguous());
    assert(single.disc_index() == 0 && single.track_index() == 1);

    unique_number_match two_discs;
    two_discs.consider(0, 1, 1, 1, 1, 1, 0, 0);
    two_discs.consider(0, 1, 2, 1, 1, 3, 1, 0);
    assert(!two_discs.unique() && two_discs.ambiguous());

    unique_number_match explicit_disc;
    explicit_disc.consider(2, 1, 1, 1, 1, 1, 0, 0);
    explicit_disc.consider(2, 1, 2, 1, 1, 3, 1, 0);
    assert(explicit_disc.unique() && explicit_disc.disc_index() == 1);

    unique_number_match ordinal_collision;
    ordinal_collision.consider(0, 2, 1, 2, 2, 2, 0, 1);
    ordinal_collision.consider(0, 2, 2, 2, 2, 4, 1, 1);
    assert(ordinal_collision.ambiguous());

    unique_number_match same_track;
    same_track.consider(0, 2, 1, 2, 2, 2, 0, 1);
    assert(same_track.unique()); // Two conventions, one physical track.

    unique_number_match missing;
    missing.consider(1, 5, 1, 1, 1, 1, 0, 0);
    missing.consider(-1, 1, 1, 1, 1, 1, 0, 0);
    missing.consider(1, 0, 1, 0, 0, 1, 0, 0);
    assert(!missing.unique() && !missing.ambiguous());
    return 0;
}
