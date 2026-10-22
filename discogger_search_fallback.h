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
    if (position == std::string_view::npos) return {};
    return trim_copy(artist.substr(position + offset));
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
struct fallback_candidate {
    std::string label;
    std::string q;
    std::string title;
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
    if (candidate.q.empty()) return {};
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
// At most two extra database/search requests, only after an empty initial
// query. No unqualified short/empty title or broad artist-only search.
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
    if (!using_track_title && !partner.empty() && partner != initial_artist)
        plan.push_back({"collaboration partner and title", partner, key});
    // Deliberately broaden to a review-only Discogs 'q', keeping the exact
    // structured query untouched. A track title must not masquerade as the
    // release title; only the broad text index is queried in that case.
    const std::string broad = without_terminal_mix_suffix(key);
    if (broad.size() >= 3 && (plan.empty() || partner != broad))
        plan.push_back({using_track_title ? "track-title discovery (review required)"
                                          : "title-only discovery (review required)", broad, ""});
    return plan;
}

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
