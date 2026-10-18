#include "stdafx.h"

#include "file_info_manager.h"
#include "discogger_write_safety.h"

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

void file_info_manager::write_infos() {
	size_t n = 0;
	size_t outptr = 0;
	const size_t count = items.get_count();
	// Defense in depth: reject a mixed physical/virtual batch before scheduling
	// any asynchronous write. Do not partially commit the physical members.
	if (discogger_write_safety::contains_virtual_subsong(count,
		[&](size_t index) { return items[index]->get_subsong_index(); })) {
		throw foo_discogs_exception("Tagging stopped: virtual CUE/subsong handles cannot be written through the physical metadata writer.");
	}
	pfc::array_t<metadb_handle_ptr> items_to_update;
	pfc::array_t<file_info *> infos_to_write;
	items_to_update.set_size(count);
	infos_to_write.set_size(count);

	for (n = 0; n < count; n++) {
		if (info_wrappers[n]->mask) {
			items_to_update[outptr] = items[n];
			infos_to_write[outptr] = &info_wrappers[n]->info;
			outptr++;
		}
	}

	if (outptr != 0) {
		static_api_ptr_t<metadb_io_v2> api;
		service_ptr_t<write_tags_completion> callback = new service_impl_t<write_tags_completion>();
		api->update_info_async_simple(
			pfc::list_const_array_t<metadb_handle_ptr, const pfc::array_t<metadb_handle_ptr>&>(items_to_update, outptr),
			pfc::list_const_array_t<const file_info*, const pfc::array_t<const file_info*>&>(infos_to_write, outptr),
			core_api::get_main_window(),
			0,
			callback);
	}
}

void write_tags_completion::on_completion(unsigned p_code) {
	if (p_code != metadb_io::load_info_success) {
		add_error("Writing tags failed. Verify the physical files before retrying; artwork may have been started separately.", true);
		display_errors();
	}
}

void read_tags_completion::on_completion(unsigned p_code) {
	if (p_code != metadb_io::load_info_success) {
		add_error("Error reading tags.");
		display_errors();
	}
}
