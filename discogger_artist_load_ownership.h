#pragma once
#include <algorithm>
#include <cstddef>

// Call with the shared active-artist registry mutex held. The caller-owned
// vector records exactly which markers this task inserted, not every artist
// it searched. Consume that ownership after a completion/cancel/failure so
// duplicate callbacks cannot erase a later task's registration.
namespace discogger_async {
template <typename Owned, typename Registry, typename Equal>
std::size_t release_owned_ids(Owned& owned, Registry& active, Equal equal) {
    std::size_t released = 0;
    for (const auto& id : owned) {
        const auto found = std::find_if(active.begin(), active.end(),
            [&](const auto& candidate) { return equal(id, candidate); });
        if (found != active.end()) {
            active.erase(found);
            ++released;
        }
    }
    owned.clear();
    return released;
}
} // namespace discogger_async
