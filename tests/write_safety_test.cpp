#include "../discogger_write_safety.h"
#include <array>
#include <cassert>
#include <cstddef>

int main() {
    const std::array<unsigned, 3> physical = {0, 0, 0};
    const std::array<unsigned, 3> mixed = {0, 7, 0};
    const std::array<unsigned, 2> virtual_only = {1, 2};
    assert(!discogger_write_safety::contains_virtual_subsong(physical.size(),
        [&](std::size_t i) { return physical[i]; }));
    assert(discogger_write_safety::contains_virtual_subsong(mixed.size(),
        [&](std::size_t i) { return mixed[i]; }));
    assert(discogger_write_safety::contains_virtual_subsong(virtual_only.size(),
        [&](std::size_t i) { return virtual_only[i]; }));
    std::size_t calls = 0;
    assert(discogger_write_safety::contains_virtual_subsong(mixed.size(),
        [&](std::size_t i) { ++calls; return mixed[i]; }));
    assert(calls == 2); // stop before considering later physical members
    // Positive, negative, and duplicate-completion regressions.
    int artwork_starts = 0;
    discogger_write_safety::write_completion_gate failed;
    assert(!failed.finish(false, [&] { ++artwork_starts; }));
    assert(!failed.finish(true, [&] { ++artwork_starts; }));
    assert(artwork_starts == 0);
    discogger_write_safety::write_completion_gate succeeded;
    assert(succeeded.finish(true, [&] { ++artwork_starts; }));
    assert(!succeeded.finish(true, [&] { ++artwork_starts; }));
    assert(!succeeded.finish(false, [&] { ++artwork_starts; }));
    assert(artwork_starts == 1);
    assert(!discogger_write_safety::contains_virtual_subsong(0,
        [&](std::size_t) -> unsigned { assert(false); return 0; }));
}
