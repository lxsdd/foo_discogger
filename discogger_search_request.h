#pragma once
#include <cstdint>
#include <limits>

// Search callbacks run on the foobar UI thread, but may complete after a
// newer search has started in the SAME dialog HWND. Window identity alone
// does not establish that an asynchronous result still belongs to the UI.
// Only begin()/current()/matches() on the UI thread; workers carry tickets.
namespace discogger_async {
class search_request_epoch {
    std::uint64_t epoch_ = 1; // Zero is always an invalid/unbound ticket.
public:
    std::uint64_t begin() noexcept {
        if (++epoch_ == 0) ++epoch_;
        return epoch_;
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
