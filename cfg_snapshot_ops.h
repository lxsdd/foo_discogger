#pragma once
// Detached snapshots for modern foobar2000 cfg_objList.
// Preserve historical GUIDs, ordering, and untouched IDs; persist only changed data.
#include <algorithm>
#include <cstddef>
#include <utility>
namespace discogger_cfg {
template<class List> void truncate(List& list, std::size_t count) {
    auto entries = list.get();
    if (entries.size() <= count) return;
    entries.resize(count);
    list.set(std::move(entries));
}
template<class List> bool remove_by_id(List& list, int id) {
    auto entries = list.get();
    const auto it = std::find_if(entries.begin(), entries.end(), [id](const auto& v) { return v.id == id; });
    if (it == entries.end()) return false;
    entries.erase(it);
    list.set(std::move(entries));
    return true;
}
template<class List, class Value> bool take_by_id(List& list, int id, Value& out) {
    auto entries = list.get();
    const auto it = std::find_if(entries.begin(), entries.end(), [id](const auto& v) { return v.id == id; });
    if (it == entries.end()) return false;
    out = *it;
    entries.erase(it);
    list.set(std::move(entries));
    return true;
}
template<class List, class Value> bool replace_by_id(List& list, int id, const Value& value) {
    auto entries = list.get();
    const auto it = std::find_if(entries.begin(), entries.end(), [id](const auto& v) { return v.id == id; });
    if (it == entries.end()) return false;
    *it = value;
    list.set(std::move(entries));
    return true;
}
}
