#include "../discogger_result_alignment.h"
#include <cassert>
int main() {
    using discogger_result_alignment::valid_result_shape;
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
