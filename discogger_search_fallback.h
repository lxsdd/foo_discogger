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
    const std::string key = requested_title.empty() ? requested_track : requested_title;
    if (key.size() < 3) return plan;
    const std::string partner = trailing_collaborator(initial_artist);
    if (!partner.empty() && partner != initial_artist)
        plan.push_back({"collaboration partner and title", partner, key});
    // Deliberately broaden to a title-only Discogs 'q', keeping the exact
    // structured query untouched. Low-confidence candidates require review.
    const std::string broad = without_terminal_mix_suffix(key);
    if (broad.size() >= 3 && (plan.empty() || partner != broad))
        plan.push_back({"title-only discovery (review required)", broad, ""});
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
