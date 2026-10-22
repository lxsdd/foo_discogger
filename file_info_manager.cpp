#include "stdafx.h"

#include "file_info_manager.h"
#include "discogger_write_safety.h"
#include "discogger_physical_targets.h"
#include "SDK/input.h"
#include <cstring>
#include <exception>
#include <stdexcept>
#include <string>
#include <utility>

discogger_write_safety::failed_write_audit discogger_audit_failed_write(
    const discogger_async_tag_payload& payload, size_t read_limit) noexcept {
    using state = discogger_write_safety::failed_write_file_state;
    discogger_write_safety::failed_write_audit audit;
    const size_t total = payload.submitted_count;
    if (payload.items_to_update.get_count() < total ||
        payload.infos_to_write.get_count() < total ||
        payload.original_infos.get_count() < total ||
        payload.staged_infos.size() < total ||
        payload.original_copies.size() < total) {
        for (size_t i = 0; i < total; ++i) audit.record(state::unreadable);
        return audit;
    }
    const size_t inspect = (read_limit < total) ? read_limit : total;
    for (size_t i = 0; i < inspect; ++i) {
        try {
            const auto& item = payload.items_to_update[i];
            if (!item || item->get_subsong_index() != 0 ||
                !payload.infos_to_write[i] || !payload.original_infos[i])
                throw std::runtime_error("invalid audit snapshot");
            const char* path = item->get_path();
            if (!path || !*path) throw std::runtime_error("missing audit path");
            abort_callback_dummy abort;
            service_ptr_t<input_info_reader> reader;
            input_entry::g_open_for_info_read(reader, nullptr, path, abort);
            file_info_impl actual;
            reader->get_info(0, actual, abort);
            const auto equal = [&actual](const file_info& expected) {
                return file_info::g_is_meta_equal(actual, expected) &&
                    replaygain_info::g_equal(actual.get_replaygain(), expected.get_replaygain());
            };
            if (equal(*payload.original_infos[i])) audit.record(state::original);
            else if (equal(*payload.infos_to_write[i])) audit.record(state::requested);
            else audit.record(state::divergent);
        } catch (...) {
            audit.record(state::unreadable);
        }
    }
    for (size_t i = inspect; i < total; ++i) audit.record(state::unreadable);
    return audit;
}

bool file_info_manager::read_infos() {
    // Clear previous snapshot permissions before any operation that may fail.
    begin_physical_read();
    if (items.get_count() != info_wrappers.get_count()) return false;
    if (discogger_inspect_file_targets(items, false) != discogger_write_safety::target_batch_decision::ready) return false;
    for (size_t i = 0; i < info_wrappers.get_count(); ++i)
        if (!info_wrappers[i]) return false;
	static_api_ptr_t<metadb_io> api;
	if (api->load_info_multi(items, metadb_io::load_info_default, core_api::get_main_window(), 0) != metadb_io::load_info_success) {
		return false;
	}
	const size_t m = items.get_count();
	size_t loaded_count = 0;
    for (size_t n = 0; n < m; n++) {
        bool val = items[n]->get_info(info_wrappers[n]->info);
        // Host metadb projections can lag behind physical writes for MP3 as
        // well as embedded-CUE FLAC. Never authorize a physical tag edit using
        // an outdated or cue-projected snapshot. Read physical subsong zero
        // through the host's read-only decoder for every supported target.
        // A decoder failure is a hard stop, not permission to use stale cache.
        if (val && items[n]->get_subsong_index() == 0) {
            try {
                const char* path = items[n]->get_path();
                if (!path || !*path) throw std::runtime_error("missing physical path");
                abort_callback_dummy abort;
                service_ptr_t<input_info_reader> reader;
                input_entry::g_open_for_info_read(reader, nullptr, path, abort);
                file_info_impl physical;
                reader->get_info(0, physical, abort);
                info_wrappers[n]->info = physical;
            } catch (...) {
                val = false;
            }
        }
        if (val) ++loaded_count;
        info_wrappers[n]->mask = val;
    }
    const bool complete = m_initial_read.finish(true, m, loaded_count);
    if (!complete) begin_physical_read(); // no partially loaded batch stays selected
    return complete;
}

void file_info_manager::read_infos_new() {
    // This async read has no validated complete-snapshot callback yet: fail closed.
    begin_physical_read();
    if (discogger_inspect_file_targets(items, false) != discogger_write_safety::target_batch_decision::ready)
        throw foo_discogs_exception("Reading tags stopped: invalid or empty file target.");
	static_api_ptr_t<metadb_io_v2> api;
	service_ptr_t<read_tags_completion> callback = new service_impl_t<read_tags_completion>(*this);
	api->load_info_async(items,
		metadb_io_v3::load_info_default, 
		core_api::get_main_window(), 
		0,
		callback);
}

