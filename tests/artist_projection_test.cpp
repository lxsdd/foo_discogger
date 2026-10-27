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
    return 0;
}
