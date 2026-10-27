#include "../discogger_artist_search_pages.h"
#include <cassert>
#include <string>

int main() {
    using namespace discogger_search;
    std::vector<artist_search_hit> hits;
    std::unordered_set<std::string> seen;
    assert(stage_artist_search_hit(hits, seen, "38012673", "Estiva"));
    assert(stage_artist_search_hit(hits, seen, "37332711", "Kyau & Albert × Maywave"));
    assert(stage_artist_search_hit(hits, seen, "36397609", "Matt Fax"));
    assert(stage_artist_search_hit(hits, seen, "36819730", "Matt Fax"));
    assert(stage_artist_search_hit(hits, seen, "35534398", "Quivver"));
    assert(hits.size() == 5 && seen.size() == 5);
    assert(hits[1].title == "Kyau & Albert × Maywave");
    assert(!stage_artist_search_hit(hits, seen, "38012673", "duplicate"));
    for (auto id : {"", "0", "-1", "+1", "123abc", "2147483648",
                    "999999999999999999999999"})
        assert(!stage_artist_search_hit(hits, seen, id, "invalid"));
    assert(!stage_artist_search_hit(hits, seen, "123", ""));
    assert(hits.size() == 5 && seen.size() == 5);
    assert(continue_artist_search(1, 8, false));
    assert(continue_artist_search(4, 8, false));
    assert(!continue_artist_search(5, 8, false));
    assert(!continue_artist_search(1, 8, true));
    assert(!continue_artist_search(1, 1, false));
    assert(!continue_artist_search(0, 2, false));
    assert(!continue_artist_search(6, 8, false));
    for (std::size_t i = hits.size(); i < max_artist_search_hits; ++i)
        assert(stage_artist_search_hit(hits, seen, std::to_string(i + 100), "artist"));
    assert(hits.size() == max_artist_search_hits);
    assert(!stage_artist_search_hit(hits, seen, "99999", "over cap"));
    assert(seen.size() == max_artist_search_hits);
}