void file_info_manager::write_infos(std::function<void()> after_success,
    discogger_write_safety::async_write_gate::lease write_lease,
    bool tag_deletions_approved) {
    if (!write_lease) throw foo_discogs_exception("Tagging stopped: no exclusive physical write lease");
	size_t n = 0;
	size_t outptr = 0;
	const size_t count = items.get_count();
    // Reject every unsafe target atomically before any handle dereference or I/O.
    discogger_require_physical_targets(items);
	// Validate the complete batch before dereferencing wrappers or scheduling I/O.
	const auto batch = discogger_write_safety::plan_snapshot_batch(
		count, info_wrappers.get_count(),
		[&](size_t i) { return static_cast<bool>(info_wrappers[i]); },
		[&](size_t i) { return info_wrappers[i]->mask; },
		[&](size_t i) { return info_wrappers[i]->original_captured; });
	switch (batch) {
	case discogger_write_safety::snapshot_batch_decision::ready: break;
	case discogger_write_safety::snapshot_batch_decision::reject_count_mismatch:
		throw foo_discogs_exception("Tagging stopped: physical file and metadata wrapper counts differ.");
	case discogger_write_safety::snapshot_batch_decision::reject_missing_wrapper:
		throw foo_discogs_exception("Tagging stopped: a physical metadata wrapper is missing.");
	case discogger_write_safety::snapshot_batch_decision::reject_missing_snapshot:
		throw foo_discogs_exception("Tagging stopped: original physical metadata snapshot is missing.");
	}
    // Freshness guard against an external tag editor changing a selected file
    // between Discogger's initial snapshot and its async commit. Check ALL
    // selected physical targets before staging even the first write. A failed
    // SDK read or mismatched metadata must reject the entire batch.
    for (size_t i = 0; i < count; ++i) {
        if (!info_wrappers[i]->mask) continue;
        try {
            const char* path = items[i]->get_path();
            if (!path || !*path)
                throw foo_discogs_exception("Tagging stopped: missing physical path.");
            abort_callback_dummy abort;
            service_ptr_t<input_info_reader> reader;
            input_entry::g_open_for_info_read(reader, nullptr, path, abort);
            file_info_impl current_physical;
            // The actual file-level tags live at physical subsong 0, including
            // when the input also exposes virtual tracks from an embedded CUE.
            reader->get_info(0, current_physical, abort);
            if (!file_info::g_is_meta_equal(
                    current_physical, info_wrappers[i]->original_info) ||
                !replaygain_info::g_equal(
                    current_physical.get_replaygain(),
                    info_wrappers[i]->original_info.get_replaygain()))
                throw foo_discogs_exception(
                    "Tagging stopped: physical tags changed since preview; reload before writing.");
        } catch (const foo_discogs_exception&) {
            throw;
        } catch (...) {
            throw foo_discogs_exception(
                "Tagging stopped: could not verify current physical file tags; reload before writing.");
        }
    }
    // Borrowed SDK arrays and their file_info payloads must survive the entire
    // async operation, even if a preview changes its working wrappers meanwhile.
    auto payload = std::make_shared<discogger_async_tag_payload>();
    payload->items_to_update.set_size(count);
    payload->infos_to_write.set_size(count);
    payload->original_infos.set_size(count);

	for (n = 0; n < count; n++) {
		const auto& wrapper = info_wrappers[n];
		if (!wrapper->mask) continue;
		if (!wrapper->original_captured) {
			throw foo_discogs_exception("Tagging stopped: original physical metadata snapshot is missing.");
		}
        // Defense in depth: even future direct writer callers cannot bypass the
        // explicit destructive-change approval or edit CUESHEET as a flat tag.
        const auto report = discogger_write_safety::inspect_tag_deletions(
            wrapper->original_info, wrapper->info, [](const char*) {});
        if (report.cuesheet_modified) {
            throw foo_discogs_exception("Tagging stopped: embedded CUESHEET requires a dedicated cue editor.");
        }
        if (!tag_deletions_approved && (report.removed_fields || report.removed_values)) {
            throw foo_discogs_exception("Tagging stopped: metadata removals were not explicitly approved.");
        }
		const bool metadata_equal = file_info::g_is_meta_equal(wrapper->original_info, wrapper->info);
		const bool replaygain_equal = replaygain_info::g_equal(
			wrapper->original_info.get_replaygain(), wrapper->info.get_replaygain());
		const auto decision = discogger_write_safety::plan_metadata_write(
			true, true, metadata_equal, replaygain_equal);
		if (decision == discogger_write_safety::metadata_write_decision::skip) continue;
        payload->items_to_update[outptr] = items[n];
        payload->infos_to_write[outptr] = payload->staged_infos.append_copy(wrapper->info);
        payload->original_infos[outptr] = payload->original_copies.append_copy(wrapper->original_info);
        ++outptr;
	}

    payload->submitted_count = outptr;
	if (outptr != 0) {
		static_api_ptr_t<metadb_io_v2> api;
        // Callback ownership keeps all staged file_info wrappers alive through I/O.
        auto keep_alive = shared_from_this();
        try {
            service_ptr_t<write_tags_completion> callback =
                new service_impl_t<write_tags_completion>(
                    std::move(after_success), std::move(write_lease), std::move(keep_alive), payload);
            api->update_info_async_simple(
                pfc::list_const_array_t<metadb_handle_ptr, const pfc::array_t<metadb_handle_ptr>&>(payload->items_to_update, outptr),
                pfc::list_const_array_t<const file_info*, const pfc::array_t<const file_info*>&>(payload->infos_to_write, outptr),
                core_api::get_main_window(), 0, callback);
        } catch (...) {
            // Scheduling failed synchronously: discard uncommitted in-memory tags.
            discard_pending_changes();
            throw;
        }
    }
    else if (after_success) {
        // There was no metadata write, but embedded artwork may still change
        // the physical file. Invalidate stale original-tag snapshots before
        // the artwork worker can run; only a complete fresh physical read
        // may authorize another tag commit.
        begin_physical_read();
        after_success();
    }
}

