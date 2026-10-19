#pragma once
#include <cstddef>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

// Pure, SDK-independent preflight used by both the task and the final writer.
// A mixed selection is rejected atomically before any asynchronous tag writes.
namespace discogger_write_safety {
// Fail closed on unsafe handles before any SDK dereference or file write.
 // Reads may legitimately contain virtual CUE tracks or duplicate physical
 // locations; writes must reject them atomically. The comparison callback
 // supplies the host's case-insensitive path ordering, without filesystem I/O.
enum class target_batch_decision {
    ready, reject_empty_selection, reject_null_handle, reject_empty_path,
    reject_virtual_subsong, reject_duplicate_path
};
template <typename HasHandleAt, typename PathAt, typename SubsongAt, typename ComparePaths>
target_batch_decision inspect_target_batch(std::size_t count, bool physical_write,
    HasHandleAt has_handle, PathAt path_at, SubsongAt subsong_at, ComparePaths compare_paths) {
    if (count == 0) return target_batch_decision::reject_empty_selection;
    auto less = [&](const std::string& a, const std::string& b) {
        return compare_paths(a, b) < 0;
    };
    std::set<std::string, decltype(less)> seen(less);
    for (std::size_t i = 0; i < count; ++i) {
        if (!has_handle(i)) return target_batch_decision::reject_null_handle;
        const char* path = path_at(i);
        if (!path || !*path) return target_batch_decision::reject_empty_path;
        if (physical_write) {
            if (subsong_at(i) != 0)
                return target_batch_decision::reject_virtual_subsong;
            if (!seen.insert(path).second)
                return target_batch_decision::reject_duplicate_path;
        }
    }
    return target_batch_decision::ready;
}

template <typename SubsongIndexAt>
bool contains_virtual_subsong(std::size_t count, SubsongIndexAt index_at) {
    for (std::size_t i = 0; i < count; ++i) {
        if (index_at(i) != 0) return true;
    }
    return false;
}
// Revoke stale per-file snapshots atomically when a fresh physical read begins,
// and after a failed/cancelled commit. This is SDK-independent for regressions.
// Snapshot each file_info into separate heap storage before asynchronous I/O.
// Subsequent UI/preview edits may mutate their original wrappers, but cannot
// mutate the payload handed to the host metadata writer. The outer callback
// owns this store until the host reports completion.
template <typename T>
class stable_write_copies {
    std::vector<std::unique_ptr<T>> copies_;
public:
    T* append_copy(const T& source) {
        auto copy = std::make_unique<T>();
        *copy = source;
        T* stable = copy.get();
        copies_.push_back(std::move(copy));
        return stable;
    }
    std::size_t size() const noexcept { return copies_.size(); }
};

template <typename RevokeAt>
void revoke_snapshot_batch(std::size_t count, RevokeAt revoke_at) {
    for (std::size_t i = 0; i < count; ++i) revoke_at(i);
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

// A failed/partial SDK read must never be treated as a complete physical
// snapshot when the tag writer prepares potentially destructive changes.
class initial_metadata_read {
    bool complete_ = false;
public:
    void begin() noexcept { complete_ = false; }
    bool finish(bool sdk_success, std::size_t expected, std::size_t loaded) noexcept {
        complete_ = sdk_success && expected == loaded;
        return complete_;
    }
    bool ready() const noexcept { return complete_; }
};

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
