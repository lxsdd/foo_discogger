#pragma once
#include <cstdint>
#include <limits>

// Search callbacks run on the foobar UI thread, but may complete after a
// newer search has started in the SAME dialog HWND. Window identity alone
// does not establish that an asynchronous result still belongs to the UI.
// Only begin()/current()/matches() on the UI thread; workers carry tickets.
namespace discogger_async {
class search_request_epoch {
    std::uint64_t epoch_ = 1; // Active request; zero is invalid.
    std::uint64_t next_epoch_ = 1; // Monotonic allocation, never rewound by rollback.
public:
    std::uint64_t begin() noexcept {
        if (++next_epoch_ == 0) ++next_epoch_;
        epoch_ = next_epoch_;
        return epoch_;
    }
    // A modeless worker can reject start() while the previous worker is still
    // valid. Undo only our own uncommitted selection; never resurrect an older
    // request after a reentrant UI selection has already superseded it.
    bool rollback_failed_start(std::uint64_t attempted, std::uint64_t previous) noexcept {
        if (previous == 0 || !matches(attempted)) return false;
        epoch_ = previous; // Restore validity, NOT ticket allocation (avoid ABA).
        return true;
    }
    std::uint64_t current() const noexcept { return epoch_; }
    bool matches(std::uint64_t ticket) const noexcept {
        return ticket != 0 && ticket == epoch_;
    }
};

// A Win32 hide may synchronously dispatch messages that replace or destroy
// the original release dialog. Recheck the ticket after that reentrant step
// before opening the track comparison UI. Runs only on the main thread.
// Commit history only after successful preview and a second epoch check.
// All callbacks execute on the UI thread; no I/O occurs in this helper.
template <typename IsCurrent, typename Hide, typename Present, typename RecordHistory>
bool commit_release_preview_if_current(IsCurrent is_current, Hide hide, Present present,
                                       RecordHistory record_history) {
    if (!is_current()) return false;
    hide();
    if (!is_current()) return false;
    if (!present()) return false;
    if (!is_current()) return false;
    record_history();
    return true;
}
} // namespace discogger_async
