#pragma once
#include <cstddef>
#include <limits>
#include <vector>

// Automatic duration matching is safe only with a uniquely closest,
// unclaimed Discogs track. Claimed tracks with an equal/better score make
// the greedy mapping ambiguous, so the caller must request manual matching.
namespace discogger_matching {
inline std::size_t unique_nearest_unclaimed(
    const std::vector<int>& deltas, const std::vector<bool>& claimed) noexcept {
    const std::size_t none = static_cast<std::size_t>(-1);
    if (deltas.empty() || deltas.size() != claimed.size()) return none;
    int best = std::numeric_limits<int>::max();
    std::size_t winner = none;
    bool ambiguous = false;
    for (std::size_t i = 0; i < deltas.size(); ++i) {
        if (deltas[i] < 0) return none;
        if (claimed[i]) continue;
        if (deltas[i] < best) {
            best = deltas[i];
            winner = i;
            ambiguous = false;
        } else if (deltas[i] == best) {
            ambiguous = true;
        }
    }
    if (winner == none || ambiguous) return none;
    for (std::size_t i = 0; i < deltas.size(); ++i)
        if (claimed[i] && deltas[i] <= best) return none;
    return winner;
}
} // namespace discogger_matching
