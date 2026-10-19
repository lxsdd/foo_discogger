#include "../tag_deletion_preflight.h"
#include <cassert>
#include <cctype>
#include <cstdint>
#include <string>
#include <vector>
#include <utility>
#include <algorithm>
struct Meta {
    std::vector<std::pair<std::string, std::vector<std::string>>> data;
    std::size_t meta_find(const char* name) const {
        auto fold=[](unsigned char c){return std::tolower(c);};
        for(std::size_t i=0;i<data.size();++i) {
            const auto& tag=data[i].first;
            if(tag.size()!=std::string(name).size()) continue;
            if(std::equal(tag.begin(),tag.end(),name,[&](char a,char b){return fold(a)==fold(b);}))return i;
        }
        return SIZE_MAX;
    }
    std::size_t meta_get_count() const{return data.size();}
    const char* meta_enum_name(std::size_t i) const{return data[i].first.c_str();}
    std::size_t meta_enum_value_count(std::size_t i) const{return data[i].second.size();}
    const char* meta_enum_value(std::size_t i,std::size_t j) const{return data[i].second[j].c_str();}
};
struct ApprovalMask {
    std::vector<bool> data;
    std::size_t size() const { return data.size(); }
    std::size_t find_first(bool desired, std::size_t start, std::size_t n) const {
        for (std::size_t i = start; i < n; ++i)
            if (data[i] == desired) return i;
        return n;
    }
};
int main() {
    using discogger_write_safety::has_any_user_approval;
    assert(!has_any_user_approval(ApprovalMask{{}}));
    assert(!has_any_user_approval(ApprovalMask{{false,false,false}}));
    assert(has_any_user_approval(ApprovalMask{{true,false,false}}));
    assert(has_any_user_approval(ApprovalMask{{false,false,true}}));

    using discogger_write_safety::inspect_tag_deletions;
    const Meta old{{{"ARTIST",{"A"}},{"GENRE",{"Trance","Progressive"}},{"CUESHEET",{"FILE x.mp3 MP3\nTRACK 01 AUDIO"}},{"CUSTOM",{"keep"}}}};
    std::vector<std::string> names;
    auto inspect=[&](const Meta& after){names.clear();return inspect_tag_deletions(old,after,[&](const char* name){names.emplace_back(name);});};
    auto r=inspect(old);
    assert(r.removed_fields==0 && r.removed_values==0 && !r.cuesheet_modified && names.empty());
    Meta next=old; next.data[0].second={"renamed"};
    r=inspect(next);
    assert(r.removed_fields==0 && r.removed_values==0 && !r.cuesheet_modified);
    next=old; next.data.pop_back();
    r=inspect(next);
    assert(r.removed_fields==1 && r.removed_values==0 && names==std::vector<std::string>{"CUSTOM"});
    next=old; next.data[1].second.pop_back();
    r=inspect(next);
    assert(r.removed_fields==0 && r.removed_values==1 && names==std::vector<std::string>{"GENRE"});
    next=old; next.data[2].second={"TRACK 02 AUDIO"};
    assert(inspect(next).cuesheet_modified);
    next=old; next.data.erase(next.data.begin()+2);
    assert(inspect(next).cuesheet_modified);
    next=old; next.data[2].second.push_back("TRACK 03 AUDIO");
    assert(inspect(next).cuesheet_modified);
    next=old; next.data[2].first="CueSheet";
    assert(!inspect(next).cuesheet_modified);
    next=old; next.data[2].second.clear();
    assert(inspect(next).cuesheet_modified);
    const Meta empty{};
    assert(inspect(empty).removed_fields==4 && inspect(empty).cuesheet_modified);
    auto from_empty=inspect_tag_deletions(empty,old,[&](const char*){assert(false);});
    assert(from_empty.cuesheet_modified && from_empty.removed_fields==0);
}
