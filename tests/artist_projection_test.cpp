#include "../discogger_artist_projection.h"
#include <cassert>
#include <string>
#include <unordered_set>

int main() {
    using namespace discogger_search;
    assert(artist_preview_identity("101") == "101");
    assert(artist_preview_identity("000101") == "101");
    assert(artist_preview_identity("000000000000101") == "101");
    assert(artist_preview_identity("0") == "0"); // existing aggregate preview
    assert(artist_preview_identity("0000").empty());
    for (const char* bad : {"", "abc", "1a", " 101", "+101", "-1",
                            "2147483648", "999999999999999999999"}) {
        assert(artist_preview_identity(bad).empty());
        assert(!same_artist_preview_identity("101", bad));
    }
    assert(same_artist_preview_identity("101", "000101"));
    assert(same_artist_preview_identity("0", "0"));
    assert(!same_artist_preview_identity("0", "0000"));
    assert(!same_artist_preview_identity("101", "102"));
    assert(!same_artist_preview_identity("", ""));
    assert(artist_preview_identity("00038012673") == "38012673");
    assert(artist_preview_identity("00037332711") == "37332711");
    assert(artist_preview_identity("00036397609") == "36397609");
    assert(artist_preview_identity("00036819730") == "36819730");
    assert(artist_preview_identity("00035534398") == "35534398");

    // Release/master are distinct types, even when numeric IDs coincide.
    std::unordered_set<std::string> releases, masters;
    assert(insert_preview_release_identity(releases, "00038012673"));
    assert(!insert_preview_release_identity(releases, "38012673"));
    assert(insert_preview_release_identity(masters, "38012673"));
    assert(!insert_preview_release_identity(masters, "00038012673"));
    assert(insert_preview_release_identity(releases, "000410003"));
    assert(insert_preview_release_identity(masters, "000410004"));
    for (const char* bad : {"", "0", "0000", "bad", "-1", "2147483648"}) {
        assert(!insert_preview_release_identity(releases, bad));
        assert(!insert_preview_release_identity(masters, bad));
    }
    assert(releases.size() == 2 && masters.size() == 2);
    assert(releases.count("38012673") == 1 && masters.count("38012673") == 1);
    assert(releases.count("410003") == 1 && masters.count("410004") == 1);
    using preview = advanced_preview_action;
    assert(decide_advanced_preview_action(false, false, false, false, false) == preview::keep);
    assert(decide_advanced_preview_action(false, true, true, false, true) == preview::keep);
    assert(decide_advanced_preview_action(true, false, false, false, false) == preview::clear);
    assert(decide_advanced_preview_action(true, false, true, true, true) == preview::show);
    assert(decide_advanced_preview_action(true, false, true, true, false) == preview::clear);
    assert(decide_advanced_preview_action(true, true, false, false, false) == preview::keep);
    assert(decide_advanced_preview_action(true, true, true, true, true) == preview::keep);
    assert(decide_advanced_preview_action(true, true, true, false, false) == preview::clear);
    assert(decide_advanced_preview_action(true, true, true, false, true) == preview::show);
    return 0;
}
