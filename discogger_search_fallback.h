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
        " x ", " × ", " vs. ", " vs ", " feat. ", " feat ", " featuring ", " ft. ", " ft "
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
// Explicit collaboration credits can use the first participant as a
// review-only release-title index hint. Never split an ampersand-only duo.
inline std::string primary_artist_before_explicit_collaboration(std::string_view artist) {
    const std::string lowered = ascii_lower(artist);
    constexpr std::string_view joins[] = {" x ", " × ", " vs. ", " vs ", " versus "};
    std::size_t first = std::string_view::npos, offset = 0;
    for (const auto join : joins) {
        const auto pos = lowered.find(join);
        if (pos != std::string_view::npos &&
            (first == std::string_view::npos || pos < first)) {
            first = pos;
            offset = join.size();
        }
    }
    if (first == std::string_view::npos) return {};
    const std::string left = trim_copy(artist.substr(0, first));
    const std::string right = trim_copy(artist.substr(first + offset));
    return left.size() >= 2 && right.size() >= 2 ? left : std::string();
}
inline std::string release_discovery_artist(std::string_view artist) {
    const std::string featured = primary_artist_before_featuring(artist);
    const std::string collaborating = primary_artist_before_explicit_collaboration(artist);
    if (!featured.empty() && !collaborating.empty())
        return featured.size() < collaborating.size() ? featured : collaborating;
    if (!featured.empty()) return featured;
    if (!collaborating.empty()) return collaborating;
    return trim_copy(artist);
}
// Only strip a *recognized, terminal* version descriptor. Preserve the
// user's exact title and version for all later track matching and tagging.
inline std::string without_terminal_mix_suffix(std::string_view title) {
    const std::string clean = trim_copy(title);
    const std::string lower = ascii_lower(clean);
    constexpr std::string_view suffixes[] = {
        " (dj version)", " (extended mix)", " (extended version)",
        " (original mix)", " (radio edit)", " (club mix)",
        " (extended remix)", " (instrumental mix)", " (dub mix)",
        " (original version)", " [original mix]", " [extended version]",
        " [dj version]", " [extended mix]", " [radio edit]",
        " - extended mix", " - original mix", " - radio edit",
        " - club mix", " - extended remix", " (deluxe edition)",
        " [deluxe edition]", " (special edition)", " [special edition]",
        " (remastered)", " [remastered]"
    };
    for (const auto suffix : suffixes) {
        if (lower.size() > suffix.size() && lower.compare(
            lower.size() - suffix.size(), suffix.size(), suffix) == 0)
            return trim_copy(std::string_view(clean).substr(0, clean.size() - suffix.size()));
    }
    // Artist-named remixes and year-labelled remasters are useful bounded
    // discovery hints, not exact release identities. Keep other parentheses.
    if (lower.size() > 5 && (lower.back() == ')' || lower.back() == ']')) {
        const auto open = lower.rfind(lower.back() == ')' ? " (" : " [");
        if (open != std::string::npos && open > 0 &&
            lower.size() - open <= 80) {
            const std::string_view inner(lower.data() + open + 2,
                                         lower.size() - open - 3);
            const auto ends = [](std::string_view value, std::string_view suffix) {
                return value.size() > suffix.size() &&
                    value.substr(value.size() - suffix.size()) == suffix;
            };
            if (inner == "remix" || inner == "remaster" ||
                ends(inner, " remix") || ends(inner, " remaster") ||
                (inner.size() > 11 && inner.substr(0, 11) == "remastered ")) {
                return trim_copy(std::string_view(clean).substr(0, open));
            }
        }
    }
    // Discogs and user tags also use Unicode en/em dashes as version
    // separators. Strip only a recognized terminal descriptor; never
    // discard an arbitrary subtitle such as " - Live".
    constexpr std::string_view separators[] = {" - ", " – ", " — "};
    constexpr std::string_view terminal_versions[] = {
        "extended mix", "original mix", "radio edit", "club mix",
        "extended remix", "instrumental mix", "dub mix", "dj version",
        "deluxe edition", "special edition", "remastered"
    };
    for (const auto separator : separators) {
        const auto at = lower.rfind(separator);
        if (at == std::string::npos || at == 0) continue;
        const std::string_view tail(lower.data() + at + separator.size(),
                                    lower.size() - at - separator.size());
        bool recognized = false;
        for (const auto version : terminal_versions)
            if (tail == version) { recognized = true; break; }
        if (!recognized &&
            ((tail.size() > 6 && tail.substr(tail.size() - 6) == " remix") ||
             (tail.size() > 9 && tail.substr(tail.size() - 9) == " remaster")))
recognized = true;
        if (recognized)
            return trim_copy(std::string_view(clean).substr(0, at));
    }
    return clean;
}

 // Treat a structured search hit as plausible only if its *release* title
 // contains the requested release title as whole words. Discogs may prefix
 // the result with artist credits or append a mix/version. This is a
 // conservative discovery gate, NOT an identity match or tag approval.
