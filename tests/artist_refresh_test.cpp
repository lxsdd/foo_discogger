#include "../discogger_artist_refresh.h"
#include <cassert>
#include <thread>
int main() {
    using discogger_cache::skip_coalesced_artist_load;
    using discogger_cache::should_fetch_artist;
    // A populated artist search row must never swallow an explicit refresh.
    assert(should_fetch_artist(false, true, true));
    assert(should_fetch_artist(true, true, false));
    assert(should_fetch_artist(true, false, false));
    assert(!should_fetch_artist(false, true, false));
    assert(!should_fetch_artist(false, false, true));
    // Another worker's freshly committed list suppresses a redundant fetch,
    // unless a newer, explicit user refresh is pending after that commit.
    assert(skip_coalesced_artist_load(true, 10, 11, false));
    assert(!skip_coalesced_artist_load(true, 10, 11, true));
    assert(!skip_coalesced_artist_load(true, 10, 10, false));
    assert(!skip_coalesced_artist_load(false, 10, 11, false));
    // The pending refresh survives the skipped-in-flight race and will be
    // consumed by the first worker that actually takes the per-artist lock.
    discogger_cache::artist_release_refresh queued;
    queued.request();
    assert(!skip_coalesced_artist_load(true, 3, 4, queued.pending()));
    assert(queued.take() && !queued.pending());
    assert(skip_coalesced_artist_load(true, 3, 4, queued.pending()));

    discogger_cache::artist_release_refresh gate;
    assert(!gate.pending() && !gate.take());
    gate.request();
    gate.request();
    assert(gate.pending() && gate.take() && !gate.pending());
    assert(!gate.take());
    gate.request();
    const bool failed = gate.take();
    assert(failed && !gate.pending());
    gate.retry_after_failure(failed);
    assert(gate.pending() && gate.take() && !gate.pending());
    gate.retry_after_failure(false);
    assert(!gate.pending());

    // A disk-cache/setup failure before any HTTP request must requeue intent.
    gate.request();
    const bool before_cache_probe = gate.take();
    try {
        throw 42; // synthetic offline setup error
    } catch (...) {
        gate.retry_after_failure(before_cache_probe);
    }
    assert(gate.pending() && gate.take() && !gate.pending());
    gate.request();
    const bool in_flight = gate.take();
    std::thread next([&] { gate.request(); });
    next.join();
    assert(in_flight && gate.pending());
    gate.retry_after_failure(in_flight);
    assert(gate.pending() && gate.take() && !gate.pending());
    return 0;
}
