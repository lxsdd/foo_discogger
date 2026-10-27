#include "../discogger_search_fallback.h"
#include "../discogger_search_transaction.h"
#include <cassert>
#include <stdexcept>
#include <string>
#include <string_view>
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
    assert(primary_artist_before_featuring("Above & Beyond feat. Richard Bedford") == "Above & Beyond");
    assert(primary_artist_before_featuring("Above & Beyond FEAT Richard Bedford") == "Above & Beyond");
    assert(primary_artist_before_featuring("Artist ft. Guest") == "Artist");
    assert(primary_artist_before_featuring("Artist featuring Singer") == "Artist");
    assert(primary_artist_before_featuring("Producer x Guest").empty());
    assert(primary_artist_before_featuring("Kyau & Albert").empty());
    assert(primary_artist_before_explicit_collaboration("Kyau & Albert × Maywave") == "Kyau & Albert");
    assert(primary_artist_before_explicit_collaboration("Maywave x Kyau & Albert") == "Maywave");
    assert(primary_artist_before_explicit_collaboration("Artist VS. Guest") == "Artist");
    assert(primary_artist_before_explicit_collaboration("Artist versus Guest") == "Artist");
    assert(primary_artist_before_explicit_collaboration("X Japan").empty());
    assert(primary_artist_before_explicit_collaboration("Kyau & Albert").empty());
    assert(primary_artist_before_explicit_collaboration("Artist x   ").empty());
    assert(release_discovery_artist("Kyau & Albert x Maywave feat. Singer") == "Kyau & Albert");
    assert(release_discovery_artist("Kyau & Albert") == "Kyau & Albert");
    assert(release_discovery_artist("Above & Beyond feat. Richard Bedford") == "Above & Beyond");
    assert(without_terminal_mix_suffix("1000 Years (DJ Version)") == "1000 Years");
    assert(without_terminal_mix_suffix("Only You (Extended Mix)") == "Only You");
    assert(without_terminal_mix_suffix("1000 Years (Live)") == "1000 Years (Live)");
    assert(without_terminal_mix_suffix("Only You (John Smith Remix)") == "Only You");
    assert(without_terminal_mix_suffix("Till You Let Go (2026 Remaster)") == "Till You Let Go");
    assert(without_terminal_mix_suffix("Consciousness EP [Deluxe Edition]") == "Consciousness EP");
    assert(without_terminal_mix_suffix("Only You - Extended Mix") == "Only You");
    assert(without_terminal_mix_suffix("Only You (Live at Home)") == "Only You (Live at Home)");
    assert(plausible_release_title("Only You (John Smith Remix)", "Matt Fax - Only You"));
    assert(plausible_release_title("Till You Let Go (2026 Remaster)", "Till You Let Go"));
    auto edition = build_fallback_plan("Quivver", "Consciousness EP [Deluxe Edition]", "");
    assert(edition.size() == 3 && edition[0].artist_filter == "Quivver" &&
           edition[0].release_title_filter == "Consciousness EP [Deluxe Edition]" &&
           edition[1].artist_filter == "Quivver" &&
           edition[1].release_title_filter == "Consciousness EP" &&
           edition[2].q == "Consciousness EP");
    auto named_remix = build_fallback_plan("Matt Fax", "Only You (John Smith Remix)", "");
    assert(named_remix.size() == 3 && named_remix[0].artist_filter == "Matt Fax" &&
           named_remix[0].release_title_filter == "Only You (John Smith Remix)" &&
           named_remix[1].artist_filter == "Matt Fax" &&
           named_remix[1].release_title_filter == "Only You" &&
           named_remix[2].q == "Only You");
    assert(encode_fallback_query(named_remix[1]) ==
           "artist=Matt%20Fax&release_title=Only%20You&type=release");
    // Non-version subtitles are not silently stripped or queried as a base.
    const auto live_title = build_fallback_plan("Matt Fax", "Only You (Live)", "");
    assert(live_title.size() == 2 && live_title[0].release_title_filter == "Only You (Live)" &&
           live_title[1].q == "Only You (Live)");
    assert(without_terminal_mix_suffix("Version") == "Version");

    // Known user-facing misses: these are deterministic SEARCH-PLAN
    // regressions only, not unverified claims of live API/host availability.
    struct known { const char* id; const char* artist; const char* title; const char* indexed_artist; };
    const known fixture[] = {
        {"38012673", "Estiva", "Via Balearia", "Estiva"},
        {"37332711", "Kyau & Albert x Maywave", "1000 Years", "Kyau & Albert"},
        {"36397609", "Matt Fax", "Only You", "Matt Fax"},
        {"36819730", "Matt Fax", "Till You Let Go", "Matt Fax"},
        {"35534398", "Quivver", "Consciousness EP", "Quivver"}
    };
    for (const auto& example : fixture) {
        const auto plan = build_fallback_plan(example.artist, example.title, "");
        assert(plan.size() >= 2 && plan.size() <= 3);
        assert(plan.front().artist_filter == example.indexed_artist &&
               plan.front().release_title_filter == example.title);
        assert(encode_fallback_query(plan.front()).find("&release_title=") != std::string::npos);
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
    assert(encode_fallback_query({"featured", "", "", "Above & Beyond", "Sun & Moon"}) ==
           "artist=Above%20%26%20Beyond&release_title=Sun%20%26%20Moon&type=release");
    assert(encode_fallback_query({"bad-mixed", "Guest", "", "Above & Beyond", "Sun & Moon"}).empty());
    assert(encode_fallback_query({"bad-empty-title", "", "", "Above & Beyond", ""}).empty());
    assert(encode_fallback_query({"bad-mixed-title", "", "Sun & Moon", "Above & Beyond", "Sun & Moon"}).empty());
    assert(encode_fallback_query({"bad-release-title", "Sun & Moon", "", "", "Injected"}).empty());
    assert(encode_fallback_query({"filtered-input", "", "",
           "Producer & release_title=Attacker", "C & type=master"}) ==
           "artist=Producer%20%26%20release_title%3DAttacker&release_title=C%20%26%20type%3Dmaster&type=release");
    assert(encode_fallback_query({"q-only", "Via Balearia", ""}) ==
           "q=%22Via%20Balearia%22");
    auto collab = build_fallback_plan("Kyau & Albert X Maywave", "1000 Years", "1000 Years (DJ Version)");
    assert(collab.size() == 3 && collab[0].artist_filter == "Kyau & Albert");
    assert(collab[1].q == "Maywave" && collab[1].title == "1000 Years");
    assert(collab[2].q == "1000 Years" && collab[2].title.empty());
    auto ampersand = build_fallback_plan("Producer & Vocalist", "A New Day", "");
    assert(ampersand.size() == 3 && ampersand[0].artist_filter == "Producer & Vocalist");
    assert(ampersand[1].q == "Vocalist" && ampersand[1].title == "A New Day");
    assert(ampersand[2].q == "A New Day" && ampersand[2].title.empty());
    assert(encode_fallback_query(ampersand[1]) ==
           "q=%22Vocalist%22&title=A%20New%20Day");
    // A permanent duo is never silently accepted as a match:
    // these remain bounded, manual-review fallback candidates.
    auto duo = build_fallback_plan("Kyau & Albert", "1000 Years", "");
    assert(duo.size() == 3 && duo[0].artist_filter == "Kyau & Albert" && duo[1].q == "Albert");

    // Identical partner and title values do not make these requests
    // interchangeable: prefer the proven featured-primary release-only
    // search, then guest/title, then the broad review-only text index.
    auto same_partner_title = build_fallback_plan(
        "Producer feat. Sunrise", "Sunrise", "");
    assert(same_partner_title.size() == 3);
    assert(same_partner_title[0].artist_filter == "Producer" &&
           same_partner_title[0].release_title_filter == "Sunrise" &&
           same_partner_title[0].q.empty());
    assert(same_partner_title[1].label == "collaboration partner and title");
    assert(same_partner_title[1].q == "Sunrise" &&
           same_partner_title[1].title == "Sunrise");
    assert(same_partner_title[2].label == "title-only discovery (review required)");
    assert(same_partner_title[2].q == "Sunrise" &&
           same_partner_title[2].title.empty());
    assert(encode_fallback_query(same_partner_title[0]) ==
           "artist=Producer&release_title=Sunrise&type=release");
    assert(encode_fallback_query(same_partner_title[1]) ==
           "q=%22Sunrise%22&title=Sunrise");
    assert(encode_fallback_query(same_partner_title[2]) ==
           "q=%22Sunrise%22");
    // Version suffix normalization can produce the same partner/title token:
    // both requests are still distinct and must never auto-confirm identity.
    auto same_partner_version = build_fallback_plan(
        "Producer x Sunrise", "Sunrise (Extended Mix)", "");
    // An "x" collaboration contributes a review-only first-artist search
    // hint, never a canonical artist identity or automatic tag match.
    assert(same_partner_version.size() == 3 && same_partner_version[0].artist_filter == "Producer");
    assert(same_partner_version[1].title == "Sunrise (Extended Mix)");
    assert(same_partner_version[2].q == "Sunrise" &&
           same_partner_version[2].title.empty());
    assert(same_partner_version[2].artist_filter.empty());
    // Short/generic release titles must not suppress a useful track search.
    // In particular, the Discogs title= filter refers to the RELEASE title,
    // not to the supplied track title. Such fallbacks must remain review-only.
    // Authenticated 2026-10-25 API result: bare "Sun & Moon" did not
    // contain release 2783977 on pages 1 or 2; primary artist plus
    // release_title and type=release found it immediately on page 1.
    const auto sun_moon = build_fallback_plan(
        "Above & Beyond feat. Richard Bedford", "Sun & Moon", "Sun & Moon (Club Mix)");
    assert(sun_moon.size() == 3);
    assert(sun_moon[0].artist_filter == "Above & Beyond" &&
           sun_moon[0].release_title_filter == "Sun & Moon");
    assert(encode_fallback_query(sun_moon[0]) ==
           "artist=Above%20%26%20Beyond&release_title=Sun%20%26%20Moon&type=release");
    assert(sun_moon[1].q == "Richard Bedford" && sun_moon[1].title == "Sun & Moon");
    assert(sun_moon[2].q == "Sun & Moon" && sun_moon[2].title.empty());
    // Do not confuse a TRACK name with a release title for this filter.
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
    assert(normal_release.size() == 3 && normal_release[0].artist_filter == "Kyau & Albert" && normal_release[1].title == "1000 Years");
    auto mix = build_fallback_plan("Matt Fax", "Only You (Extended Mix)", "");
    assert(mix.size() == 3 && mix[0].artist_filter == "Matt Fax" &&
           mix[1].artist_filter == "Matt Fax" &&
           mix[1].release_title_filter == "Only You" &&
           mix[2].q == "Only You");
    assert(build_fallback_plan("", "Only You", "").empty());
    assert(build_fallback_plan("Matt Fax", "X", "").empty());
    assert(build_fallback_plan("Matt Fax", "", "Till You Let Go").size() == 1);
    // Nonempty but irrelevant original results must permit bounded discovery;
    // a plausible release-title hit suppresses extra API requests.
    assert(plausible_release_title("Sun & Moon", "Above & Beyond - Sun & Moon"));
    assert(plausible_release_title("1000 Years", "Kyau & Albert x Maywave - 1000 Years (DJ Version)"));
    assert(plausible_release_title("Only You (Extended Mix)", "Matt Fax - Only You"));
    assert(plausible_release_title("Till You Let Go", "Matt Fax - Till You Let Go"));
    assert(!plausible_release_title("Sun & Moon", "Above & Beyond - Sun & Moonlight"));
    assert(!plausible_release_title("1000 Years", "Kyau & Albert - 2000 Years"));
    assert(!plausible_release_title("Only You", "Artist - Only Yourself"));
    assert(!plausible_release_title("EP", "Artist - EP"));
    assert(!plausible_release_title("", "Artist - Only You"));
    assert(!plausible_release_title("Only You", ""));

    // A partial whole-word title is not a valid reason to skip fallback.
    // Exact editions and named remixes must remain distinct discovery hints.
    assert(!plausible_release_title("Only You", "Matt Fax - Only You & Me"));
    assert(!plausible_release_title("Only You", "Matt Fax - I Love Only You"));
    assert(!plausible_release_title("Only You", "Matt Fax - I Love — Only You"));
    assert(!plausible_release_title("Only You", "Matt Fax - Only You (Live)"));
    assert(!plausible_release_title("Only You (Extended Mix)",
        "Matt Fax - Only You (Radio Edit)"));
    assert(!plausible_release_title("Only You (John Smith Remix)",
        "Matt Fax - Only You (Jane Doe Remix)"));
    assert(plausible_release_title("Only You (Extended Mix)",
        "Matt Fax — Only You – Extended Mix"));
    assert(plausible_release_title("Only You (Extended Mix)",
        "Matt Fax - Only You")); // unsuffixed master is still review-only
    assert(plausible_release_title("Only You", "Matt Fax - Only You (Extended Mix)"));
    assert(plausible_release_title("Café – L’Été",
        "Estiva — Cafe - L'Ete (Extended Remix)"));
    assert(!plausible_original_release_title("Matt Fax", "Only You",
        "Matt Fax - Only You & Me"));
    assert(!plausible_original_release_title("Matt Fax", "Only You (Extended Mix)",
        "Matt Fax - Only You (Radio Edit)"));
    assert(plausible_original_release_title("Matt Fax", "Only You (Extended Mix)",
        "Matt Fax - Only You (Extended Mix)"));
    const auto child_title = [](std::string_view value) { return value; };
    assert(!plausible_original_release_group_title("Matt Fax", "Only You (Extended Mix)",
        "Matt Fax - Only You (Radio Edit)",
        std::vector<std::string_view>{"Matt Fax - Only You (Radio Edit)"}, child_title));
    assert(plausible_original_release_group_title("Matt Fax", "Only You (Extended Mix)",
        "Matt Fax - Only You (Radio Edit)",
        std::vector<std::string_view>{"Matt Fax - Only You (Extended Mix)"}, child_title));


    // Original Discogs results may group a requested version below a
    // compilation/master with a different title. The matching child is a
    // plausible original hit and must not trigger extra fallback requests.
    const std::vector<std::string_view> grouped_versions = {
        "Unrelated Mix", "A New Day (Extended Mix)", ""
    };
    auto version_title = [](std::string_view value) { return value; };
    assert(plausible_release_group_title(
        "A New Day", "Various Artists - Compilation", grouped_versions, version_title));
    assert(plausible_release_group_title(
        "Compilation", "Various Artists - Compilation", grouped_versions, version_title));
    assert(!plausible_release_group_title(
        "Only You", "Various Artists - Compilation", grouped_versions, version_title));
    assert(!plausible_release_group_title(
        "A New Day", "Compilation", std::vector<std::string_view>{}, version_title));
    assert(!plausible_release_group_title(
        "A New Day", "Compilation", std::vector<std::string_view>{"A New Daylight"}, version_title));
    assert(plausible_release_group_title(
        "Only You (Extended Mix)", "Compilation",
        std::vector<std::string_view>{"Only You"}, version_title));

    // A nonempty but unrelated guest/title response must not stop the
    // following broad-title phase. No phase ever implies tag acceptance.
    fallback_query_budget three_phases(3);
    assert(three_phases.begin());
    three_phases.observe(true); // wrong release from first phase
    assert(three_phases.has_candidates() && three_phases.can_continue());
    assert(three_phases.begin());
    three_phases.observe(false); // second phase empty
    assert(three_phases.has_candidates() && three_phases.can_continue());
    assert(three_phases.begin());
    three_phases.observe(true); // third phase contributes a candidate
    assert(three_phases.has_candidates() && !three_phases.can_continue());
    assert(!three_phases.begin());
    fallback_query_budget capped(999);
    assert(capped.begin() && capped.begin() && capped.begin());
    assert(!capped.begin()); // never more than three online requests
    fallback_query_budget limited(3);
    assert(limited.begin());
    limited.observe(false);
    limited.stop(); // provider result cap: no further online request
    assert(!limited.begin() && !limited.can_continue() &&
           !limited.has_candidates());
    fallback_query_budget empty(0);
    assert(!empty.begin() && !empty.has_candidates());
    fallback_query_budget three_collab_phases(collab.size());
    assert(three_collab_phases.begin());
    three_collab_phases.observe(true);
    assert(three_collab_phases.begin()); // wrong nonempty partner cannot hide title
    assert(three_collab_phases.begin()); // partner cannot hide broad title
    assert(!three_collab_phases.begin());
    std::vector<std::string> ids;
    assert(!already_seen_release_id(ids, "36397609"));
    assert(already_seen_release_id(ids, "36397609"));
    assert(!already_seen_release_id(ids, "36819730"));
    assert(!already_seen_release_id(ids, "35534398"));
    assert(already_seen_release_id(ids, ""));
    assert(ids.size() == 3);
    // Common Unicode display variants are discovery-only; never alter tags.
    assert(trailing_collaborator("Kyau & Albert × Maywave") == "Maywave");
    assert(without_terminal_mix_suffix("Only You (Extended Remix)") == "Only You");
    assert(without_terminal_mix_suffix("Only You (Instrumental Mix)") == "Only You");
    assert(without_terminal_mix_suffix("Only You (Live)") == "Only You (Live)");
    assert(plausible_release_title("Café – L’Été", "Cafe - L'Ete (Extended Remix)"));
    assert(plausible_release_title("Música", "Musica"));
    assert(plausible_release_title("Äther", "Ather"));
    assert(plausible_release_title("Æther", "Aether"));
    assert(plausible_release_title("Straße", "Strasse"));
    assert(plausible_release_title("Café", "Café")); // decomposed combining accent
    assert(plausible_release_title("Only You (Extended Remix)", "Only You"));
    assert(plausible_release_title("Only You (Instrumental Mix)", "Only You"));
    assert(!plausible_release_title("Café", "Cafeteria"));
    assert(!plausible_release_title("Café", "Cafeine"));
    assert(!plausible_release_title("Only You", "Only Yourself"));
    auto unicode_partner = build_fallback_plan("Kyau & Albert × Maywave",
        "1000 Years (Extended Remix)", "");
    assert(unicode_partner.size() == 3 && unicode_partner[0].artist_filter == "Kyau & Albert");
    assert(unicode_partner[1].q == "Maywave");
    assert(unicode_partner[2].q == "1000 Years");
    // Explicitly mismatched artist credits cannot suppress fallback merely
    // because an unrelated release has the requested title.
    assert(plausible_original_release_title("Matt Fax", "Only You",
           "Matt Fax - Only You"));
    assert(!plausible_original_release_title("Matt Fax", "Only You",
            "Other Artist - Only You"));
    assert(plausible_original_release_title(
           "Above & Beyond feat. Richard Bedford", "Sun & Moon",
           "Above & Beyond - Sun & Moon"));
    assert(plausible_original_release_title("Matt Fax", "Only You",
           "Only You - Extended Mix"));
    assert(plausible_original_release_title("Matt Fax", "Only You",
           "Only You"));
    assert(!plausible_original_release_group_title(
           "Matt Fax", "Only You", "Various - Compilation",
           std::vector<std::string_view>{"Unrelated Artist - Only You"},
           [](std::string_view value) { return value; }));
    assert(plausible_original_release_group_title(
           "Matt Fax", "Only You", "Various - Compilation",
           std::vector<std::string_view>{"Matt Fax - Only You"},
           [](std::string_view value) { return value; }));
    assert(without_terminal_mix_suffix("Only You [John Smith Remix]") == "Only You");
    assert(without_terminal_mix_suffix("Till You Let Go [2026 Remaster]") == "Till You Let Go");
    assert(without_terminal_mix_suffix("Only You – Extended Mix") == "Only You");
    assert(without_terminal_mix_suffix("Only You — Radio Edit") == "Only You");
    assert(without_terminal_mix_suffix("Only You – Live") == "Only You – Live");
    assert(without_terminal_mix_suffix("Only You [Live]") == "Only You [Live]");
    const auto unicode_version = build_fallback_plan(
        "Estiva", "Café – L’Été (Extended Mix)", "");
    assert(unicode_version.size() == 3 &&
           unicode_version[0].release_title_filter == "Café – L’Été (Extended Mix)" &&
           unicode_version[1].release_title_filter == "Café – L’Été" &&
           unicode_version[2].q == "Café – L’Été");
    assert(encode_fallback_query(unicode_version[1]) ==
           "artist=Estiva&release_title=Caf%C3%A9%20%E2%80%93%20L%E2%80%99%C3%89t%C3%A9&type=release");
    const auto unicode_title = build_fallback_plan("Estiva", "Café – L’Été", "");
    assert(unicode_title.size() == 3 && unicode_title[0].artist_filter == "Estiva");
    assert(unicode_title[1].q == "Café – L’Été");
    assert(unicode_title[2].q == "cafe l ete");
    assert(unicode_title[2].label == "unicode-folded title discovery (review required)");
    assert(encode_fallback_query(unicode_title[2]) == "q=%22cafe%20l%20ete%22");
    const auto unicode_collab = build_fallback_plan(
        "Kyau & Albert × Maywave", "Café – L’Été", "");
    assert(unicode_collab.size() == 3);
    assert(unicode_collab[0].artist_filter == "Kyau & Albert" && unicode_collab[1].q == "Maywave");
    assert(unicode_collab[2].q == "Café – L’Été");
    assert(unicode_collab[2].artist_filter.empty());
    const auto other_script = build_fallback_plan("Artist", "東京", "");
    assert(other_script.size() == 2 && other_script[0].artist_filter == "Artist" && other_script[1].q == "東京");
    fallback_query_budget unicode_budget(unicode_collab.size());
    assert(unicode_budget.begin() && unicode_budget.begin() && unicode_budget.begin());
    assert(!unicode_budget.begin());
    // Real search output is not cleared or partially replaced when a later
    // optional fallback page, preview or abort fails after staging candidates.
    std::vector<int> previous_exact{101}, previous_other{202};
    bool aborted = false;
    try {
        staged_search_result_pair<std::vector<int>> partial;
        partial.exact.push_back(303);
        partial.other.push_back(404);
        throw std::runtime_error("synthetic late provider failure");
    } catch (const std::runtime_error&) {
        aborted = true;
    }
    assert(aborted && previous_exact == std::vector<int>{101} &&
           previous_other == std::vector<int>{202});
    staged_search_result_pair<std::vector<int>> completed;
    completed.exact.push_back(505);
    completed.other.push_back(606);
    completed.publish(previous_exact, previous_other);
    assert(previous_exact == std::vector<int>{505} &&
           previous_other == std::vector<int>{606});
    staged_search_result_pair<std::vector<int>> no_results;
    no_results.publish(previous_exact, previous_other);
    assert(previous_exact.empty() && previous_other.empty());
    return 0;
}
