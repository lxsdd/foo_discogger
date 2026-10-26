#pragma once
// Own mappings while tag results retain pointers to their entries.
// Taking a detached cfg_objList snapshot prevents pointers into ephemeral config data.
#include <cstddef>
#include <utility>
#include <vector>

namespace discogger_mapping {
template<class Entry>
class owned_snapshot {
    std::vector<Entry> entries_;
public:
    template<class Config>
    void from_config(Config& config) {
        auto replacement = config.get();
        entries_.swap(replacement);
    }
    template<class List>
    void from_list(const List& mappings) {
        std::vector<Entry> replacement;
        replacement.reserve(mappings.get_count());
        for (std::size_t i = 0; i < mappings.get_count(); ++i) {
            replacement.push_back(mappings.get_item_ref(i));
        }
        entries_.swap(replacement);
    }
    std::size_t size() const noexcept { return entries_.size(); }
    const Entry& at(std::size_t index) const { return entries_.at(index); }
};
} // namespace discogger_mapping
