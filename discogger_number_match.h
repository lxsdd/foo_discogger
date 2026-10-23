#pragma once
#include <cstddef>

// Missing DISCNUMBER and multiple numbering conventions can identify more
// than one release track. Never silently choose the first matching track.
namespace discogger_number_matching {
class unique_number_match {
public:
    void consider(int requested_disc, int requested_track,
        int release_disc, int release_track, int release_disc_track,
        std::size_t absolute_track_number,
        std::size_t disc_index, std::size_t track_index) noexcept {
        if (requested_disc < 0 || requested_track <= 0) return;
        const bool disc_matches = requested_disc == 0 || requested_disc == release_disc;
        const bool local_match = disc_matches &&
            (requested_track == release_track || requested_track == release_disc_track);
        const bool absolute_match = requested_disc == 0 &&
            static_cast<std::size_t>(requested_track) == absolute_track_number;
        if (local_match || absolute_match) {
            if (++matches_ == 1) {
                disc_index_ = disc_index;
                track_index_ = track_index;
            }
        }
    }
    bool unique() const noexcept { return matches_ == 1; }
    bool ambiguous() const noexcept { return matches_ > 1; }
    std::size_t disc_index() const noexcept { return disc_index_; }
    std::size_t track_index() const noexcept { return track_index_; }
private:
    std::size_t matches_ = 0;
    std::size_t disc_index_ = 0;
    std::size_t track_index_ = 0;
};
} // namespace discogger_number_matching
