#pragma once
#include <algorithm>
#include <cstddef>
#include <vector>

// Call with the shared active-artist registry mutex held. The caller-owned
// vector records exactly which markers this task inserted, not every artist
// it searched. Consume that ownership after a completion/cancel/failure so
// duplicate callbacks cannot erase a later task's registration.
namespace discogger_async {
// Preserve original selection order, but process an ID at most once per task.
template <typename Id>
std::vector<Id> unique_artist_ids(const std::vector<Id>& ids) {
    std::vector<Id> result;
    result.reserve(ids.size());
    for (const auto& id : ids)
        if (std::find(result.begin(), result.end(), id) == result.end()) result.push_back(id);
    return result;
}

// Only the worker owning the registry ID may fetch its releases. Another
// concurrent worker can still display the cached artist profile.
template <typename Owned, typename Id, typename Equal>
bool owns_id(const Owned& owned, const Id& id, Equal equal) {
    return std::find_if(owned.begin(), owned.end(),
        [&](const auto& candidate) { return equal(candidate, id); }) != owned.end();
}
inline bool should_load_owned_releases(bool requested, bool owned) noexcept {
    return requested && owned;
}

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
