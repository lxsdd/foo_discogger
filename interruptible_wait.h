#pragma once
#include <algorithm>
#include <chrono>
#include <thread>

// Pure, SDK-independent bounded sleep. The caller's abort callback is
// checked before waiting, between slices and after the final slice.
namespace discogger_net {
template <typename Abort>
void interruptible_wait(unsigned milliseconds, Abort& abort) {
    using clock = std::chrono::steady_clock;
    abort.check();
    const auto deadline = clock::now() + std::chrono::milliseconds(milliseconds);
    for (;;) {
        abort.check();
        const auto now = clock::now();
        if (now >= deadline) break;
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
        const auto slice = (std::min)(remaining, std::chrono::milliseconds(50));
        std::this_thread::sleep_for(slice.count() > 0 ? slice : std::chrono::milliseconds(1));
    }
    abort.check();
}
} // namespace discogger_net
