#pragma once
#include <string_view>
#include <utility>
#include "discogger_release_id.h"

// Parse and validate artist profile metadata into a private object before
// publishing. A failure in parsing, optional cache persistence or cancellation
// leaves the canonical artist, release lists, search state and artwork intact.
namespace discogger_cache {
inline bool same_artist_profile_identity(std::string_view expected,
                                         std::string_view returned) noexcept {
    return discogger_search::valid_release_id(expected) && expected == returned;
}

template <typename Stage, typename Parse, typename Persist,
          typename Check, typename Commit>
void stage_artist_profile(Stage& staged, Parse&& parse, Persist&& persist,
                          Check&& check, Commit&& commit) {
    check();
    std::forward<Parse>(parse)(staged);
    check();
    std::forward<Persist>(persist)();
    check();
    std::forward<Commit>(commit)(staged);
}
} // namespace discogger_cache
