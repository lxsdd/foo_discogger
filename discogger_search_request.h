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
} // namespace discogger_async
