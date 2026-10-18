#pragma once
#include <cstddef>

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
} // namespace discogger_write_safety
