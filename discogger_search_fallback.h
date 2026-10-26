#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

// Search planning is independent of foobar/Discogs and intentionally bounded.
// Candidates are suggestions only. Never rewrite the user's source metadata.
namespace discogger_search {

inline std::string trim_copy(std::string_view value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return std::string(value.substr(first, last - first + 1));
}
inline std::string ascii_lower(std::string_view value) {
    std::string out(value);
    for (char& ch : out)
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    return out;
}
// Suggestions from ampersand-separated artist credits remain review-only:
// an ampersand can also be part of a permanent duo/group name.
inline std::string trailing_collaborator(std::string_view artist) {
    const auto lowered = ascii_lower(artist);
    constexpr std::string_view joins[] = {
        " x ", " vs. ", " vs ", " feat. ", " feat ", " featuring ", " ft. ", " ft "
    };
    std::size_t position = std::string_view::npos, offset = 0;
    for (const auto join : joins) {
        const auto p = lowered.rfind(join);
        if (p != std::string::npos && (position == std::string_view::npos || p > position)) {
            position = p; offset = join.size();
        }
    }
    // Prefer an explicit featuring/x/vs join over an ampersand inside a
    // collaborating group's name (e.g. "Maywave x Kyau & Albert").
    // Only use '&' as a weaker discovery hint if no explicit join exists.
    if (position == std::string_view::npos) {
        position = lowered.rfind(" & ");
        offset = 3;
    }
    if (position == std::string_view::npos) return {};
    return trim_copy(artist.substr(position + offset));
}
// A featuring credit may add a guest to a release's display artist while
// Discogs indexes the release under the primary artist. Only explicit
// "feat/ft/featuring" joins qualify; an ampersand within a band's name or
// an "x" collaboration alone is NOT evidence of a canonical main artist.
// Preserve the user's complete artist credit for tags and track matching.
inline std::string primary_artist_before_featuring(std::string_view artist) {
    const std::string lowered = ascii_lower(artist);
    constexpr std::string_view joins[] = {
        " feat. ", " feat ", " featuring ", " ft. ", " ft "
    };
    std::size_t first = std::string_view::npos;
    for (const auto join : joins) {
        const auto pos = lowered.find(join);
        if (pos != std::string::npos &&
            (first == std::string_view::npos || pos < first))
            first = pos;
    }
    return first == std::string_view::npos
        ? std::string() : trim_copy(artist.substr(0, first));
}
// Only strip a *recognized, terminal* version descriptor. Preserve the
// user's exact title and version for all later track matching and tagging.
inline std::string without_terminal_mix_suffix(std::string_view title) {
    const std::string clean = trim_copy(title);
    const std::string lower = ascii_lower(clean);
    constexpr std::string_view suffixes[] = {
        " (dj version)", " (extended mix)", " (extended version)",
        " (original mix)", " (radio edit)", " (club mix)",
        " [dj version]", " [extended mix]", " [radio edit]"
    };
    for (const auto suffix : suffixes) {
        if (lower.size() > suffix.size() && lower.compare(
            lower.size() - suffix.size(), suffix.size(), suffix) == 0)
            return trim_copy(std::string_view(clean).substr(0, clean.size() - suffix.size()));
    }
    return clean;
}

