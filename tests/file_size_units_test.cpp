#include "../discogger_file_size_units.h"
#include <cassert>
#include <cstdint>
#include <limits>

int main() {
    using discogger_file_size::split;
    constexpr std::uint64_t kib = 1024ULL;
    constexpr std::uint64_t mib = kib * kib;
    constexpr std::uint64_t gib = mib * kib;
    constexpr std::uint64_t tib = gib * kib;
    static_assert(split(0).unit_index == 0, "zero is bytes");
    static_assert(split(1023).unit_index == 0, "byte boundary");
    static_assert(split(kib).unit_index == 1, "KB boundary");
    static_assert(split(mib).unit_index == 2, "MB boundary");
    static_assert(split(gib).unit_index == 3, "GB boundary");
    static_assert(split(tib).unit_index == 4, "TB boundary");
    static_assert(split(tib - 1).unit_index == 3, "just below TB");
    static_assert(split(tib - 1).whole == 1023, "last GB");
    static_assert(split(tib + 512 * gib).whole == 1, "fractional TB");
    static_assert(split(tib + 512 * gib).remainder == 512, "TB remainder");
    static_assert(split(std::numeric_limits<std::uint64_t>::max()).unit_index == 4,
                  "huge sizes must never exceed the units array");
    assert(split(5 * tib).whole == 5);
    assert(split(0).remainder == 0);
    return 0;
}