// A review-only discovery fold: normalize common Latin diacritics,
// decomposed accents and Unicode punctuation without changing stored tags,
// query bytes or release identities. Other scripts remain byte-exact.
inline std::string fold_title_words(std::string_view text) {
    std::string out;
    auto add_word = [&](std::string_view value) {
        out.append(value.data(), value.size());
    };
    auto add_boundary = [&]() {
        if (!out.empty() && out.back() != ' ') out.push_back(' ');
    };
    for (std::size_t i = 0; i < text.size();) {
        const unsigned char ch = static_cast<unsigned char>(text[i]);
        if (ch < 0x80) {
            if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
                (ch >= '0' && ch <= '9')) {
                out.push_back(ch >= 'A' && ch <= 'Z'
                    ? static_cast<char>(ch - 'A' + 'a')
                    : static_cast<char>(ch));
            } else {
                add_boundary();
            }
            ++i;
            continue;
        }
        if (i + 1 < text.size()) {
            const unsigned char next = static_cast<unsigned char>(text[i + 1]);
            if (ch == 0xC2 && next == 0xA0) {
                add_boundary(); i += 2; continue; // nonbreaking space
            }
            if (ch == 0xC3) {
                std::string_view mapped;
                if ((next >= 0x80 && next <= 0x85) ||
                    (next >= 0xA0 && next <= 0xA5)) mapped = "a";
                else if (next == 0x86 || next == 0xA6) mapped = "ae";
                else if (next == 0x87 || next == 0xA7) mapped = "c";
                else if ((next >= 0x88 && next <= 0x8B) ||
                         (next >= 0xA8 && next <= 0xAB)) mapped = "e";
                else if ((next >= 0x8C && next <= 0x8F) ||
                         (next >= 0xAC && next <= 0xAF)) mapped = "i";
                else if (next == 0x90 || next == 0xB0) mapped = "d";
                else if (next == 0x91 || next == 0xB1) mapped = "n";
                else if ((next >= 0x92 && next <= 0x96) ||
                         (next >= 0xB2 && next <= 0xB6) ||
                         next == 0x98 || next == 0xB8) mapped = "o";
                else if ((next >= 0x99 && next <= 0x9C) ||
                         (next >= 0xB9 && next <= 0xBC)) mapped = "u";
                else if (next == 0x9D || next == 0xBD ||
                         next == 0xBF) mapped = "y";
                else if (next == 0x9E || next == 0xBE) mapped = "th";
                else if (next == 0x9F) mapped = "ss";
                if (!mapped.empty()) {
                    add_word(mapped); i += 2; continue;
                }
                if (next == 0x97 || next == 0xB7) {
                    add_boundary(); i += 2; continue; // multiplication/division
                }
            }
            if (ch == 0xC5 && (next == 0x92 || next == 0x93 ||
                               next == 0xB8)) {
                add_word(next == 0xB8 ? "y" : "oe");
                i += 2; continue;
            }
            // U+0300..U+036F: combining accents, not word separators.
            if ((ch == 0xCC && next >= 0x80 && next <= 0xBF) ||
                (ch == 0xCD && next >= 0x80 && next <= 0xAF)) {
                i += 2; continue;
            }
        }
        if (i + 2 < text.size() && ch == 0xE2) {
            const unsigned char second = static_cast<unsigned char>(text[i + 1]);
            const unsigned char third = static_cast<unsigned char>(text[i + 2]);
            if ((second == 0x80 && third >= 0x90 && third <= 0xAF) ||
                (second == 0x88 && third == 0x92)) {
                add_boundary(); i += 3; continue; // curly quotes/dashes/minus
            }
        }
        // Do not invent transliterations for non-Latin scripts.
        out.push_back(static_cast<char>(ch));
        ++i;
    }
    if (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}
