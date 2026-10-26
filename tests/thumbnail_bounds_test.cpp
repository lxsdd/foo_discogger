#include "../discogger_thumbnail_bounds.h"
#include <cassert>
#include <cstddef>
#include <limits>
int main() {
    using namespace discogger_thumbnail;
    assert(!valid_cache_size(0));
    assert(valid_cache_size(1));
    assert(valid_cache_size(max_cache_bytes));
    assert(!valid_cache_size(max_cache_bytes + 1));
    assert(!valid_cache_size(std::numeric_limits<std::uintmax_t>::max()));
    std::size_t index = 4;
    assert(!consume_artist_images(0, index) && index == 4);
    assert(!consume_artist_images(3, index) && index == 1);
    assert(consume_artist_images(2, index) && index == 1);
    index = std::numeric_limits<std::size_t>::max();
    assert(!consume_artist_images(3, index));
    assert(index == std::numeric_limits<std::size_t>::max() - 3);
    assert(!complete_read(-1, 5));
    assert(!complete_read(4, 5));
    assert(complete_read(5, 5));
    assert(!complete_read(6, 5));
    return 0;
}
