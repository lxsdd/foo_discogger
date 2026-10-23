#include "../discogger_search_fallback.h"
#include <cassert>
#include <string>
#include <vector>
int main() {
    using namespace discogger_search;
    assert(trailing_collaborator("Kyau & Albert X Maywave") == "Maywave");
    assert(trailing_collaborator("Kyau & Albert x Maywave") == "Maywave");
    // Ampersand collaborations are bounded discovery hints, not safe tag matches.
    assert(trailing_collaborator("Artist & Guest") == "Guest");
    assert(trailing_collaborator("Artist & ") == "");
    assert(trailing_collaborator("Kyau & Albert") == "Albert"); // duo: review required
    assert(trailing_collaborator("Kyau & Albert x Maywave") == "Maywave");
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
        {"35534398", "Quivver", "Consciousness EP"}
    };
    for (const auto& example : fixture) {
        const auto plan = build_fallback_plan(example.artist, example.title, "");
        assert(!plan.empty() && plan.size() <= 2);
        assert(plan.back().q == example.title && plan.back().title.empty());
        assert(std::string(example.id).size() >= 8);
    }
    // Encode untrusted UTF-8 values before composing fixed query fields:
    // a literal " & title=" must never become an injected filter.
    assert(encode_fallback_query({"partner", "Maywave", "1000 Years"}) ==
           "q=%22Maywave%22&title=1000%20Years");
    assert(encode_fallback_query({"title", "Only You", ""}) ==
           "q=%22Only%20You%22");
    assert(encode_fallback_query({"injection", "Guest & title=Injected", "True & artist=Other"}) ==
           "q=%22Guest%20%26%20title%3DInjected%22&title=True%20%26%20artist%3DOther");
    assert(encode_fallback_query({"symbols", "Don't + Stop #1", "A/B? C%D"}) ==
           "q=%22Don%27t%20%2B%20Stop%20%231%22&title=A%2FB%3F%20C%25D");
    assert(percent_encode_search_value("Ä") == "%C3%84");
    assert(encode_fallback_query({"empty", "", "Do Not Search"}).empty());
    assert(encode_fallback_query({"q-only", "Via Balearia", ""}) ==
           "q=%22Via%20Balearia%22");
    auto collab = build_fallback_plan("Kyau & Albert X Maywave", "1000 Years", "1000 Years (DJ Version)");
    assert(collab.size() == 2);
    assert(collab[0].q == "Maywave" && collab[0].title == "1000 Years");
    assert(collab[1].q == "1000 Years" && collab[1].title.empty());
    auto ampersand = build_fallback_plan("Producer & Vocalist", "A New Day", "");
    assert(ampersand.size() == 2);
    assert(ampersand[0].q == "Vocalist" && ampersand[0].title == "A New Day");
    assert(ampersand[1].q == "A New Day" && ampersand[1].title.empty());
    assert(encode_fallback_query(ampersand[0]) ==
           "q=%22Vocalist%22&title=A%20New%20Day");
    // A permanent duo is never silently accepted as a match:
    // these remain bounded, manual-review fallback candidates.
    auto duo = build_fallback_plan("Kyau & Albert", "1000 Years", "");
    assert(duo.size() == 2 && duo[0].q == "Albert");
    // Short/generic release titles must not suppress a useful track search.
    // In particular, the Discogs title= filter refers to the RELEASE title,
    // not to the supplied track title. Such fallbacks must remain review-only.
    auto short_release = build_fallback_plan(
        "Kyau & Albert x Maywave", "EP", "1000 Years (DJ Version)");
    assert(short_release.size() == 1);
    assert(short_release[0].q == "1000 Years" && short_release[0].title.empty());
    assert(short_release[0].label == "track-title discovery (review required)");
    assert(encode_fallback_query(short_release[0]) == "q=%221000%20Years%22");
    auto track_only = build_fallback_plan("Artist feat. Guest", "", "Only You");
    assert(track_only.size() == 1 && track_only[0].q == "Only You");
    assert(track_only[0].title.empty());
    assert(build_fallback_plan("Artist", "EP", "X").empty());
    // A substantive release title retains the existing partner + title plan.
    auto normal_release = build_fallback_plan(
        "Kyau & Albert x Maywave", "1000 Years", "1000 Years (DJ Version)");
    assert(normal_release.size() == 2 && normal_release[0].title == "1000 Years");
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
