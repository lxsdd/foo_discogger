#pragma once
#include <cstddef>
#include <utility>

// Pure, SDK-independent preflight used by both the task and the final writer.
// A mixed selection is rejected atomically before any asynchronous tag writes.
namespace discogger_write_safety {
template <typename SubsongIndexAt>
bool contains_virtual_subsong(std::size_t count, SubsongIndexAt index_at) {
    for (std::size_t i = 0; i < count; ++i) {
        if (index_at(i) != 0) return true;
    }
    return false;
}
// Single-shot completion gate: never start dependent artwork after a failed
// physical tag commit, and never dispatch twice on duplicate notifications.
class write_completion_gate {
    bool completed_ = false;
public:
    template <typename SuccessAction>
    bool finish(bool successful, SuccessAction&& action) {
        if (completed_) return false;
        completed_ = true;
        if (!successful) return false;
        std::forward<SuccessAction>(action)();
        return true;
    }
};
} // namespace discogger_write_safety
