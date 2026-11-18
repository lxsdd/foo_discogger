#pragma once
#include <cstddef>

namespace discogger_ui {
// Return true only on a transition from outstanding work to completed.
// A zero or duplicate completion must not reopen the dialog, and an
// oversized completion must never wrap the unsigned pending count.
inline bool complete_preview_jobs(std::size_t& pending, std::size_t completed) noexcept {
    if (pending == 0 || completed == 0) return false;
    if (completed >= pending) {
        pending = 0;
        return true;
    }
    pending -= completed;
    return false;
}
} // namespace discogger_ui
