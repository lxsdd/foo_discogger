#pragma once
#include <atomic>

// The per-artist mutex coalesces network loads. This gate preserves an
// explicitly requested online refresh across cache hits and failed loads.
namespace discogger_cache {
class artist_release_refresh {
    std::atomic<bool> pending_{false};
public:
    void request() noexcept { pending_.store(true, std::memory_order_release); }
    bool pending() const noexcept { return pending_.load(std::memory_order_acquire); }
    bool take() noexcept { return pending_.exchange(false, std::memory_order_acq_rel); }
    void retry_after_failure(bool taken) noexcept { if (taken) request(); }
};
} // namespace discogger_cache
