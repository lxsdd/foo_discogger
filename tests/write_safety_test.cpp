#include "../discogger_write_safety.h"
#include <array>
#include <cassert>
#include <cstddef>
#include <stdexcept>

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
    // Snapshot/no-op planning: unchanged metadata and ReplayGain never schedule I/O.
    using decision = discogger_write_safety::metadata_write_decision;
    using discogger_write_safety::plan_metadata_write;
    assert(plan_metadata_write(false, false, false, false) == decision::skip);
    assert(plan_metadata_write(true, false, false, false) == decision::reject_missing_snapshot);
    assert(plan_metadata_write(true, true, true, true) == decision::skip);
    assert(plan_metadata_write(true, true, false, true) == decision::write);
    assert(plan_metadata_write(true, true, true, false) == decision::write);
    assert(plan_metadata_write(true, true, false, false) == decision::write);
    // Artwork requires its own independently captured snapshot.
    using art_decision = discogger_write_safety::artwork_write_decision;
    using discogger_write_safety::plan_artwork_write;
    assert(plan_artwork_write(false, false, false) == art_decision::skip);
    assert(plan_artwork_write(true, false, false) == art_decision::reject_missing_snapshot);
    assert(plan_artwork_write(true, true, true) == art_decision::skip);
    assert(plan_artwork_write(true, true, false) == art_decision::write);
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
    discogger_write_safety::write_completion_gate throwing;
    bool caught = false;
    try { throwing.finish(true, [] { throw std::runtime_error("synthetic callback failure"); }); }
    catch (const std::runtime_error&) { caught = true; }
    assert(caught);
    assert(!throwing.finish(true, [&] { ++artwork_starts; }));
    assert(artwork_starts == 1);
    assert(!discogger_write_safety::contains_virtual_subsong(0,
        [&](std::size_t) -> unsigned { assert(false); return 0; }));
}