inline bool plausible_release_title(std::string_view requested, std::string_view result) {
    // Only suppress bounded discovery for a complete title, optionally
    // preceded by a Discogs artist credit. A title occurring inside a
    // different title ("Only You & Me", "I Love Only You") is NOT evidence.
    const std::string requested_clean = trim_copy(requested);
    const std::string result_clean = trim_copy(result);
    const std::string requested_base = without_terminal_mix_suffix(requested_clean);
    const std::string result_base = without_terminal_mix_suffix(result_clean);
    const std::string needle = fold_title_words(requested_base);
    if (needle.size() < 3) return false;

    // Two explicitly named but different edits/remixes must not prevent a
    // later search for the requested edition. A bare master/title can still
    // be a useful original hit; identity always remains REVIEW_REQUIRED.
    const std::string requested_version = fold_title_words(
        std::string_view(requested_clean).substr(requested_base.size()));
    const std::string result_version = fold_title_words(
        std::string_view(result_clean).substr(result_base.size()));
    if (!requested_version.empty() && !result_version.empty() &&
        requested_version != result_version) return false;

    const std::string candidate = fold_title_words(result_base);
    if (candidate == needle) return true;
    // The FIRST artist/title delimiter must introduce the entire requested
    // title. Do not match a later nested delimiter in a different title.
    std::size_t separator_at = std::string_view::npos;
    std::size_t separator_size = 0;
    for (const auto separator : {std::string_view(" - "),
                                 std::string_view(" – "),
                                 std::string_view(" — ")}) {
        const auto at = result_base.find(separator);
        if (at != std::string::npos &&
            (separator_at == std::string_view::npos || at < separator_at)) {
            separator_at = at;
            separator_size = separator.size();
        }
    }
    if (separator_at == std::string_view::npos) return false;
    const std::string candidate_tail = fold_title_words(
        std::string_view(result_base).substr(separator_at + separator_size));
    return candidate_tail == needle;
}
// A title-only match under a *different explicitly credited artist* is not
// sufficient evidence to suppress bounded discovery. Keep the original
// preview, but continue review-only fallback when the result has a clear
// "Artist - Release" prefix that conflicts with the requested artist.
// Titles without a parseable credit remain plausible, not auto-approved.
inline bool plausible_original_release_title(
    std::string_view artist, std::string_view title, std::string_view result) {
    if (!plausible_release_title(title, result)) return false;
    std::size_t separator_at = std::string_view::npos;
    for (const auto separator : {std::string_view(" - "),
                                 std::string_view(" – "),
                                 std::string_view(" — ")}) {
        const auto at = result.find(separator);
        if (at != std::string_view::npos &&
            (separator_at == std::string_view::npos || at < separator_at))
            separator_at = at;
    }
    if (separator_at == std::string_view::npos) return true;
    const auto prefix = trim_copy(result.substr(0, separator_at));
    // "Only You - Extended Mix" has no artist prefix; do not misclassify
    // the title itself as a conflicting artist credit.
    if (plausible_release_title(title, prefix)) return true;
    const auto credited = fold_title_words(prefix);
    const auto requested = fold_title_words(artist);
    if (credited.empty() || requested.empty() || credited == requested)
        return true;
    const auto featured_primary = primary_artist_before_featuring(artist);
    return !featured_primary.empty() &&
        credited == fold_title_words(featured_primary);
}

template <typename Children, typename ChildTitle>
inline bool plausible_original_release_group_title(
    std::string_view artist, std::string_view title,
    std::string_view master_title, const Children& children,
    ChildTitle title_of) {
    if (plausible_original_release_title(artist, title, master_title))
        return true;
    for (const auto& child : children)
        if (plausible_original_release_title(artist, title, title_of(child)))
            return true;
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
// search or one with no plausible release-title match: a dedicated artist +
// release_title lookup, an optional partner/title hint, and broad discovery.
// All remain review-only.
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
        // The dedicated release_title index is distinct from the original
        // title query and broad q index. Try it for single-artist misses as
        // well as featuring/x/×/vs credits; keep full source metadata.
        // A track title must never masquerade as a release_title.
        const std::string indexed_artist = release_discovery_artist(initial_artist);
        if (indexed_artist.size() >= 2)
            plan.push_back({"artist and release title (review required)",
                "", "", indexed_artist, key});
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
    if (broad.size() >= 3) {
        plan.push_back({using_track_title ? "track-title discovery (review required)"
                                          : "title-only discovery (review required)", broad, ""});
        // A Unicode-normalized spelling is a distinct, review-only search
        // hint, not a replacement for the exact UTF-8 query. Never exceed
        // the three-request provider budget or transliterate other scripts.
        const bool non_ascii = std::any_of(broad.begin(), broad.end(),
            [](unsigned char ch) { return ch >= 0x80; });
        if (non_ascii && plan.size() < 3) {
            const std::string folded = fold_title_words(broad);
            if (folded.size() >= 3 && folded != ascii_lower(broad))
                plan.push_back({"unicode-folded title discovery (review required)",
                                folded, ""});
        }
    }
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
