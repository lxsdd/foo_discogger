#include "../discogger_duration_match.h"
#include <cassert>
#include <cstddef>
int main() {
    using discogger_matching::unique_nearest_unclaimed;
    const std::size_t none = static_cast<std::size_t>(-1);
    assert(unique_nearest_unclaimed({}, {}) == none);
    assert(unique_nearest_unclaimed({2, 1}, {false}) == none);
    assert(unique_nearest_unclaimed({2, -1}, {false, false}) == none);
    assert(unique_nearest_unclaimed({2, 2}, {false, false}) == none);
    assert(unique_nearest_unclaimed({0, 1}, {false, false}) == 0);
    assert(unique_nearest_unclaimed({1, 0, 2}, {false, false, false}) == 1);
    assert(unique_nearest_unclaimed({0, 1}, {true, false}) == none);
    assert(unique_nearest_unclaimed({1, 1}, {true, false}) == none);
    assert(unique_nearest_unclaimed({5, 0, 1}, {true, false, false}) == 1);
    assert(unique_nearest_unclaimed({0, 1}, {true, true}) == none);
    assert(unique_nearest_unclaimed({0}, {false}) == 0);
    return 0;
}
