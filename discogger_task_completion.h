#pragma once
#include <exception>
#include <utility>

// SDK modeless on_success() callbacks run on the UI thread. In particular,
// physical tag preflight/readback and artwork scheduling can throw ordinary
// std::exceptions (not only Discogger's own exception type). Never let those
// escape into foobar's UI callback; report a fatal error so on_error can revoke
// the in-memory preview. A failing reporter must not terminate the host.
namespace discogger_async {
template <typename Action, typename Report>
void run_success_with_error_boundary(Action&& action, Report&& report) noexcept {
    try {
        std::forward<Action>(action)();
    } catch (const std::exception& e) {
        try { std::forward<Report>(report)(e.what()); } catch (...) {}
    } catch (...) {
        try { std::forward<Report>(report)("Unknown error completing task."); }
        catch (...) {}
    }
}
} // namespace discogger_async