 // Treat a structured search hit as plausible only if its *release* title
 // contains the requested release title as whole words. Discogs may prefix
 // the result with artist credits or append a mix/version. This is a
 // conservative discovery gate, NOT an identity match or tag approval.
inline std::string fold_title_words(std::string_view text) {
    std::string out;
    for (unsigned char ch : text) {
        const bool word = (ch >= 'A' && ch <= 'Z') ||
            (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch >= 0x80;
        if (word) {
            out.push_back(ch >= 'A' && ch <= 'Z'
                ? static_cast<char>(ch - 'A' + 'a') : static_cast<char>(ch));
        } else if (!out.empty() && out.back() != ' ') {
            out.push_back(' ');
        }
    }
    if (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}
inline bool plausible_release_title(std::string_view requested, std::string_view result) {
    const std::string needle = fold_title_words(without_terminal_mix_suffix(requested));
    if (needle.size() < 3) return false;
    const std::string haystack = fold_title_words(result);
    std::size_t at = haystack.find(needle);
    while (at != std::string::npos) {
        if ((at == 0 || haystack[at - 1] == ' ') &&
            (at + needle.size() == haystack.size() ||
             haystack[at + needle.size()] == ' '))
            return true;
        at = haystack.find(needle, at + 1);
    }
    return false;
}


 // A database-search release can be grouped under a differently titled
 // master. The requested title may match a child version even when the
 // master title is a compilation or unrelated parent title. Suppressing
 // fallback based on the master alone would miss a usable original hit.
 // This remains a discovery heuristic, never a release identity approval.
template <typename Children, typename ChildTitle>
inline bool plausible_release_group_title(
    std::string_view requested, std::string_view master_title,
    const Children& children, ChildTitle title_of) {
    if (plausible_release_title(requested, master_title)) return true;
    for (const auto& child : children)
        if (plausible_release_title(requested, title_of(child))) return true;
    return false;
}

struct fallback_candidate {
    std::string label;
    std::string q;
    std::string title;
    // Optional, stricter Discogs release-only path. Never combine with q or
    // the broad title filter: values always come from separate UTF-8 fields.
    std::string artist_filter{};
    std::string release_title_filter{};
};

// Build URL parameters from *separate values*, never by parsing a string
// containing untrusted artist/title metadata as a query-field expression.
// UTF-8 bytes are percent encoded directly; only fixed field names form
// parameter delimiters. Preserve the previous exact quoted-q semantics.
inline std::string percent_encode_search_value(std::string_view value) {
    constexpr char hex[] = "0123456789ABCDEF";
    std::string encoded;
    encoded.reserve(value.size() * 3);
    for (unsigned char ch : value) {
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
            (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' ||
            ch == '.' || ch == '~') {
            encoded.push_back(static_cast<char>(ch));
        } else {
            encoded.push_back('%');
            encoded.push_back(hex[ch >> 4]);
            encoded.push_back(hex[ch & 15]);
        }
    }
    return encoded;
}
inline std::string encode_fallback_query(const fallback_candidate& candidate) {
    if (!candidate.artist_filter.empty()) {
        if (!candidate.q.empty() || !candidate.title.empty() ||
            candidate.release_title_filter.empty()) return {};
        return "artist=" + percent_encode_search_value(candidate.artist_filter) +
            "&release_title=" +
            percent_encode_search_value(candidate.release_title_filter) +
            "&type=release";
    }
    if (candidate.q.empty() || !candidate.release_title_filter.empty()) return {};
    std::string quoted_q;
    quoted_q.reserve(candidate.q.size() + 2);
    quoted_q.push_back('"');
    quoted_q.append(candidate.q);
    quoted_q.push_back('"');
    std::string params = "q=" + percent_encode_search_value(quoted_q);
    if (!candidate.title.empty()) {
        params += "&title=";
        params += percent_encode_search_value(candidate.title);
    }
    return params;
}
// At most three bounded fallback *queries*, after an empty original
// search or one with no plausible release-title match: a guest-artist hint, an optional explicitly featured MAIN artist
// plus release title, and broad title discovery. All remain review-only.
// No unqualified short/empty title or broad artist-only search.
inline std::vector<fallback_candidate> build_fallback_plan(
    std::string_view artist, std::string_view title, std::string_view track) {
    std::vector<fallback_candidate> plan;
    const std::string initial_artist = trim_copy(artist);
    const std::string requested_title = trim_copy(title);
    const std::string requested_track = trim_copy(track);
    if (initial_artist.empty() || (requested_title.empty() && requested_track.empty()))
        return plan;
    // A short release title (e.g. "EP") cannot anchor a useful fallback.
    // Prefer a substantive track title in that case, but NEVER feed it into
    // Discogs' release-title filter: track and release titles are not the same.
    const bool using_track_title = requested_title.size() < 3;
    const std::string key = using_track_title ? requested_track : requested_title;
    if (key.size() < 3) return plan;
    const std::string partner = trailing_collaborator(initial_artist);
    if (!using_track_title) {
        const std::string primary = primary_artist_before_featuring(initial_artist);
        if (primary.size() >= 2 && primary != initial_artist) {
            // Authenticated Discogs evidence: broad "Sun & Moon" did not
            // contain release 2783977 on pages 1-2, but an artist +
            // release_title + type=release query found it on page 1.
            // Put this *first* for featuring credits: the existing guest
            // lookup stops after ANY nonempty result, even a wrong release.
            // Do not broaden title=track; no automatic identity acceptance.
            plan.push_back({"featured primary artist and release title (review required)",
                "", "", primary, key});
        }
    }
    if (!using_track_title && !partner.empty() && partner != initial_artist)
        plan.push_back({"collaboration partner and title", partner, key});
    // Deliberately broaden to a review-only Discogs 'q', keeping the exact
    // structured query untouched. A track title must not masquerade as the
    // release title; only the broad text index is queried in that case.
    const std::string broad = without_terminal_mix_suffix(key);
    // Even when the partner text equals the title, these are two *different*
    // Discogs queries: q=partner&title=title versus q=title. Omitting the
    // broad request hides relevant releases after an empty partner search.
    if (broad.size() >= 3)
        plan.push_back({using_track_title ? "track-title discovery (review required)"
                                          : "title-only discovery (review required)", broad, ""});
    return plan;
}

// Every planned review-only phase gets a chance to contribute results. A
// nonempty guest/title response may contain only unrelated releases, so it
// must not suppress the subsequent broad-title phase. The budget is capped
// independently of the planner; a provider limit/error still stops requests.
class fallback_query_budget {
public:
    explicit fallback_query_budget(std::size_t planned) noexcept
        : remaining_(planned < std::size_t{3} ? planned : std::size_t{3}) {}
    bool begin() noexcept {
        if (stopped_ || remaining_ == 0) return false;
        --remaining_;
        return true;
    }
    void observe(bool has_parsed_candidates) noexcept {
        saw_candidates_ = saw_candidates_ || has_parsed_candidates;
    }
    void stop() noexcept { stopped_ = true; }
    bool has_candidates() const noexcept { return saw_candidates_; }
    bool can_continue() const noexcept { return !stopped_ && remaining_ != 0; }
private:
    std::size_t remaining_;
    bool saw_candidates_ = false;
    bool stopped_ = false;
};

// Identity is type + numerical Discogs ID, not title, role or search source.
// Release and master with the same numerical ID are different objects.
inline bool already_seen_release_id(
    std::vector<std::string>& seen_ids, std::string_view id) {
    if (id.empty()) return true;
    const std::string candidate(id);
    if (std::find(seen_ids.begin(), seen_ids.end(), candidate) != seen_ids.end())
        return true;
    seen_ids.push_back(candidate);
    return false;
}
} // namespace discogger_search
