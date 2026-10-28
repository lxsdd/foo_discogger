#pragma once
#include <string>
#include <string_view>
#include <unordered_set>
#include "discogger_release_id.h"

// A search-only artist row is not a confirmed Discogs artist. Preserve the
// existing "0" aggregate preview sentinel, but never turn malformed or
// overflowing IDs into selectable rows. Release and master identities use
// independent sets, so a release and a master may share a numeric ID.
namespace discogger_search {
inline std::string artist_preview_identity(std::string_view id) {
    if (id == "0") return "0"; // synthetic aggregate, never a cache key
    return canonical_release_id(id);
}

inline bool same_artist_preview_identity(std::string_view left,
                                         std::string_view right) {
    const auto key = artist_preview_identity(left);
    return !key.empty() && key == artist_preview_identity(right);
}

inline bool insert_preview_release_identity(
    std::unordered_set<std::string>& seen, std::string_view id) {
    const auto key = canonical_release_id(id);
    return !key.empty() && seen.emplace(key).second;
}
// A completed new advanced search must never leave an old release tree
// selectable when its artist rows contain no release/master previews.
// Incremental empty callbacks and a manually selected artist keep the tree.
enum class advanced_preview_action { keep, clear, show };
inline advanced_preview_action decide_advanced_preview_action(
    bool advanced_mode, bool append, bool incoming_results,
    bool artist_selected, bool has_preview_children) noexcept {
    if (!advanced_mode || (append && (!incoming_results || artist_selected)))
        return advanced_preview_action::keep;
    return has_preview_children ? advanced_preview_action::show
                                : advanced_preview_action::clear;
}
} // namespace discogger_search
