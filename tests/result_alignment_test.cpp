#include "../discogger_result_alignment.h"
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
    return 0;
}
