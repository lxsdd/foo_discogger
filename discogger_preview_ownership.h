#pragma once
#include <cstddef>
#include <vector>
#include <map>
#include <utility>
#include <cstdint>

namespace discogger_ui {
// Per-artwork-slot request versions distinguish a repeated refresh of the
// same release/files from an older asynchronous completion.
class preview_request_versions {
public:
    std::uint64_t next(std::size_t index, bool artist) {
        return ++m_versions[std::make_pair(index, artist)];
    }
    bool matches(std::size_t index, bool artist, std::uint64_t version) const {
        const auto it = m_versions.find(std::make_pair(index, artist));
        return version != 0 && it != m_versions.end() && it->second == version;
    }
private:
    std::map<std::pair<std::size_t, bool>, std::uint64_t> m_versions;
};

template <typename Items>
auto preview_item_count(const Items& items) -> decltype(items.get_count()) { return items.get_count(); }
template <typename T, typename A>
std::size_t preview_item_count(const std::vector<T, A>& items) { return items.size(); }

// Compare the source captured when a preview worker was started with the
// current original-dialog source before applying any result. File identity,
// not just selection length, matters for local artwork.
template <typename Release, typename Items>
bool preview_source_matches(const Release& requested_release,
                            const Release& current_release,
                            const Items* requested_items,
                            const Items* current_items) {
    if (!requested_release || requested_release != current_release) return false;
    if ((requested_items == nullptr) != (current_items == nullptr)) return false;
    if (!requested_items) return true;
    if (preview_item_count(*requested_items) != preview_item_count(*current_items)) return false;
    for (std::size_t i = 0; i < preview_item_count(*requested_items); ++i) {
        if ((*requested_items)[i] != (*current_items)[i]) return false;
    }
    return true;
}
} // namespace discogger_ui
