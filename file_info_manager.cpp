#include "stdafx.h"

#include "file_info_manager.h"
#include "discogger_write_safety.h"
#include <exception>
#include <utility>

bool file_info_manager::read_infos() {
	static_api_ptr_t<metadb_io> api;
	if (api->load_info_multi(items, metadb_io::load_info_default, core_api::get_main_window(), 0) != metadb_io::load_info_success) {
		return false;
	}
	const size_t m = items.get_count();
	size_t loaded_count = 0;
	for (size_t n = 0; n < m; n++) {
		bool val = items[n]->get_info(info_wrappers[n]->info);
		if (val) {
			loaded_count++;
		}
		info_wrappers[n]->mask = val;
	}
	return loaded_count == m;
}

void file_info_manager::read_infos_new() {
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
	// Defense in depth: reject a mixed physical/virtual batch before scheduling
	// any asynchronous write. Do not partially commit the physical members.
	if (discogger_write_safety::contains_virtual_subsong(count,
		[&](size_t index) { return items[index]->get_subsong_index(); })) {
		throw foo_discogs_exception("Tagging stopped: virtual CUE/subsong handles cannot be written through the physical metadata writer.");
	}
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
	pfc::array_t<metadb_handle_ptr> items_to_update;
	pfc::array_t<file_info *> infos_to_write;
	items_to_update.set_size(count);
	infos_to_write.set_size(count);

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
		items_to_update[outptr] = items[n];
		infos_to_write[outptr] = &wrapper->info;
		++outptr;
	}

	if (outptr != 0) {
		static_api_ptr_t<metadb_io_v2> api;
        // Callback ownership keeps all staged file_info wrappers alive through I/O.
        auto keep_alive = shared_from_this();
        try {
            service_ptr_t<write_tags_completion> callback =
                new service_impl_t<write_tags_completion>(
                    std::move(after_success), std::move(write_lease), std::move(keep_alive));
            api->update_info_async_simple(
                pfc::list_const_array_t<metadb_handle_ptr, const pfc::array_t<metadb_handle_ptr>&>(items_to_update, outptr),
                pfc::list_const_array_t<const file_info*, const pfc::array_t<const file_info*>&>(infos_to_write, outptr),
                core_api::get_main_window(), 0, callback);
        } catch (...) {
            // Scheduling failed synchronously: discard uncommitted in-memory tags.
            discard_pending_changes();
            throw;
        }
    }
    else if (after_success) {
        // No tag writer was scheduled; an explicitly requested artwork action
        // may still proceed. Artwork no-op detection remains a separate gate.
        after_success();
    }
}

void write_tags_completion::on_completion(unsigned p_code) {
    if (m_gate.completed()) return; // Ignore duplicate SDK notifications.
    const bool success = p_code == metadb_io::update_info_success;
    try {
        m_gate.finish(success, [this]() {
            if (m_after_success) {
                auto action = std::move(m_after_success);
                action();
            }
        }, [this]() {
            // In-memory reset only; partial physical writes require host verification.
            if (m_finfo_manager) m_finfo_manager->discard_pending_changes();
        });
    }
    catch (const std::exception& e) {
        add_error(e.what(), true);
        display_errors();
        return;
    }
    catch (...) {
        add_error("Tag write completed, but callback cleanup or dependent artwork failed.", true);
        display_errors();
        return;
    }
    if (!success) {
        add_error("Writing tags failed; dependent artwork was not started. Verify the physical files before retrying.", true);
        display_errors();
    }
}

void read_tags_completion::on_completion(unsigned p_code) {
	if (p_code != metadb_io::load_info_success) {
		add_error("Error reading tags.");
		display_errors();
	}
}
