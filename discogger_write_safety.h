#pragma once
#include <cstddef>
#include <utility>

// Pure, SDK-independent preflight used by both the task and the final writer.
// A mixed selection is rejected atomically before any asynchronous tag writes.
namespace discogger_write_safety {
template <typename SubsongIndexAt>
bool contains_virtual_subsong(std::size_t count, SubsongIndexAt index_at) {
    for (std::size_t i = 0; i < count; ++i) {
        if (index_at(i) != 0) return true;
    }
    return false;
}
// Metadata-only no-op planner: caller compares full physical metadata and ReplayGain.
enum class metadata_write_decision { skip, write, reject_missing_snapshot };
inline metadata_write_decision plan_metadata_write(bool selected, bool snapshot_available,
    bool metadata_equal, bool replaygain_equal) noexcept {
    if (!selected) return metadata_write_decision::skip;
    if (!snapshot_available) return metadata_write_decision::reject_missing_snapshot;
    return metadata_equal && replaygain_equal ? metadata_write_decision::skip
                                               : metadata_write_decision::write;
}


// Explicit artwork no-op decision: an unchanged tag snapshot alone is NOT
// sufficient evidence that an artwork write can be skipped.
enum class artwork_write_decision { skip, write, reject_missing_snapshot };
inline artwork_write_decision plan_artwork_write(bool requested,
    bool artwork_snapshot_available, bool artwork_equal) noexcept {
    if (!requested) return artwork_write_decision::skip;
    if (!artwork_snapshot_available) return artwork_write_decision::reject_missing_snapshot;
    return artwork_equal ? artwork_write_decision::skip : artwork_write_decision::write;
}

// Single-shot completion gate: never start dependent artwork after a failed
// physical tag commit, and never dispatch twice on duplicate notifications.
class write_completion_gate {
    bool completed_ = false;
public:
    template <typename SuccessAction>
    bool finish(bool successful, SuccessAction&& action) {
        if (completed_) return false;
        completed_ = true;
        if (!successful) return false;
        std::forward<SuccessAction>(action)();
        return true;
    }
};
} // namespace discogger_write_safety
