#include "../discogger_track_mapping_row.h"
#include <cassert>
#include <cstddef>
#include <limits>
#include <vector>
int main() {
    using discogger_matching::mapping_file_index;
    using discogger_matching::valid_presenter_row;
    assert(valid_presenter_row(0));
    assert(valid_presenter_row(17));
    assert(!valid_presenter_row(static_cast<std::size_t>(-1)));
    assert(!valid_presenter_row((std::numeric_limits<std::size_t>::max)()));
    const std::size_t invalid = static_cast<std::size_t>(-1);
    assert(mapping_file_index(0, 2, 0, invalid) == 0);
    assert(mapping_file_index(1, 2, 7, invalid) == 7); // reordered file
    assert(mapping_file_index(2, 2, 7, invalid) == -1); // no stale previous index
    assert(mapping_file_index(0, 1, invalid, invalid) == -1);
    assert(mapping_file_index(0, 1, static_cast<std::size_t>((std::numeric_limits<int>::max)()) + 1u, invalid) == -1);
    assert(mapping_file_index(0, 0, 0, invalid) == -1);
    using discogger_matching::same_file_selection;
    const std::vector<int> a{10, 20};
    assert(same_file_selection(a, std::vector<int>{10, 20}));
    assert(!same_file_selection(a, std::vector<int>{10, 30})); // same count, different file
    assert(!same_file_selection(a, std::vector<int>{20, 10})); // order changed
    assert(!same_file_selection(a, std::vector<int>{10}));
    assert(!same_file_selection(a, std::vector<int>{}));
    assert(same_file_selection(std::vector<int>{}, std::vector<int>{}));
    return 0;
}
