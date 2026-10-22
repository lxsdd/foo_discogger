#include "../discogger_write_safety.h"
#include <array>
#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <string>

int main() {
    using discogger_write_safety::unchanged_single_value;
    assert(unchanged_single_value(0, nullptr, ""));
    assert(!unchanged_single_value(0, nullptr, "New"));
    assert(unchanged_single_value(1, "RUNNER-VERIFIED", "RUNNER-VERIFIED"));
    assert(!unchanged_single_value(1, "Old", "New"));
    assert(!unchanged_single_value(2, "RUNNER-VERIFIED", "RUNNER-VERIFIED"));
    assert(!unchanged_single_value(1, "Value", ""));
    assert(!unchanged_single_value(1, nullptr, "Value"));
    assert(!unchanged_single_value(0, nullptr, nullptr));
    // Pure target validation: no filesystem access and no host writes.
    using target = discogger_write_safety::target_batch_decision;
    struct input { bool handle; const char* path; unsigned subsong; };
    std::vector<input> entries = {{true, "file://C:\\Music\\Track.mp3", 0},
                                  {true, "file://C:\\Music\\Other.flac", 0}};
    int paths_examined = 0;
    const auto cmp = [](const std::string& a, const std::string& b) {
        auto fold = [](std::string s) {
            for (auto& ch : s) if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
            return s;
        };
        const auto lhs = fold(a), rhs = fold(b);
        return lhs < rhs ? -1 : (lhs > rhs ? 1 : 0);
    };
    const auto check = [&](bool write) {
        paths_examined = 0;
        return discogger_write_safety::inspect_target_batch(entries.size(), write,
            [&](std::size_t i) { return entries[i].handle; },
            [&](std::size_t i) { ++paths_examined; return entries[i].path; },
            [&](std::size_t i) { return entries[i].subsong; }, cmp);
    };
    assert(check(true) == target::ready && paths_examined == 2);
    assert(check(false) == target::ready);
    entries[1].path = "FILE://c:\\music\\TRACK.mp3";
    assert(check(true) == target::reject_duplicate_path);
    assert(check(false) == target::ready); // repeated paths are valid for reads
    entries[1].subsong = 2;
    assert(check(true) == target::reject_virtual_subsong);
    assert(check(false) == target::ready); // virtual CUE reads remain supported
    entries[1].subsong = 0;
    entries[1].path = "";
    assert(check(true) == target::reject_empty_path);
    assert(check(false) == target::reject_empty_path);
    entries[1].path = nullptr;
    assert(check(true) == target::reject_empty_path);
    entries[1].handle = false;
    assert(check(true) == target::reject_null_handle && paths_examined == 1);
    assert(check(false) == target::reject_null_handle);
    entries.clear();
    assert(check(true) == target::reject_empty_selection && paths_examined == 0);
    entries = {{true, "file://C:\\Music\\Good.mp3", 0}};
    assert(check(true) == target::ready);
    // Regression: a repeated large selection stays bounded by ordered lookup.
    std::vector<std::string> generated;
    for (int i = 0; i < 128; ++i) generated.push_back("file://D:/synthetic/" + std::to_string(i) + ".flac");
    entries.clear();
    for (const auto& path : generated) entries.push_back({true, path.c_str(), 0});
    assert(check(true) == target::ready);
    entries.push_back({true, "FILE://d:/SYNTHETIC/37.FLAC", 0});
    assert(check(true) == target::reject_duplicate_path);

    // A successful SDK call is not sufficient if even one physical file_info
    // failed to load. Re-reading must revoke any previously valid snapshot.
    discogger_write_safety::initial_metadata_read initial_read;
    assert(!initial_read.ready());
    assert(!initial_read.finish(false, 2, 2));
    assert(!initial_read.ready());
    initial_read.begin();
    assert(!initial_read.finish(true, 2, 1));
    assert(!initial_read.ready());
    initial_read.begin();
    assert(initial_read.finish(true, 2, 2));
    assert(initial_read.ready());
    initial_read.begin();
    assert(!initial_read.ready());
    assert(!initial_read.finish(false, 0, 0));
    assert(initial_read.finish(true, 0, 0));
    // A completed physical write invalidates the old read permission before
    // dependent artwork is dispatched. A fresh full read is required.
    discogger_write_safety::initial_metadata_read post_commit;
    assert(post_commit.finish(true, 1, 1) && post_commit.ready());
    post_commit.begin();
    assert(!post_commit.ready());
    assert(!post_commit.finish(true, 2, 1));
    assert(!post_commit.ready());
    assert(post_commit.finish(true, 2, 2) && post_commit.ready());
    const std::array<unsigned, 3> physical = {0, 0, 0};
    const std::array<unsigned, 3> mixed = {0, 7, 0};
    const std::array<unsigned, 2> virtual_only = {1, 2};
    assert(!discogger_write_safety::contains_virtual_subsong(physical.size(),
        [&](std::size_t i) { return physical[i]; }));
    assert(discogger_write_safety::contains_virtual_subsong(mixed.size(),
        [&](std::size_t i) { return mixed[i]; }));
    assert(discogger_write_safety::contains_virtual_subsong(virtual_only.size(),
        [&](std::size_t i) { return virtual_only[i]; }));
    std::size_t calls = 0;
    assert(discogger_write_safety::contains_virtual_subsong(mixed.size(),
        [&](std::size_t i) { ++calls; return mixed[i]; }));
    assert(calls == 2); // stop before considering later physical members
    // Batch preflight is atomic and must never inspect out-of-range wrappers.
    using batch_decision = discogger_write_safety::snapshot_batch_decision;
    using discogger_write_safety::plan_snapshot_batch;
    // Stable async payloads are independent of mutable preview wrappers.
    // Multiple inserts must not invalidate pointers already given to the SDK.
    struct synthetic_info {
        int replaygain = 0;
        std::string artist;
    };
    discogger_write_safety::stable_write_copies<synthetic_info> staged;
    synthetic_info first{4, "Original"};
    auto* held_first = staged.append_copy(first);
    for (int i = 0; i < 128; ++i) {
        synthetic_info extra{i, "Supplemental"};
        staged.append_copy(extra);
        extra.artist = "Modified after staging";
    }
    first.artist = "Edited in preview";
    first.replaygain = 999;
    assert(staged.size() == 129);
    assert(held_first->artist == "Original" && held_first->replaygain == 4);
    // A synthetic callback may outlive the producer of the working wrappers.
    auto owned = std::make_shared<discogger_write_safety::stable_write_copies<synthetic_info>>();
    auto* callback_value = owned->append_copy(first);
    std::shared_ptr<discogger_write_safety::stable_write_copies<synthetic_info>> callback_owner = owned;
    owned.reset();
    first.artist = "Changed again";
    assert(callback_owner->size() == 1 && callback_value->artist == "Edited in preview");

    // A subsequent failing read or write must revoke every old permission,
    // including an old selected item with an otherwise valid snapshot.
    std::array<bool, 3> old_snapshots = {true, false, true};
    std::array<bool, 3> old_selections = {true, true, false};
    std::size_t revoked = 0;
    discogger_write_safety::revoke_snapshot_batch(old_snapshots.size(),
        [&](std::size_t i) {
            old_snapshots[i] = false;
            old_selections[i] = false;
            ++revoked;
        });
    assert(revoked == old_snapshots.size());
    for (std::size_t i = 0; i < old_snapshots.size(); ++i)
        assert(!old_snapshots[i] && !old_selections[i]);
    discogger_write_safety::revoke_snapshot_batch(0, [](std::size_t) { assert(false); });
    std::array<bool, 3> wrappers = {true, true, true};
    std::array<bool, 3> selected = {false, true, false};
    std::array<bool, 3> captured = {false, true, false};
    const auto plan_batch = [&](std::size_t items, std::size_t wrappers_count) {
        return plan_snapshot_batch(items, wrappers_count,
            [&](std::size_t i) { return wrappers[i]; },
            [&](std::size_t i) { return selected[i]; },
            [&](std::size_t i) { return captured[i]; });
    };
    assert(plan_batch(3, 3) == batch_decision::ready);
    assert(plan_batch(3, 2) == batch_decision::reject_count_mismatch);
    wrappers[2] = false;
    assert(plan_batch(3, 3) == batch_decision::reject_missing_wrapper);
    wrappers[2] = true;
    captured[1] = false;
    assert(plan_batch(3, 3) == batch_decision::reject_missing_snapshot);
    selected[1] = false;
    assert(plan_batch(3, 3) == batch_decision::ready);
    int touched = 0;
    assert(plan_snapshot_batch(2, 3,
        [&](std::size_t) { ++touched; return true; },
        [&](std::size_t) { ++touched; return true; },
        [&](std::size_t) { ++touched; return true; }) == batch_decision::reject_count_mismatch);
    assert(touched == 0);
    // Failed preview staging rolls back; completed previews retain staged data.
    int stage_resets = 0;
    try {
        discogger_write_safety::staging_rollback_guard rollback([&] { ++stage_resets; });
        throw std::runtime_error("synthetic stage failure");
    } catch (const std::runtime_error&) {}
    assert(stage_resets == 1);
    {
        discogger_write_safety::staging_rollback_guard rollback([&] { ++stage_resets; });
        rollback.disarm();
    }
    assert(stage_resets == 1);
    {
        discogger_write_safety::staging_rollback_guard rollback([&] {
            ++stage_resets;
            throw std::runtime_error("synthetic reset exception");
        });
    }
    assert(stage_resets == 2);
    // A failed physical callback may throw while restoring its preview.
    // Revocation must already cover EVERY item, including later items.
    std::array<bool, 3> stale_approval = {true, true, true};
    std::array<int, 3> preview = {7, 8, 9};
    bool restoration_failed = false;
    try {
        discogger_write_safety::revoke_before_restore(
            [&]() { for (auto& allowed : stale_approval) allowed = false; },
            [&]() {
                for (bool allowed : stale_approval) assert(!allowed);
                preview[0] = 1;
                throw std::runtime_error("synthetic file_info assignment failure");
            });
    } catch (const std::runtime_error&) { restoration_failed = true; }
    assert(restoration_failed && preview[0] == 1);
    for (bool allowed : stale_approval) assert(!allowed);
    bool restored_without_revocation = false;
    discogger_write_safety::revoke_before_restore(
        [&]() { stale_approval[0] = false; },
        [&]() { restored_without_revocation = !stale_approval[0]; });
    assert(restored_without_revocation);

    // Snapshot/no-op planning: unchanged metadata and ReplayGain never schedule I/O.
    using decision = discogger_write_safety::metadata_write_decision;
    using discogger_write_safety::plan_metadata_write;
    assert(plan_metadata_write(false, false, false, false) == decision::skip);
    assert(plan_metadata_write(true, false, false, false) == decision::reject_missing_snapshot);
    assert(plan_metadata_write(true, true, true, true) == decision::skip);
    assert(plan_metadata_write(true, true, false, true) == decision::write);
    assert(plan_metadata_write(true, true, true, false) == decision::write);
    assert(plan_metadata_write(true, true, false, false) == decision::write);
    // Artwork requires its own independently captured snapshot.
    using art_decision = discogger_write_safety::artwork_write_decision;
    using discogger_write_safety::plan_artwork_write;
    assert(plan_artwork_write(false, false, false) == art_decision::skip);
    assert(plan_artwork_write(true, false, false) == art_decision::reject_missing_snapshot);
    assert(plan_artwork_write(true, true, true) == art_decision::skip);
    assert(plan_artwork_write(true, true, false) == art_decision::write);
    // Failed async status does not imply an atomic rollback. Unknown
    // readbacks must be counted as unverified, never as unchanged.
    using failed_state = discogger_write_safety::failed_write_file_state;
    discogger_write_safety::failed_write_audit audit;
    assert(audit.count() == 0 && !audit.cannot_confirm_unchanged());
    audit.record(failed_state::original);
    assert(audit.count() == 1 && !audit.cannot_confirm_unchanged());
    audit.record(failed_state::requested);
    audit.record(failed_state::divergent);
    audit.record(failed_state::unreadable);
    assert(audit.count() == 4 && audit.original == 1 && audit.requested == 1 &&
           audit.divergent == 1 && audit.unreadable == 1 && audit.cannot_confirm_unchanged());
    // Positive, negative, and duplicate-completion regressions.
    int artwork_starts = 0;
    discogger_write_safety::write_completion_gate failed;
    assert(!failed.finish(false, [&] { ++artwork_starts; }));
    assert(!failed.finish(true, [&] { ++artwork_starts; }));
    assert(artwork_starts == 0);
    discogger_write_safety::write_completion_gate succeeded;
    assert(succeeded.finish(true, [&] { ++artwork_starts; }));
    assert(!succeeded.finish(true, [&] { ++artwork_starts; }));
    assert(!succeeded.finish(false, [&] { ++artwork_starts; }));
    assert(artwork_starts == 1);
    discogger_write_safety::write_completion_gate throwing;
    bool caught = false;
    try { throwing.finish(true, [] { throw std::runtime_error("synthetic callback failure"); }); }
    catch (const std::runtime_error&) { caught = true; }
    assert(caught);
    assert(!throwing.finish(true, [&] { ++artwork_starts; }));
    assert(artwork_starts == 1);
    int resets = 0;
    discogger_write_safety::write_completion_gate failed_with_reset;
    assert(!failed_with_reset.finish(false, [&] { ++artwork_starts; }, [&] { ++resets; }));
    assert(failed_with_reset.completed());
    assert(!failed_with_reset.finish(false, [&] { ++artwork_starts; }, [&] { ++resets; }));
    assert(!failed_with_reset.finish(true, [&] { ++artwork_starts; }, [&] { ++resets; }));
    assert(resets == 1 && artwork_starts == 1);
    discogger_write_safety::write_completion_gate succeeded_with_reset;
    assert(succeeded_with_reset.finish(true, [&] { ++artwork_starts; }, [&] { ++resets; }));
    assert(resets == 1 && artwork_starts == 2);
    discogger_write_safety::write_completion_gate throwing_reset;
    try { throwing_reset.finish(false, [] { assert(false); }, [] { throw std::runtime_error("synthetic reset failure"); }); }
    catch (const std::runtime_error&) { caught = true; }
    assert(throwing_reset.completed());
    assert(!throwing_reset.finish(false, [] { assert(false); }, [] { assert(false); }));
    assert(!discogger_write_safety::contains_virtual_subsong(0,
        [&](std::size_t) -> unsigned { assert(false); return 0; }));
}
