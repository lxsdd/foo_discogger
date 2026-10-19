#include "../cfg_snapshot_ops.h"
#include <cassert>
#include <string>
#include <vector>
#include <utility>
template<class T> struct ConfigList {
    std::vector<T> entries;
    int saves = 0;
    std::vector<T> get() const { return entries; }
    void set(std::vector<T> v) { entries = std::move(v); ++saves; }
};
struct Entry { int id; std::string value; };
int main() {
    ConfigList<Entry> cfg{{{10,"a"},{20,"b"},{30,"c"}},0};
    discogger_cfg::truncate(cfg, 4);
    assert(cfg.saves == 0);
    discogger_cfg::truncate(cfg, 2);
    assert(cfg.saves == 1 && cfg.entries.size() == 2 && cfg.entries[1].id == 20);
    assert(!discogger_cfg::remove_by_id(cfg, 99) && cfg.saves == 1);
    assert(discogger_cfg::remove_by_id(cfg, 10));
    assert(cfg.saves == 2 && cfg.entries.size() == 1 && cfg.entries[0].id == 20);
    Entry old{-1,"untouched"};
    assert(!discogger_cfg::take_by_id(cfg, 77, old));
    assert(old.id == -1 && old.value == "untouched" && cfg.saves == 2);
    assert(discogger_cfg::take_by_id(cfg, 20, old));
    assert(old.id == 20 && old.value == "b" && cfg.entries.empty() && cfg.saves == 3);
    assert(!discogger_cfg::replace_by_id(cfg, 20, Entry{20,"new"}));
    assert(cfg.saves == 3);
    cfg.entries = {{5,"first"},{7,"second"},{9,"third"}};
    assert(discogger_cfg::replace_by_id(cfg, 7, Entry{7,"updated"}));
    assert(cfg.saves == 4 && cfg.entries[0].id == 5 && cfg.entries[1].value == "updated" && cfg.entries[2].id == 9);
    ConfigList<Entry> legacy{{{101,"1"},{103,"0"}},0};
    int flags = 0;
    Entry value{-1,""};
    if (discogger_cfg::take_by_id(legacy, 101, value) && value.value == "1") flags |= 1;
    if (discogger_cfg::take_by_id(legacy, 102, value) && value.value == "1") flags |= 2;
    if (discogger_cfg::take_by_id(legacy, 103, value) && value.value == "1") flags |= 4;
    assert(flags == 1 && legacy.entries.empty() && legacy.saves == 2);
    ConfigList<Entry> untouched{{{1,"alpha"},{2,"beta"}},0};
    assert(!discogger_cfg::replace_by_id(untouched, 404, Entry{404,"wrong"}));
    assert(untouched.saves == 0 && untouched.entries[1].value == "beta");
    auto staged = untouched.get();
    staged.push_back({3,"new"});
    assert(untouched.saves == 0 && untouched.entries.size() == 2);
    untouched.set(std::move(staged));
    assert(untouched.saves == 1 && untouched.entries.size() == 3);
}