void write_tags_completion::on_completion(unsigned p_code) {
    if (m_gate.completed()) return; // Ignore duplicate SDK notifications.
    const bool success = p_code == metadb_io::update_info_success;
    // Failure cleanup can throw even after a partial physical SDK commit.
    // Keep its diagnostic without suppressing the bounded readback below.
    std::string callback_cleanup_error;
    try {
        m_gate.finish(success, [this]() {
            // An SDK success commits physical bytes. Never leave the old
            // pre-write snapshot eligible for a second metadata operation.
            // Revoke before dependent artwork starts or a callback throws.
            if (m_finfo_manager) m_finfo_manager->begin_physical_read();
            if (m_after_success) {
                // Reopen physical subsong zero on every submitted target
                // before running a dependent, potentially destructive
                // artwork operation. Never infer a complete batch commit
                // solely from an asynchronous SDK success notification.
                const size_t submitted = m_write_payload ? m_write_payload->submitted_count : 0;
                const auto audit = m_write_payload
                    ? discogger_audit_failed_write(*m_write_payload, submitted)
                    : discogger_write_safety::failed_write_audit{};
                if (!audit.fully_requested(submitted)) {
                    throw foo_discogs_exception(
                        "Dependent artwork withheld: physical tag commit was not fully verified. "
                        "Reload and inspect the files before another write.");
                }
                auto action = std::move(m_after_success);
                action();
            }
        }, [this]() {
            // In-memory reset only; partial physical writes require host verification.
            if (m_finfo_manager) m_finfo_manager->discard_pending_changes();
        });
    }
    catch (const std::exception& e) {
        callback_cleanup_error = e.what();
    }
    catch (...) {
        callback_cleanup_error = "Unknown callback cleanup or dependent artwork error";
    }
    if (!success) {
        // A failed callback is not evidence that the host rolled back all files.
        // The readback is deliberately bounded, non-writing and path-free.
        const auto audit = m_write_payload
            ? discogger_audit_failed_write(*m_write_payload)
            : discogger_write_safety::failed_write_audit{};
        std::string message =
            "Writing tags failed; dependent artwork was not started. "
            "No rollback or retry was attempted. Physical metadata readback "
            "(not a byte-integrity proof): unchanged=" + std::to_string(audit.original) +
            ", requested=" + std::to_string(audit.requested) +
            ", divergent=" + std::to_string(audit.divergent) +
            ", unverified=" + std::to_string(audit.unreadable) +
            ". Reload and inspect affected files before retrying.";
        if (!callback_cleanup_error.empty()) {
            message += " Callback cleanup also failed: ";
            message += callback_cleanup_error;
            message += ".";
        }
        add_error(message.c_str(), true);
        display_errors();
    } else if (!callback_cleanup_error.empty()) {
        const std::string message =
            "Physical tag commit reported success, but dependent callback failed: " +
            callback_cleanup_error;
        add_error(message.c_str(), true);
        display_errors();
    }
}

void read_tags_completion::on_completion(unsigned p_code) {
	if (p_code != metadb_io::load_info_success) {
		add_error("Error reading tags.");
		display_errors();
	}
}
