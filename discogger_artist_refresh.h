#pragma once
#include <atomic>

// The per-artist mutex coalesces network loads. This gate preserves an
// explicitly requested online refresh across cache hits and failed loads.
namespace discogger_cache {
// A waiting load can skip its duplicate download if another worker has
// published the same artist in the meantime. A newer explicit refresh
// request must take precedence over that generation-based coalescing.
// Call only after acquiring the per-artist load mutex.
inline bool skip_coalesced_artist_load(bool loaded, unsigned long observed_generation,
                                      unsigned long current_generation,
                                      bool explicit_refresh_pending) noexcept {
    return loaded && observed_generation != current_generation &&
           !explicit_refresh_pending;
}
// Explicit online refresh must not be swallowed by a populated search row's
// display-only fast path; the canonical artist must receive a worker load.
inline bool should_fetch_artist(bool needs_data, bool selecting_releases,
                                bool explicit_refresh_pending) noexcept {
    return needs_data || (selecting_releases && explicit_refresh_pending);
}
class artist_release_refresh {
    std::atomic<bool> pending_{false};
public:
    void request() noexcept { pending_.store(true, std::memory_order_release); }
    bool pending() const noexcept { return pending_.load(std::memory_order_acquire); }
    bool take() noexcept { return pending_.exchange(false, std::memory_order_acq_rel); }
    void retry_after_failure(bool taken) noexcept { if (taken) request(); }
};
} // namespace discogger_cache
