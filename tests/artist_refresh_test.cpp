#include "../discogger_artist_refresh.h"
#include <cassert>
#include <thread>
int main() {
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
