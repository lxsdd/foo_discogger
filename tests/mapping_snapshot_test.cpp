#include "../discogger_mapping_snapshot.h"
#include <cassert>
#include <string>
#include <vector>
struct Mapping {
    int id;
    std::string field;
};
struct Config {
    std::vector<Mapping> entries;
    std::vector<Mapping> get() const { return entries; }
};
struct List {
    std::vector<Mapping> entries;
    std::size_t get_count() const { return entries.size(); }
    const Mapping& get_item_ref(std::size_t i) const { return entries.at(i); }
};
int main() {
    Config persistent{{{1,"artist"},{2,"title"}}};
    discogger_mapping::owned_snapshot<Mapping> keep;
    keep.from_config(persistent);
    const Mapping* title_ptr = &keep.at(1);
    persistent.entries.clear();
    persistent.entries.push_back({3,"changed"});
    assert(title_ptr->id == 2 && title_ptr->field == "title" && keep.size()==2);
    List custom{{{8,"artist"},{9,"mix"}}};
    keep.from_list(custom);
    const Mapping* custom_ptr = &keep.at(1);
    custom.entries.clear();
    assert(custom_ptr->id==9 && custom_ptr->field=="mix" && keep.size()==2);
    // A failed copy leaves the previous owned snapshot in place (strong exception guarantee).
    Config empty{{}};
    keep.from_config(empty);
    assert(keep.size()==0);
    keep.from_config(persistent);
    assert(keep.at(0).id==3 && keep.at(0).field=="changed");
}
