#include "../discogger_search_fallback.h"
#include "live-discogs-search-corpus-qa.h"
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
    // Independent, diverse public release references: test the SAME bounded
    // search-plan builder with eleven original queries, not just user reports.
    // These portable tests do not claim that Discogs live responses succeeded.
    std::vector<long long> corpus_ids;
    for (const auto& row : discogger_live_corpus_qa::cases) {
        assert(row.expected_release_id > 0);
        assert(std::string(row.encoded_search_params).find("per_page=100&page=1") != std::string::npos);
        assert(std::find(corpus_ids.begin(), corpus_ids.end(), row.expected_release_id) == corpus_ids.end());
        corpus_ids.push_back(row.expected_release_id);
        const auto plan = build_fallback_plan(row.artist, row.release_title, row.track_title);
        assert(!plan.empty() && plan.size() <= 2);
        assert(plan.back().q == without_terminal_mix_suffix(row.release_title) ||
               plan.back().q == without_terminal_mix_suffix(row.track_title));
        assert(plan.back().title.empty());
        const auto encoded = encode_fallback_query(plan.back());
        assert(!encoded.empty() && encoded.rfind("q=%22", 0) == 0);
        // Literal metadata separator characters must never become new filters.
        assert(encoded.find(" & title=") == std::string::npos);
    }
    assert(corpus_ids.size() == 14);
    assert(percent_encode_search_value("Björk – Jóga") ==
           "Bj%C3%B6rk%20%E2%80%93%20J%C3%B3ga");
    assert(percent_encode_search_value("Röyksopp") == "R%C3%B6yksopp");
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

    // Identical partner and title values do not make these requests
    // interchangeable: the first constrains Discogs release title and the
    // second only searches the broad text index.
    auto same_partner_title = build_fallback_plan(
        "Producer feat. Sunrise", "Sunrise", "");
    assert(same_partner_title.size() == 2);
    assert(same_partner_title[0].label == "collaboration partner and title");
    assert(same_partner_title[0].q == "Sunrise" &&
           same_partner_title[0].title == "Sunrise");
    assert(same_partner_title[1].label == "title-only discovery (review required)");
    assert(same_partner_title[1].q == "Sunrise" &&
           same_partner_title[1].title.empty());
    assert(encode_fallback_query(same_partner_title[0]) ==
           "q=%22Sunrise%22&title=Sunrise");
    assert(encode_fallback_query(same_partner_title[1]) ==
           "q=%22Sunrise%22");
    // Version suffix normalization can produce the same partner/title token:
    // both requests are still distinct and must never auto-confirm identity.
    auto same_partner_version = build_fallback_plan(
        "Producer x Sunrise", "Sunrise (Extended Mix)", "");
    assert(same_partner_version.size() == 2);
    assert(same_partner_version[0].title == "Sunrise (Extended Mix)");
    assert(same_partner_version[1].q == "Sunrise" &&
           same_partner_version[1].title.empty());
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
    // Complex live-GUI cases also have deterministic, review-only recovery
    // plans. These checks preserve Unicode/ampersands and release-vs-track
    // semantics without depending on an upstream network response.
    auto featured_ampersands = build_fallback_plan(
        "Above & Beyond feat. Richard Bedford", "Sun & Moon", "Sun & Moon (Club Mix)");
    assert(featured_ampersands.size() == 2);
    assert(featured_ampersands[0].q == "Richard Bedford");
    assert(featured_ampersands[0].title == "Sun & Moon");
    assert(encode_fallback_query(featured_ampersands[0]) ==
           "q=%22Richard%20Bedford%22&title=Sun%20%26%20Moon");
    assert(encode_fallback_query(featured_ampersands[1]) ==
           "q=%22Sun%20%26%20Moon%22");

    auto unicode_mix = build_fallback_plan("Björk", "Jóga", "Jóga (Howie B)");
    assert(unicode_mix.size() == 1);
    assert(encode_fallback_query(unicode_mix[0]) == "q=%22J%C3%B3ga%22");
    auto unicode_artist = build_fallback_plan("Röyksopp", "Eple", "Eple (Edit)");
    assert(unicode_artist.size() == 1 && unicode_artist[0].q == "Eple");
    auto distinct_release_track = build_fallback_plan("Bicep", "Glue EP", "Glue");
    assert(distinct_release_track.size() == 1);
    assert(distinct_release_track[0].q == "Glue EP" &&
           distinct_release_track[0].title.empty());
    auto dotted_title = build_fallback_plan("Underworld", "Born Slippy .NUXX", "Born Slippy / NUXX (Mix)");
    assert(dotted_title.size() == 1);
    assert(encode_fallback_query(dotted_title[0]) ==
           "q=%22Born%20Slippy%20.NUXX%22");
    assert(without_terminal_mix_suffix("Sun & Moon (Club Mix)") == "Sun & Moon");

    // A title containing query-like text must stay ONE literal title value,
    // not become a new Discogs filter or silently alter the original title.
    auto reserved = build_fallback_plan(
        "Kyau & Albert X Maywave", "1000 Years & title=Injected", "1000 Years (DJ Version)");
    assert(reserved.size() == 2);
    assert(encode_fallback_query(reserved[0]) ==
           "q=%22Maywave%22&title=1000%20Years%20%26%20title%3DInjected");
    assert(encode_fallback_query(reserved[1]) ==
           "q=%221000%20Years%20%26%20title%3DInjected%22");

    std::vector<std::string> ids;
    assert(!already_seen_release_id(ids, "36397609"));
    assert(already_seen_release_id(ids, "36397609"));
    assert(!already_seen_release_id(ids, "36819730"));
    assert(!already_seen_release_id(ids, "35534398"));
    assert(already_seen_release_id(ids, ""));
    assert(ids.size() == 3);
    return 0;
}
