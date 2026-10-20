#include "../discogger_search_fallback.h"
#include <cassert>
#include <string>
#include <vector>
int main() {
    using namespace discogger_search;
    assert(trailing_collaborator("Kyau & Albert X Maywave") == "Maywave");
    assert(trailing_collaborator("Kyau & Albert x Maywave") == "Maywave");
    assert(trailing_collaborator("Kyau & Albert") == "");
    assert(trailing_collaborator("Maywave x Kyau & Albert") == "Kyau & Albert");
    assert(trailing_collaborator("Matt Fax feat. Lovlee") == "Lovlee");
    assert(trailing_collaborator("Artist ft. Guest") == "Guest");
    assert(trailing_collaborator("Quivver") == "");
    assert(trailing_collaborator("Artist VS. Guest") == "Guest");
    assert(trailing_collaborator("Artist x   ") == "");
    assert(without_terminal_mix_suffix("1000 Years (DJ Version)") == "1000 Years");
    assert(without_terminal_mix_suffix("Only You (Extended Mix)") == "Only You");
    assert(without_terminal_mix_suffix("1000 Years (Live)") == "1000 Years (Live)");
    assert(without_terminal_mix_suffix("Version") == "Version");

    // Known user-facing misses: these are deterministic SEARCH-PLAN
    // regressions only, not unverified claims of live API/host availability.
    struct known { const char* id; const char* artist; const char* title; };
    const known fixture[] = {
        {"38012673", "Estiva", "Via Balearia"},
        {"37332711", "Kyau & Albert x Maywave", "1000 Years"},
        {"36397609", "Matt Fax", "Only You"},
        {"36819730", "Matt Fax", "Till You Let Go"},
        {"35534398", "Synthetic Artist", "Synthetic Case Five"}
    };
    for (const auto& example : fixture) {
        const auto plan = build_fallback_plan(example.artist, example.title, "");
        assert(!plan.empty() && plan.size() <= 2);
        assert(plan.back().q == example.title && plan.back().title.empty());
        assert(std::string(example.id).size() >= 8);
    }
    auto collab = build_fallback_plan("Kyau & Albert X Maywave", "1000 Years", "1000 Years (DJ Version)");
    assert(collab.size() == 2);
    assert(collab[0].q == "Maywave" && collab[0].title == "1000 Years");
    assert(collab[1].q == "1000 Years" && collab[1].title.empty());
    auto mix = build_fallback_plan("Matt Fax", "Only You (Extended Mix)", "");
    assert(mix.size() == 1 && mix[0].q == "Only You");
    assert(build_fallback_plan("", "Only You", "").empty());
    assert(build_fallback_plan("Matt Fax", "X", "").empty());
    assert(build_fallback_plan("Matt Fax", "", "Till You Let Go").size() == 1);
    std::vector<std::string> ids;
    assert(!already_seen_release_id(ids, "36397609"));
    assert(already_seen_release_id(ids, "36397609"));
    assert(!already_seen_release_id(ids, "36819730"));
    assert(!already_seen_release_id(ids, "35534398"));
    assert(already_seen_release_id(ids, ""));
    assert(ids.size() == 3);
    return 0;
}
