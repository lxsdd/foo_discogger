#include "../discogger_track_mapping_row.h"
#include <cassert>
#include <cstddef>
#include <limits>
int main() {
    using discogger_matching::mapping_file_index;
    const std::size_t invalid = static_cast<std::size_t>(-1);
    assert(mapping_file_index(0, 2, 0, invalid) == 0);
    assert(mapping_file_index(1, 2, 7, invalid) == 7); // reordered file
    assert(mapping_file_index(2, 2, 7, invalid) == -1); // no stale previous index
    assert(mapping_file_index(0, 1, invalid, invalid) == -1);
    assert(mapping_file_index(0, 1, static_cast<std::size_t>((std::numeric_limits<int>::max)()) + 1u, invalid) == -1);
    assert(mapping_file_index(0, 0, 0, invalid) == -1);
    return 0;
}
