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
// Fail closed before scheduling any asynchronous write if the item/wrapper
// vectors are out of sync, a wrapper is absent, or an edited item has no
// original physical metadata snapshot. This is intentionally SDK-independent.
enum class snapshot_batch_decision {
    ready, reject_count_mismatch, reject_missing_wrapper, reject_missing_snapshot
};
template <typename HasWrapperAt, typename SelectedAt, typename SnapshotAt>
snapshot_batch_decision plan_snapshot_batch(std::size_t item_count,
    std::size_t wrapper_count, HasWrapperAt has_wrapper,
    SelectedAt selected, SnapshotAt has_snapshot) {
    if (item_count != wrapper_count) return snapshot_batch_decision::reject_count_mismatch;
    for (std::size_t i = 0; i < item_count; ++i) {
        if (!has_wrapper(i)) return snapshot_batch_decision::reject_missing_wrapper;
    }
    for (std::size_t i = 0; i < item_count; ++i) {
        if (selected(i) && !has_snapshot(i)) return snapshot_batch_decision::reject_missing_snapshot;
    }
    return snapshot_batch_decision::ready;
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
    bool completed() const noexcept { return completed_; }
    template <typename SuccessAction, typename FailureAction>
    bool finish(bool successful, SuccessAction&& success_action, FailureAction&& failure_action) {
        if (completed_) return false;
        completed_ = true; // Single-shot even if either action throws.
        if (!successful) {
            std::forward<FailureAction>(failure_action)();
            return false;
        }
        std::forward<SuccessAction>(success_action)();
        return true;
    }
    template <typename SuccessAction>
    bool finish(bool successful, SuccessAction&& action) {
        return finish(successful, std::forward<SuccessAction>(action), [] {});
    }
};
} // namespace discogger_write_safety
