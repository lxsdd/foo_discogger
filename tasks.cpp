#include "stdafx.h"
#include <unordered_set>
#include <functional>
#include <set>
#include <string>
#include <utility>
#include "discogs_interface.h"

#include "tags.h"
#include "art_download_attribs.h"
#include "ol_cache.h"
#include "configuration_dialog.h"
#include "crc.h"
#include "tasks.h"
#include "discogger_artist_load_ownership.h"
#include "discogger_write_safety.h"
#include "discogger_physical_targets.h"

void foo_discogs_threaded_process_callback::run(threaded_process_status &p_status, abort_callback &p_abort) {
	try {
		safe_run(p_status, p_abort);
	}
	catch (foo_discogs_exception &e) {
		add_error(e, true);
	}
	catch (...) {
		foo_discogs_exception ex;
		ex << "Unknown error running task.";
		add_error(ex, true);
	}
}

void foo_discogs_threaded_process_callback::on_done(HWND p_wnd, bool p_was_aborted) {
	if (p_was_aborted) {
		on_abort(p_wnd);
	}
	else {
		if (!fatal_error) {
			try {
				on_success(p_wnd);
			}
			catch (foo_discogs_exception &e) {
				add_error(e, true);
			}
		}
		if (errors.get_count()) {
			display_errors();
			if (fatal_error) {
				on_error(p_wnd);
			}
		}
	}
}

generate_tags_task::generate_tags_task(CPreviewTagsDialog *preview_dialog, TagWriter_ptr tag_writer) :
		m_tag_writer(tag_writer), m_preview_dialog(preview_dialog), m_show_preview_dialog(false), m_alt_mappings(nullptr) {
	//..
}

generate_tags_task::generate_tags_task(CTrackMatchingDialog *track_matching_dialog, TagWriter_ptr tag_writer, bool show_preview_dialog) :
		m_tag_writer(tag_writer), m_track_matching_dialog(track_matching_dialog), m_show_preview_dialog(show_preview_dialog), m_alt_mappings(nullptr) {

	//..
}
generate_tags_task::generate_tags_task(CTagCreditDialog* credits_dialog, TagWriter_ptr tag_writer, tag_mapping_list_type* alt_mappings) :
		m_tag_writer(tag_writer), m_alt_mappings(alt_mappings), m_credits_dialog(credits_dialog), m_show_preview_dialog(false) {

}

generate_tags_task::~generate_tags_task() {
	//..
}

void generate_tags_task::start() {
	threaded_process::g_run_modeless(
		this,
		threaded_process::flag_show_abort |
		threaded_process::flag_show_delayed |
		threaded_process::flag_show_progress |
		threaded_process::flag_show_item,
		core_api::get_main_window(),
		"Generating tags..."
	);
}

void generate_tags_task::safe_run(threaded_process_status& p_status, abort_callback& p_abort) {

	{
		std::lock_guard<std::mutex> guard(g_discogs->locked_tag_generation_rw_mutex);
		if (g_discogs->locked_tag_generation) {
			m_quick_abort = true;
			log_msg("tag generation is busy, try again later.");
			return;
		}
		else {
			g_discogs->locked_tag_generation = true;
		}
	}

	if (m_track_matching_dialog) {
		m_track_matching_dialog->enable(false);
	}
	else if (m_preview_dialog)  {
		m_preview_dialog->enable(false, true);
	}

	m_tag_writer->generate_tags(m_alt_mappings, p_status, p_abort);
}

void generate_tags_task::on_success(HWND p_wnd) {

	if (m_quick_abort) {
		return;
	}

	if (m_preview_dialog && m_preview_dialog->IsWindow()) {

		m_preview_dialog->Enabled(true);

		const size_t debug = TAGS.get_count();
		size_t new_res_count = m_tag_writer->tag_results.get_count();


		// pass tag writer tag mask -----------------------------------------------------------

		m_preview_dialog->cb_refresh_ui_tag_results(m_tag_writer->tag_results_mask);

		m_tag_writer->ResetMask();

		// preview exist generate list tags
		CPreviewLeadingTagDialog* preview_modal_tag_dialog = g_discogs->preview_modal_tag_dialog;
		if (preview_modal_tag_dialog && IsWindow(preview_modal_tag_dialog->m_hWnd)) {

			tag_result_ptr detailed_res;
			size_t new_sel = 0;
			size_t curr_sel = preview_modal_tag_dialog->GetResult(detailed_res);

			bool bmovetoprev = false;
			if (new_res_count <= curr_sel) {
				bmovetoprev = true;
			}
			else {

				bool new_enabled = m_tag_writer->tag_results[curr_sel]->tag_entry->enable_write || m_tag_writer->tag_results[curr_sel]->tag_entry->enable_update;
				if (!new_enabled) {
					bmovetoprev = true;
				}
			}

			if (bmovetoprev) {
				if (curr_sel - 1 < new_res_count)
					new_sel = curr_sel - 1 < 0 ? 0 : curr_sel - 1;
				else
					new_sel = new_res_count - 1;
			}
			else {
				new_sel = curr_sel;
			}

			const auto new_res_tag_entry = m_tag_writer->tag_results[new_sel]->tag_entry;
			preview_modal_tag_dialog->SetTagWriter(m_tag_writer);

			preview_modal_tag_dialog->ReloadItem(core_api::get_main_window(), new_sel, /*use current modal preview view mode*/PreView::Undef);

		}

		{
			std::lock_guard<std::mutex> guard(g_discogs->locked_tag_generation_rw_mutex);
			if (g_discogs->locked_tag_generation) {
				g_discogs->locked_tag_generation = false;
			}
			else {
				//.. should not be here
			}
		}
	}

	else if (m_show_preview_dialog) {

		// create/show preview and hide track matching dialog
		if (m_track_matching_dialog && m_track_matching_dialog->IsWindow()) {
			m_track_matching_dialog->enable(true);

			fb2k::newDialog <CPreviewTagsDialog>(core_api::get_main_window(), m_tag_writer);

			m_track_matching_dialog->hide();

		}

		{
			std::lock_guard<std::mutex> guard(g_discogs->locked_tag_generation_rw_mutex);
			if (g_discogs->locked_tag_generation) {
				g_discogs->locked_tag_generation = false;
			}
			else {
				//.. should not be here
			}
		}

	}
	else {

		//destroy track matching dialog and launch write tags process

		if (g_discogs->track_matching_dialog && g_discogs->track_matching_dialog->IsWindow()) {

			g_discogs->track_matching_dialog->destroy_all();

			try {
				service_ptr_t<write_tags_task> task = new service_impl_t<write_tags_task>(m_tag_writer);
				task->start();
			}
			catch (locked_task_exception e)
			{
				log_msg(e.what());
			}
		}

		{
			std::lock_guard<std::mutex> guard(g_discogs->locked_tag_generation_rw_mutex);
			if (g_discogs->locked_tag_generation) {
				g_discogs->locked_tag_generation = false;
			}
			else {
				//.. should not be here
			}
		}

	}
}

void generate_tags_task::on_abort(HWND p_wnd) {

	if (m_quick_abort) {
		return;
	}

	m_tag_writer->ResetMask();

	on_error(p_wnd);
}

void generate_tags_task::on_error(HWND p_wnd) {

	std::lock_guard<std::mutex> guard(g_discogs->locked_tag_generation_rw_mutex);
	if (g_discogs->locked_tag_generation) {
		g_discogs->locked_tag_generation = false;
	}
	else {
		//.. should not be here
	}

	m_tag_writer->ResetMask();
	//orphan $prompt modal dialog closed?
	//todo: global mutexes form global dialogs
	if (m_preview_dialog && m_preview_dialog->IsWindow()) {

		m_preview_dialog->enable(true, true);
	}
	else {
		if (m_track_matching_dialog && m_track_matching_dialog->IsWindow()) {
			m_track_matching_dialog->enable(true);
			m_track_matching_dialog->show();
		}
	}
}

void write_tags_task::start() {
	threaded_process::g_run_modeless(
		this,
		threaded_process::flag_show_abort |
		threaded_process::flag_show_delayed |
		threaded_process::flag_show_progress |
		threaded_process::flag_show_item,
		core_api::get_main_window(),
		"Writing tags..."
	);
}

void write_tags_task::safe_run(threaded_process_status &p_status, abort_callback &p_abort) {

    // A virtual/subsong projection, empty path, missing handle or duplicate
    // physical file must never reach the generic metadata tag writer.
    discogger_require_physical_targets(m_tag_writer->m_finfo_manager->items);
	m_tag_writer->write_tags();

}

void write_tags_task::on_success(HWND p_wnd) {
    // Revalidate on UI thread before destructive-change approval or artwork.
    discogger_require_physical_targets(m_tag_writer->m_finfo_manager->items);

    // Explicit user approval is required for any deletion, including a
    // reduction in the number of values of a multivalue field. This runs on
    // the UI callback thread before ANY asynchronous SDK write is scheduled.
    std::size_t removed_fields = 0, removed_values = 0;
    std::set<std::string> removed_names;
    const auto& finfo = m_tag_writer->m_finfo_manager;
    for (size_t index = 0; index < finfo->info_wrappers.get_count(); ++index) {
        const auto& wrapper = finfo->info_wrappers[index];
        if (!wrapper || !wrapper->mask || !wrapper->original_captured) continue;
        const auto report = discogger_write_safety::inspect_tag_deletions(
            wrapper->original_info, wrapper->info, [&](const char* name) {
                if (removed_names.size() >= 16 || !name) return;
                std::string safe;
                for (size_t c = 0; name[c] && c < 64; ++c) {
                    const unsigned char ch = static_cast<unsigned char>(name[c]);
                    safe.push_back(ch >= 32 && ch <= 126 ? static_cast<char>(ch) : '?');
                }
                removed_names.insert(std::move(safe));
            });
        if (report.cuesheet_modified) {
            finfo->discard_pending_changes();
            m_tag_writer->ResetMask();
            throw foo_discogs_exception("Writing cancelled: CUESHEET changes require a dedicated, tested cue editor. Original physical tags were not submitted.");
        }
        removed_fields += report.removed_fields;
        removed_values += report.removed_values;
    }
    bool tag_deletions_approved = false;
    if (removed_fields || removed_values) {
        pfc::string8 warning;
        warning << "Discogger is about to REMOVE " << removed_fields
                << " metadata field(s) and " << removed_values
                << " multivalue entry/entries from physical files.\n\nAffected fields:";
        for (const auto& name : removed_names) warning << "\n - " << name.c_str();
        if (removed_names.size() >= 16) warning << "\n - (additional fields may be affected)";
        warning << "\n\nThese removals cannot be undone automatically after a partial disk write."
                << "\nContinue with this destructive operation?";
        if (uMessageBox(p_wnd ? p_wnd : core_api::get_main_window(), warning.get_ptr(),
            "Discogger: confirm metadata removals",
            MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2 | MB_APPLMODAL) != IDYES) {
            finfo->discard_pending_changes();
            m_tag_writer->ResetMask();
            return;
        }
        tag_deletions_approved = true;
    }

	// Prepare optional artwork, but never launch it until the physical tag
	// writer reports success. The previous code launched it immediately.
	std::function<void()> artwork_after_tags;
	if (!m_tag_writer->tag_results_mask.size()) {

		int lpskip = CONF.album_art_skip_default_cust;
		bool lo_skip_global_defs = (LOWORD(lpskip)) & ARTSAVE_SKIP_USER_FLAG;
		bool user_wants_skip_write_artwork = (HIWORD(lpskip)) & ARTSAVE_SKIP_USER_FLAG;

		bool bskip_art = user_wants_skip_write_artwork || lo_skip_global_defs;

		bool bconf_art_save = (CONF.save_album_art || CONF.save_artist_art || CONF.embed_album_art || CONF.embed_artist_art);
		bool bcustom_art_save = !(CONF_MULTI_ARTWORK == multi_uartwork(CONF, m_tag_writer->GetRelease()));
		bool bfile_match = CONF_MULTI_ARTWORK.file_match;

		if (!bskip_art && (bconf_art_save || bcustom_art_save)) {
            const pfc::string8 release_id = m_tag_writer->GetRelease()->id;
            const metadb_handle_list physical_items = m_tag_writer->m_finfo_manager->items;
            const auto artwork_lease = m_write_lease;
            artwork_after_tags = [release_id, physical_items, bfile_match, artwork_lease]() {
                service_ptr_t<download_art_task> task =
                    new service_impl_t<download_art_task>(release_id, physical_items, bfile_match, artwork_lease);
                task->start();
            };
		}
	}
    m_tag_writer->m_finfo_manager->write_infos(std::move(artwork_after_tags), m_write_lease, tag_deletions_approved);

	if (g_discogs->preview_tags_dialog && ::IsWindow(g_discogs->preview_tags_dialog->m_hWnd)) {

		//todo: rev
		bool cfg_jump_to_original = false;
		if (cfg_jump_to_original) {
			HWND hwndOriginal = uGetDlgItem(g_discogs->preview_tags_dialog->m_hWnd, IDC_VIEW_ORIGINAL);
			SendMessage(hwndOriginal, BM_CLICK, 0, 0);
		}

		if (g_discogs->preview_modal_tag_dialog && ::IsWindow(g_discogs->preview_modal_tag_dialog->m_hWnd)) {
			DestroyWindow(g_discogs->preview_modal_tag_dialog->m_hWnd);
		}
		g_discogs->preview_tags_dialog->spawn_generate_tag_mappings();
	}
	else {
		m_tag_writer->ResetMask();
	}
}

void write_tags_task::on_abort(HWND p_wnd) {
	m_tag_writer->ResetMask();
	on_error(p_wnd);
}

void write_tags_task::on_error(HWND p_wnd) {
    m_tag_writer->m_finfo_manager->discard_pending_changes();
	m_tag_writer->ResetMask();
}


class att_vcmp {
	af m_att = af::alb_emb;
public:
	att_vcmp(af att_flg) : m_att(att_flg) {}
	void set(af att_flg) { m_att = att_flg; }

	std::function< bool(const uartwork&, const uartwork&) > comp_uart_att = [=](const uartwork& left, const uartwork& right) {
		// Lambda function to compare 2 uartworks
		bool is_album = false;

		if (m_att == af::alb_sd)
			return (left.ucfg_album_save_to_dir == right.ucfg_album_save_to_dir);
		if (m_att == af::alb_ovr)
			return (left.ucfg_album_ovr == right.ucfg_album_ovr);
		else if (m_att == af::alb_emb)
			return (left.ucfg_album_embed == right.ucfg_album_embed);
		else if (m_att == af::art_sd)
			return (left.ucfg_art_save_to_dir == right.ucfg_art_save_to_dir);
		else if (m_att == af::art_ovr)
			return (left.ucfg_art_ovr == right.ucfg_art_ovr);
		else if (m_att == af::art_emb)
			return (left.ucfg_art_embed == right.ucfg_art_embed);
		return false;
	};
};

download_art_task::download_art_task(pfc::string8 release_id, metadb_handle_list items, bool file_match,
    discogger_write_safety::async_write_gate::lease write_lease) :
    release_id(release_id), items(items), m_file_match(file_match),
    m_write_lease(write_lease ? std::move(write_lease) : discogger_write_safety::acquire_physical_write_lease()) {
    if (!m_write_lease) throw locked_task_exception("Another Discogger tag/artwork write is active");
}

void download_art_task::start() {
	threaded_process::g_run_modeless(
		this,
		threaded_process::flag_show_abort |
		threaded_process::flag_show_delayed |
		threaded_process::flag_show_progress |
		threaded_process::flag_show_item,
		core_api::get_main_window(),
		"Downloading album/artist art..."
	);
}

void download_art_task::safe_run(threaded_process_status& p_status, abort_callback& p_abort) {
    // The direct and chained artwork paths require the same physical target gate.
    discogger_require_physical_targets(items);

	size_t lkey = encode_mr(0, release_id);
	Release_ptr release = discogs_interface->get_release(lkey, p_status, p_abort);

	bool bfile_match = CONF_MULTI_ARTWORK.file_match;
	multi_uartwork multi_uartconf = multi_uartwork(CONF, release);

	bool bconf_album_save_or_embed = CONF.save_album_art || CONF.embed_album_art;
	bool bconf_artist_save_or_embed = CONF.save_artist_art || CONF.embed_artist_art;

	att_vcmp cust_cmp(af::alb_sd);
	bool bcust_album_save;
	bool bcust_album_embed;
	bool bcust_artist_save;
	bool bcust_artist_embed;

	if (CONF_MULTI_ARTWORK.vuart.size() != multi_uartconf.vuart.size()) {
		bcust_album_save = bcust_album_embed = true;
		bcust_artist_save = bcust_artist_embed = true;
	}
	else {
			bcust_album_save = bfile_match || !std::equal(CONF_MULTI_ARTWORK.vuart.begin(), CONF_MULTI_ARTWORK.vuart.end(), multi_uartconf.vuart.begin(), cust_cmp.comp_uart_att);
			cust_cmp.set(af::alb_ovr);
			bcust_album_save |= !std::equal(CONF_MULTI_ARTWORK.vuart.begin(), CONF_MULTI_ARTWORK.vuart.end(), multi_uartconf.vuart.begin(), cust_cmp.comp_uart_att);
			cust_cmp.set(af::alb_emb);
			bcust_album_embed = !std::equal(CONF_MULTI_ARTWORK.vuart.begin(), CONF_MULTI_ARTWORK.vuart.end(), multi_uartconf.vuart.begin(), cust_cmp.comp_uart_att);


			cust_cmp.set(af::art_sd);
			bcust_artist_save = bfile_match || !std::equal(CONF_MULTI_ARTWORK.vuart.begin(), CONF_MULTI_ARTWORK.vuart.end(), multi_uartconf.vuart.begin(), cust_cmp.comp_uart_att);
			cust_cmp.set(af::art_ovr);
			bcust_artist_save |= !std::equal(CONF_MULTI_ARTWORK.vuart.begin(), CONF_MULTI_ARTWORK.vuart.end(), multi_uartconf.vuart.begin(), cust_cmp.comp_uart_att);
			cust_cmp.set(af::art_emb);
			bcust_artist_embed = !std::equal(CONF_MULTI_ARTWORK.vuart.begin(), CONF_MULTI_ARTWORK.vuart.end(), multi_uartconf.vuart.begin(), cust_cmp.comp_uart_att);
	}
	bool bcust_album_save_or_embed = bcust_album_save || bcust_album_embed;
	bool bcust_artist_save_or_embed = bcust_artist_save || bcust_artist_embed;

	if (bconf_album_save_or_embed || bcust_album_save_or_embed) {

		art_download_attribs ada_mod;
		pfc::array_t<pfc::string8> done_files;
		std::map<pfc::string8, MemoryBlock> done_fetches;

		size_t cartist_art = 0;

		for (ReleaseArtist_ptr wra : release->artists) {
			cartist_art += wra->full_artist->images.get_count();
		}

		bit_array_bittable dummy_saved_mask(release->images.get_count() + cartist_art);

		for (size_t i = 0; i < items.get_count(); i++) {

			bool bcall = false;
			bool bfile_match = m_file_match;

			if (bconf_album_save_or_embed && !bcust_album_save_or_embed) {

				ada_mod = art_download_attribs(CONF.save_album_art, CONF.embed_album_art, CONF.album_art_overwrite, CONF.album_art_fetch_all, false, {}, bfile_match, bcust_album_save_or_embed);
				bcall = true;
			}
			else {

				bool bitem_write = CONF_MULTI_ARTWORK.save_to_dir(art_src::alb);
				bool bitem_embed = CONF_MULTI_ARTWORK.embed_art(art_src::alb);
				bool bitem_overwrite = CONF_MULTI_ARTWORK.ovr_art(art_src::alb);
				bool bitem_fetch_all = true;

				if (bitem_write || bitem_embed) {
					ada_mod = art_download_attribs(bitem_write, bitem_embed, bitem_overwrite, bitem_fetch_all /*always true, enter image loop*/, false, {}, bfile_match, bcust_album_save_or_embed);
					bcall = true;
				}
			}

			if (bcall) {
				g_discogs->save_album_art(release, items[i], ada_mod, CONF_ARTWORK_GUIDS /*pfc::array_t<GUID>()*/, dummy_saved_mask, done_files, done_fetches, p_status, p_abort);
			}
		}
	}

	if (bconf_artist_save_or_embed || bcust_artist_save_or_embed) {

		art_download_attribs ada_mod;
		pfc::array_t<pfc::string8> done_files;
		std::map<pfc::string8, MemoryBlock> done_fetches;

		size_t cartist_art = 0;

		for (ReleaseArtist_ptr wra : release->artists) {
			cartist_art += wra->full_artist->images.get_count();
		}

		bit_array_bittable dummy_saved_mask(release->images.get_count() + cartist_art);

		for (size_t i = 0; i < items.get_count(); i++) {

			bool bcall = false;
			bool bfile_match = m_file_match;

			if (bconf_artist_save_or_embed && !bcust_artist_save_or_embed) {

				ada_mod = art_download_attribs(CONF.save_artist_art, CONF.embed_artist_art, CONF.artist_art_overwrite, CONF.artist_art_fetch_all, false, {}, bfile_match, bcust_artist_save_or_embed);
				bcall = true;
			}
			else {

				bool bitem_write = CONF_MULTI_ARTWORK.save_to_dir(art_src::art);
				bool bitem_embed = CONF_MULTI_ARTWORK.embed_art(art_src::art);
				bool bitem_overwrite = CONF_MULTI_ARTWORK.ovr_art(art_src::art);
				bool bitem_fetch_all = true;

				if (bitem_write || bitem_embed) {
					ada_mod = art_download_attribs(bitem_write, bitem_embed, bitem_overwrite, bitem_fetch_all /*always true, need to loop all images*/, false, {}, bfile_match, bcust_artist_save_or_embed);
					bcall = true;
				}
			}

			if (bcall) {
				g_discogs->save_artist_art(release, items[i], ada_mod, CONF_ARTWORK_GUIDS /*pfc::array_t<GUID>()*/, dummy_saved_mask, done_files, done_fetches, p_status, p_abort);
			}
		}
	}
}

download_art_paths_task::download_art_paths_task(CTrackMatchingDialog* m_dialog, pfc::string8 release_id, 
	metadb_handle_list items, pfc::array_t<GUID> album_art_ids, bool to_path_only, bool file_match) :
	m_dialog(m_dialog), m_release_id(release_id), m_items(items),
	m_album_art_ids(album_art_ids), m_to_path_only(to_path_only), m_file_match(file_match),
    m_write_lease(to_path_only ? discogger_write_safety::async_write_gate::lease{}
                               : discogger_write_safety::acquire_physical_write_lease()) {

}

void download_art_paths_task::start() {
	threaded_process::g_run_modeless(
		this,
		threaded_process::flag_show_abort |
		threaded_process::flag_show_delayed |
		threaded_process::flag_show_progress |
		threaded_process::flag_show_item,
		core_api::get_main_window(),
		"Downloading album/artist art..."
	);
}

void download_art_paths_task::safe_run(threaded_process_status& p_status, abort_callback& p_abort) {
    if (!m_to_path_only) {
        if (!m_write_lease) {
            throw locked_task_exception("Another Discogger physical tag/artwork write is still active.");
        }
        discogger_require_physical_targets(m_items);
    } else if (discogger_inspect_file_targets(m_items, false) != discogger_write_safety::target_batch_decision::ready) {
        // Previews may include CUE subsongs and multiple tracks per file;
        // reject only unusable handles/paths, never perform a physical write.
        throw foo_discogs_exception("Artwork preview stopped: invalid or empty file target.");
    }

	size_t lkey = encode_mr(0, m_release_id);
	Release_ptr release = discogs_interface->get_release(lkey, p_status, p_abort);

	bit_array_bittable saved_mask(m_album_art_ids.size());
	bool bfile_match = CONF_MULTI_ARTWORK.file_match;

	multi_uartwork multi_uartconf = multi_uartwork(CONF, release);

	if (!multi_uartconf.vuart.size()) {
		//do not save artwork configuration ?
		return;
	}

	bool bconf_album_save_or_embed = CONF.save_album_art || CONF.embed_album_art;
	bool bconf_artist_save_or_embed = CONF.save_artist_art || CONF.embed_artist_art;

	att_vcmp cust_cmp(af::alb_sd);
	bool bcust_album_save;
	bool bcust_album_embed;
	bool bcust_artist_save;
	bool bcust_artist_embed;

	if (CONF_MULTI_ARTWORK.vuart.size() != multi_uartconf.vuart.size()) {
		bcust_album_save = bcust_album_embed = true;
		bcust_artist_save = bcust_artist_embed = true;
	}
	else {
		bcust_album_save = bfile_match || !std::equal(CONF_MULTI_ARTWORK.vuart.begin(), CONF_MULTI_ARTWORK.vuart.end(), multi_uartconf.vuart.begin(), cust_cmp.comp_uart_att);
		cust_cmp.set(af::alb_ovr);
		bcust_album_save |= !std::equal(CONF_MULTI_ARTWORK.vuart.begin(), CONF_MULTI_ARTWORK.vuart.end(), multi_uartconf.vuart.begin(), cust_cmp.comp_uart_att);
		cust_cmp.set(af::alb_emb);
		bcust_album_embed = !std::equal(CONF_MULTI_ARTWORK.vuart.begin(), CONF_MULTI_ARTWORK.vuart.end(), multi_uartconf.vuart.begin(), cust_cmp.comp_uart_att);


		cust_cmp.set(af::art_sd);
		bcust_artist_save = bfile_match || !std::equal(CONF_MULTI_ARTWORK.vuart.begin(), CONF_MULTI_ARTWORK.vuart.end(), multi_uartconf.vuart.begin(), cust_cmp.comp_uart_att);
		cust_cmp.set(af::art_ovr);
		bcust_artist_save |= !std::equal(CONF_MULTI_ARTWORK.vuart.begin(), CONF_MULTI_ARTWORK.vuart.end(), multi_uartconf.vuart.begin(), cust_cmp.comp_uart_att);
		cust_cmp.set(af::art_emb);
		bcust_artist_embed = !std::equal(CONF_MULTI_ARTWORK.vuart.begin(), CONF_MULTI_ARTWORK.vuart.end(), multi_uartconf.vuart.begin(), cust_cmp.comp_uart_att);
	}
	bool bcust_album_save_or_embed = bcust_album_save || bcust_album_embed;
	bool bcust_artist_save_or_embed = bcust_artist_save || bcust_artist_embed;

	if (bconf_album_save_or_embed || bcust_album_save_or_embed) {

		pfc::array_t<pfc::string8> done_files;
		std::map<pfc::string8, MemoryBlock> done_fetches;
		art_download_attribs ada_mod;

		for (size_t i = 0; i < m_items.get_count(); i++) {

			bool becall = false;
			bool bfile_match = m_file_match;

			if (bconf_album_save_or_embed && !bcust_album_save_or_embed) {

				ada_mod = art_download_attribs(CONF.save_album_art, CONF.embed_album_art, CONF.album_art_overwrite, CONF.album_art_fetch_all, m_to_path_only, {}, bfile_match, bcust_album_save_or_embed);
				becall = true;
			}
			else {

				bool bitem_write = CONF_MULTI_ARTWORK.save_to_dir(art_src::alb);
				bool bitem_embed = CONF_MULTI_ARTWORK.embed_art(art_src::alb);
				bool bitem_overwrite = CONF_MULTI_ARTWORK.ovr_art(art_src::alb);
				bool bitem_fetch_all = true;

				if (bitem_write || bitem_embed) {
					ada_mod = art_download_attribs(bitem_write, bitem_embed, bitem_overwrite, true /*always true to image loop*/, m_to_path_only, {}, bfile_match, bcust_album_save_or_embed);
					becall = true;
				}
			}

			if (becall) {

				g_discogs->save_album_art(release, m_items[i], ada_mod, m_album_art_ids, saved_mask, done_files, done_fetches, p_status, p_abort);

				for (size_t walk = 0; walk < done_files.size(); walk++) {
					m_vres.emplace_back(std::pair(done_files[walk].get_ptr(), saved_mask));
				}
			}
		}
	}

	if (bconf_artist_save_or_embed || bcust_artist_save_or_embed) {

		art_download_attribs ada_mod;
		pfc::array_t<pfc::string8> done_files;
		std::map<pfc::string8, MemoryBlock> done_fetches;

		for (size_t i = 0; i < m_items.get_count(); i++) {

			bool bcall = false;
			bool bfile_match = m_file_match;

			if (bconf_artist_save_or_embed && !bcust_artist_save_or_embed) {

				ada_mod = art_download_attribs(CONF.save_artist_art, CONF.embed_artist_art, CONF.artist_art_overwrite, CONF.artist_art_fetch_all, m_to_path_only, {}, bfile_match, bcust_artist_save_or_embed);
				bcall = true;
			}
			else {

				bool bitem_write = CONF_MULTI_ARTWORK.save_to_dir(art_src::art);
				bool bitem_embed = CONF_MULTI_ARTWORK.embed_art(art_src::art);
				bool bitem_overwrite = CONF_MULTI_ARTWORK.ovr_art(art_src::art);
				bool bitem_fetch_all = true;

				if (bitem_write || bitem_embed) {

					ada_mod = art_download_attribs(bitem_write, bitem_embed, bitem_overwrite, bitem_fetch_all /*always true to loop all images*/, m_to_path_only, {}, bfile_match, bcust_artist_save_or_embed);
					bcall = true;
				}
			}

			if (bcall) {

				g_discogs->save_artist_art(release, m_items[i], ada_mod, m_album_art_ids, saved_mask, done_files, done_fetches, p_status, p_abort);

				for (size_t walk = 0; walk < done_files.size(); walk++) {
					m_vres.emplace_back(std::pair(done_files[walk].get_ptr(), saved_mask));
				}
			}
		}
	}
}

void download_art_paths_task::on_success(HWND p_wnd) {

	std::shared_ptr<std::vector<std::pair<pfc::string8, bit_array_bittable>>> vpaths = std::make_shared<std::vector<std::pair<pfc::string8, bit_array_bittable>>>(m_vres);
#ifdef SIM_VA_MA_BETA
	CONF.find_release_dlg_flags &= ~CFindReleaseDialog::FLG_VA_AS_MA;
	CONF.save(CConf::cfgFilter::CONF, CONF, CFG_FIND_RELEASE_DIALOG_FLAG);

	if (g_discogs->configuration_dialog && IsWindow(g_discogs->configuration_dialog->m_hWnd)) {
		CConfigurationDialog* cfgdlg = static_cast<CConfigurationDialog*>(g_discogs->configuration_dialog);
		BOOL bdummy = 0;
		cfgdlg->OnCustomMsg_VA_as_MA_Reset(0, 0, 0, bdummy);
	}
#endif

	if (IsWindow(m_dialog->m_hWnd)) {
		m_dialog->process_download_art_paths_done(m_release_id, vpaths, m_album_art_ids);
	}
}


find_deleted_releases_task::find_deleted_releases_task(metadb_handle_list items) : m_items(items), m_finfo_manager(items) {
	m_finfo_manager.read_infos();
}

void find_deleted_releases_task::start() {
	threaded_process::g_run_modeless(
		this,
		threaded_process::flag_show_abort |
		threaded_process::flag_show_delayed |
		threaded_process::flag_show_progress |
		threaded_process::flag_show_item,
		core_api::get_main_window(),
		"Finding deleted releases..."
	);
}

void find_deleted_releases_task::safe_run(threaded_process_status &p_status, abort_callback &p_abort) {
	const size_t item_count = m_finfo_manager.get_item_count();

	pfc::string8 release_id;
	pfc::array_t<pfc::string8> finished;
	size_t finished_count = 0;

	for (size_t i = 0; i < item_count && !p_abort.is_aborting(); i++) {
		metadb_handle_ptr item = m_finfo_manager.get_item_handle(i);
		pfc::string8 formatted_release_name;
		item->format_title(nullptr, formatted_release_name, g_discogs->release_name_script, nullptr);

		p_status.set_item(formatted_release_name.get_ptr());
		p_status.set_progress(i + 1, item_count);

		file_info &finfo = m_finfo_manager.get_item(i);

		if (!g_discogs->file_info_get_tag(item, finfo, TAG_RELEASE_ID, release_id)) {
			continue;
		}

		bool duplicate = false;
		for (size_t j = 0; j < finished_count; j++) {
			if (STR_EQUAL(finished[j], release_id)) {
				duplicate = true;
				break;
			}
		}
		if (duplicate) {
			continue;
		}

		finished.append_single(release_id);
		finished_count++;

		try {
			size_t lkey = encode_mr(0, release_id);
			discogs_interface->get_release(lkey, p_status, p_abort, true, true);
		}
		catch (http_404_exception) {
			m_deleted_items.add_item(item);
		}
		catch (network_exception &e) {
			pfc::string8 error("release ");
			error << release_id;
			add_error(error, e, true);
			return;
		}
		catch (foo_discogs_exception &e) {
			pfc::string8 error("release ");
			error << release_id;
			add_error(error, e); //true);
		}
	}
}

void find_deleted_releases_task::on_success(HWND p_wnd) {
	finish();
}

void find_deleted_releases_task::on_abort(HWND p_wnd) {
}

void find_deleted_releases_task::on_error(HWND p_wnd) {
	finish();
}

void find_deleted_releases_task::finish() {
	if (m_deleted_items.get_count() > 0) {
		static_api_ptr_t<playlist_manager> pm;
		size_t playlist_id = pm->create_playlist("foo_discogger: invalid release ids", ~0, ~0);

		bit_array_true dummy;
		pm->playlist_insert_items(playlist_id, 0, m_deleted_items, dummy);
		pm->set_active_playlist(playlist_id);
	}
}


find_releases_not_in_collection_task::find_releases_not_in_collection_task(metadb_handle_list items) : items(items), m_finfo_manager(items) {
	m_finfo_manager.read_infos();
}

void find_releases_not_in_collection_task::start() {
	threaded_process::g_run_modeless(
		this,
		threaded_process::flag_show_abort |
		threaded_process::flag_show_delayed |
		threaded_process::flag_show_progress |
		threaded_process::flag_show_item,
		core_api::get_main_window(),
		"Finding releases not in collection..."
		);
}

void find_releases_not_in_collection_task::safe_run(threaded_process_status &p_status, abort_callback &p_abort) {
	p_status.set_progress(0, 1);
	
	const pfc::array_t<pfc::string8> &collection = discogs_interface->get_collection(p_status, p_abort);
	const size_t collection_count = collection.get_count();

	p_status.set_item("Checking for missing releases...");
	p_status.set_progress(1, 1);
	
	const size_t item_count = m_finfo_manager.get_item_count();
	pfc::string8 release_id;

	for (size_t i = 0; i < item_count && !p_abort.is_aborting(); i++) {
		file_info &finfo = m_finfo_manager.get_item(i);
		metadb_handle_ptr item = m_finfo_manager.get_item_handle(i);

		if (!g_discogs->file_info_get_tag(item, finfo, TAG_RELEASE_ID, release_id)) {
			continue;
		}

		bool found = false;
		for (size_t j = 0; j < collection_count; j++) {
			if (STR_EQUAL(collection[j], release_id)) {
				found = true;
				break;
			}
		}
		if (!found) {
			missing_items.add_item(item);
		}
	}
}

void find_releases_not_in_collection_task::on_success(HWND p_wnd) {
	finish();
}

void find_releases_not_in_collection_task::on_abort(HWND p_wnd) {
}

void find_releases_not_in_collection_task::on_error(HWND p_wnd) {
	finish();
}

void find_releases_not_in_collection_task::finish() {
	if (missing_items.get_count() > 0) {
		static_api_ptr_t<playlist_manager> pm;
		size_t playlist_id = pm->create_playlist("foo_discogger: not in collection", ~0, ~0);

		bit_array_true dummy;
		pm->playlist_insert_items(playlist_id, 0, missing_items, dummy);
		pm->set_active_playlist(playlist_id);
	}
}


void get_artist_process_callback::start(HWND parent) {
    m_owner_window = parent;
	pfc::string8 msg;
	msg << "Loading artist id " << m_artist_id << ": " << m_artist_name;

	threaded_process::g_run_modeless(this,
		threaded_process::flag_show_item |
		threaded_process::flag_show_abort |
		threaded_process::flag_show_delayed,
		parent,
		msg
	);
}

bool get_artist_process_callback::check_in() {
	std::lock_guard lg(g_discogs->mx_vArtistLoadReleasesTasks);
	auto found_it =
		std::find_if(g_discogs->vArtistLoadReleasesTasks.begin(), g_discogs->vArtistLoadReleasesTasks.end(), [&](const pfc::string8& e) {
		return e.equals(m_artist_id); });
	bool found = found_it != std::end(g_discogs->vArtistLoadReleasesTasks);

	if (found) {
		return false;
	}
	else {
		g_discogs->vArtistLoadReleasesTasks.emplace_back(m_artist_id);
	}
	return true;
}

void get_artist_process_callback::check_out() {
	if (!m_artist_load_registered) return;
	std::lock_guard lg(g_discogs->mx_vArtistLoadReleasesTasks);
	auto found_it =
		std::find_if(g_discogs->vArtistLoadReleasesTasks.begin(), g_discogs->vArtistLoadReleasesTasks.end(), [&](const pfc::string8& e) {
		return e.equals(m_artist_id); });
	bool found = found_it != std::end(g_discogs->vArtistLoadReleasesTasks);
	if (found) {
		g_discogs->vArtistLoadReleasesTasks.erase(found_it);
	}
	m_artist_load_registered = false;
}

void get_artist_process_callback::safe_run(threaded_process_status &p_status, abort_callback &p_abort) {

	if (m_bload_releases) {
		m_artist_load_registered = check_in();
		if (!m_artist_load_registered) {
			return;
		}
	}

	bool bsimple_list_sel = (m_cupdsrc == updRelSrc::ArtistProfile) && !m_bload_releases && !m_cupdsrc.oninit;

	m_artist = discogs_interface->get_artist(m_artist_id, m_bload_releases, p_status, p_abort,
		false, false, m_cupdsrc != updRelSrc::ArtistProfile, bsimple_list_sel);


	pfc::string8 status_msg;
	if (!(m_artist->loaded_preview || m_artist->loaded)) {
		status_msg = "artist details are not available.";
	}
	else if (m_artist->loaded_releases) {
		status_msg = "formatting artist releases...";
	}
	else {
		status_msg = "formatting artist preview...";
	}

	p_status.set_item(status_msg);
}

void get_artist_process_callback::on_success(HWND p_wnd) {

	// Release our marker even if the SDK returned a null artist. A duplicate
	// task never acquired the marker and must not cancel the real owner's load.
	if (m_bload_releases) {
		check_out();
	}

	if (m_artist) {

        // Honor a request made while an earlier artist load was running.
        const bool retry_refresh = m_bload_releases && m_artist->release_refresh.pending();

        // A finished network task must not target a closed/replaced dialog.
        if (!m_owner_window || !::IsWindow(m_owner_window) ||
            !g_discogs || !g_discogs->find_release_dialog ||
            g_discogs->find_release_dialog->m_hWnd != m_owner_window)
            return;

		//artist history
		//todo: only using search button
		if (m_cupdsrc != updRelSrc::ArtistProfile) {
			g_discogs->find_release_dialog->add_history(oplog_type::artist, kHistoryGetArtist, "", "", m_artist->id, m_artist->name);
		}

		try {

			service_ptr_t<on_get_artist_done_process_callback> task =
				new service_impl_t<on_get_artist_done_process_callback>(m_artist, m_cupdsrc);

			task->start(g_discogs->find_release_dialog->m_hWnd);
		}
		catch (locked_task_exception e)
		{
			log_msg(e.what());
		}
        if (retry_refresh && g_discogs && g_discogs->find_release_dialog) {
            CFindReleaseDialog* dlg = g_discogs->find_release_dialog;
            if (::IsWindow(dlg->m_hWnd) &&
                dlg->m_alist.Get_Selected_Id().equals(m_artist->id)) {
                cupdRelSrc refresh(updRelSrc::ArtistList);
                refresh.extended = true;
                dlg->convey_artist_list_selection(refresh);
            }
        }
	}
}

void get_artist_process_callback::on_abort(HWND p_wnd) {

	if (m_bload_releases) {
		check_out();
	}
}

void get_artist_process_callback::on_error(HWND p_wnd) {

	on_abort(p_wnd);
}


void get_multi_artists_process_callback::start(HWND parent) {
    m_owner_window = parent;
	pfc::string8 msg;
	std::string msg_ids;
	for (const auto& artist_id : m_artist_ids) msg_ids += (std::to_string(artist_id) + " ");

	msg << "Loading multiple artists " << msg_ids.c_str();

	threaded_process::g_run_modeless(this,
		threaded_process::flag_show_item |
		threaded_process::flag_show_abort |
		threaded_process::flag_show_delayed,
		parent,
		msg
	);
}

bool get_multi_artists_process_callback::check_in() {
	std::lock_guard lg(g_discogs->mx_vArtistLoadReleasesTasks);
	for (size_t artist_id : m_artist_ids) {
		pfc::string8 walk_str_id; walk_str_id << artist_id;
		auto found_it =
			std::find_if(g_discogs->vArtistLoadReleasesTasks.begin(), g_discogs->vArtistLoadReleasesTasks.end(), [&](const pfc::string8& e) {
			return e.equals(walk_str_id); });
		bool found = found_it != std::end(g_discogs->vArtistLoadReleasesTasks);

		if (found) {
			continue;
		}
		else {
			pfc::string8 walk_str_id; walk_str_id << artist_id;
			m_checked_in_artist_ids.emplace_back(walk_str_id);
			g_discogs->vArtistLoadReleasesTasks.emplace_back(std::move(walk_str_id));
		}
	}
	return !m_checked_in_artist_ids.empty();
}

void get_multi_artists_process_callback::check_out() {
    // A completion and a later abort/error may both arrive. Only remove
    // markers this particular task still owns, and consume that ownership
    // exactly once while the shared registry mutex is held.
    std::lock_guard lg(g_discogs->mx_vArtistLoadReleasesTasks);
    discogger_async::release_owned_ids(
        m_checked_in_artist_ids, g_discogs->vArtistLoadReleasesTasks,
        [](const pfc::string8& owned, const pfc::string8& registered) {
            return owned.equals(registered);
        });
}

void get_multi_artists_process_callback::safe_run(threaded_process_status& p_status, abort_callback& p_abort) {

	if (m_bload_releases) {
		if (!check_in()) {
			return;
		}
	}

	size_t count = 0;
	for (auto artist_id : m_artist_ids) {

		pfc::string8 status_title("Get multi-artists... ");
		status_title << (PFC_string_formatter() << count + 1 << " of " << m_artist_ids.size()).c_str();
		p_status.set_title(status_title);
		//load releases on first artist
		Artist_ptr artist = discogs_interface->get_artist(std::to_string(artist_id).c_str(), !count++ ? m_cupdsrc.extended : m_bload_releases, p_status, p_abort,
			false, false, m_cupdsrc != updRelSrc::ArtistProfile && m_bload_releases);

		m_artists.add_item(std::move(artist));
	}

	p_status.set_item("formatting artists preview...");
}

void get_multi_artists_process_callback::on_success(HWND p_wnd) {

	if (m_bload_releases) {
		check_out();
	}
    if (!m_owner_window || !::IsWindow(m_owner_window) ||
        !g_discogs || !g_discogs->find_release_dialog ||
        g_discogs->find_release_dialog->m_hWnd != m_owner_window)
        return;

	if (m_artists.get_count()) {
		m_cupdsrc.oninit = true;
		g_discogs->find_release_dialog->on_get_artist_done(m_cupdsrc, m_artists[0]);
		if (m_artists.get_count() > 1) {
			pfc::array_t<Artist_ptr>tail; tail.resize(m_artists.get_count() - 1);
			int c = 1;
			for (auto & ti : tail) {
				ti.swap(m_artists[c++]);
			}
			m_artists.force_reset();
            // The first callback may have closed the modeless window.
            if (::IsWindow(m_owner_window) && g_discogs &&
                g_discogs->find_release_dialog &&
                g_discogs->find_release_dialog->m_hWnd == m_owner_window)
                g_discogs->find_release_dialog->on_search_artist_done(tail, pfc::array_t<Artist_ptr>(), true);
		}
	}
}

void get_multi_artists_process_callback::on_error(HWND p_wnd) {

	if (m_bload_releases) {
		check_out();
	}

	pfc::array_t<Artist_ptr>tail;

	m_artists.force_reset();
    if (!m_owner_window || !::IsWindow(m_owner_window) ||
        !g_discogs || !g_discogs->find_release_dialog ||
        g_discogs->find_release_dialog->m_hWnd != m_owner_window)
        return;
	g_discogs->find_release_dialog->on_search_artist_done(pfc::array_t<Artist_ptr>(), pfc::array_t<Artist_ptr>(), true);

}

void get_multi_artists_process_callback::on_abort(HWND p_wnd) {

	if (m_bload_releases) {
		check_out();
	}
}


search_artist_process_callback::search_artist_process_callback(const char* search, bool dlgbutton, const int searchmode, const QueryDefMap qdm_search_query, const int db_dc_flags)
	: m_search(search), m_dlgbutton(dlgbutton), m_va(searchmode), m_qdm_search_query(qdm_search_query), m_db_dc_flags(db_dc_flags) {
	//..
}

search_artist_process_callback::~search_artist_process_callback() {
	//..
}

void search_artist_process_callback::start(HWND parent) {

	pfc::string8 pop_title;
	if (m_va) {
		FieldValPair exp_pair;
		search_query::MapToText(m_qdm_search_query, exp_pair);
		pop_title << "Advanced search... " << exp_pair.first.c_str();
	}
	else {
		pop_title << "Searching artist...  " << m_search;
	}

	threaded_process::g_run_modeless(this,
		threaded_process::flag_show_item |
		threaded_process::flag_show_abort,
		parent,
		PFC_string_formatter() << pop_title
	);
}

void search_artist_process_callback::safe_run(threaded_process_status &p_status, abort_callback &p_abort) {

	pfc::string8 msg;

	msg << "fetching online artist list...";

 	if (m_va & SearchMode::AT) {

		m_res_searchquery = 
				discogs_interface->search_amt_artist(m_search, m_qdm_search_query, m_artist_exact_matches, m_artist_other_matches, p_status, p_abort);
	}
	else {

		if (m_va & SearchMode::VA) {
			// todo
			return;
		}
		else {
			discogs_interface->search_artist(m_search, m_artist_exact_matches, m_artist_other_matches, p_status, p_abort);
		}

	}

	p_status.set_item(msg);
}

void search_artist_process_callback::on_success(HWND p_wnd) {

	CFindReleaseDialog* find_dlg = g_discogs->find_release_dialog;

	try {

		std::pair<size_t, size_t> res_cap;

		if (m_va & SearchMode::AT) {

			// AT

			res_cap = m_res_searchquery.first;

			FieldValPair exp_pair;
			search_query::MapToText(m_qdm_search_query, exp_pair, false);

			find_dlg->UpdateSearchQueryHasRun(exp_pair.first.c_str());
			find_dlg->UpdateSearchArtistHasRun("");

			if (m_dlgbutton) {

				// save query to search history

				std::string const  s = exp_pair.first;
				pfc::string8 scrc;
				std::ostringstream os;
				os << std::hex << std::setw(8) << std::setfill('0') << discogger_crc::crc32(s.begin(), s.end());
				scrc = os.str().c_str();

				find_dlg->add_history(oplog_type::query, kHistorySearchQuery, "", "",
					scrc, exp_pair.first.c_str());
			}

		}
		else {

			// DEFAULT

			g_discogs->find_release_dialog->m_query_mode = 0;

			// set result stat
			res_cap = m_res_searchquery.second;

			find_dlg->UpdateSearchQueryHasRun("");
			find_dlg->UpdateSearchArtistHasRun(m_search);

			if (m_dlgbutton) {

				find_dlg->UpdateSearchModeDisplayMode(nullptr);

				// save artist to search history

				if (m_artist_exact_matches.size()) {
					//artist history
					find_dlg->add_history(oplog_type::artist, kHistorySearchArtist, "", "",
						m_artist_exact_matches[0]->id, m_artist_exact_matches[0]->name);
				}
			}
		}

		service_ptr_t<on_search_artist_done_process_callback> task =
			new service_impl_t<on_search_artist_done_process_callback>(m_artist_exact_matches, m_artist_other_matches, false,
				res_cap);

		task->start(find_dlg->m_hWnd);
	}
	catch (locked_task_exception e)
	{
		log_msg(e.what());
	}

}

void search_artist_process_callback::on_abort(HWND p_wnd) {
	//..
}

void search_artist_process_callback::on_error(HWND p_wnd) {
	//..
}

void expand_master_release_process_callback::start(HWND parent) {
	threaded_process::g_run_modeless(
		this,
		threaded_process::flag_show_item |
		//threaded_process::flag_show_abort |
		threaded_process::flag_show_delayed,
		parent,
		"Expanding master release..."
		);
}

void expand_master_release_process_callback::safe_run(threaded_process_status &p_status, abort_callback &p_abort) {
	p_status.set_item("expanding master release...");

	if (!m_master_release->loaded_releases && !m_master_release->sub_releases.get_count()) {
		m_master_release->load_releases(p_status, p_abort, false, m_offlineArtist_id, get_dbfetcher(), m_query_mode);
	}

	CFindReleaseDialog* find_dlg = g_discogs->find_release_dialog;

	find_dlg->on_expand_master_release_done(m_master_release, m_pos, p_status, p_abort);
}

void expand_master_release_process_callback::on_success(HWND p_wnd) {
	//todo: re-enable tree resfresh?
}

void expand_master_release_process_callback::on_abort(HWND p_wnd) {
	//..
}

void expand_master_release_process_callback::on_error(HWND p_wnd) {
	//..
}


void tree_apply_filter_process_callback::start(HWND parent) {
	threaded_process::g_run_modeless(
		this,
		threaded_process::flag_show_item |
		threaded_process::flag_show_abort |
		threaded_process::flag_show_delayed,
		parent,
		m_strFilter.get_length() ? "preparing tree view releases..." : "filtering tree view releases..."
	);
}

void tree_apply_filter_process_callback::safe_run(threaded_process_status& p_status, abort_callback& p_abort) {
	p_status.set_item(!m_strFilter.get_length() ? "show all" : PFC_string_formatter() << "filter: " << m_strFilter);

	CFindReleaseDialog* find_dlg = g_discogs->find_release_dialog;
	try {
		find_dlg->apply_filter(m_strFilter, m_force_redraw, m_force_rebuild, p_status, p_abort);
	}
	catch (foo_discogs_exception e) {
		//..
	}
}

void tree_apply_filter_process_callback::on_success(HWND p_wnd) {
	//todo
	CFindReleaseDialog* find_dlg = g_discogs->find_release_dialog;
	::ShowWindow(find_dlg->m_release_tree, SW_SHOW);
	::InvalidateRect(g_discogs->find_release_dialog->m_release_tree, NULL, TRUE);

}

void tree_apply_filter_process_callback::on_abort(HWND p_wnd) {

	CFindReleaseDialog* find_dlg = g_discogs->find_release_dialog;
	::ShowWindow(find_dlg->m_release_tree, SW_SHOW);
	::InvalidateRect(g_discogs->find_release_dialog->m_release_tree, NULL, TRUE);
}

void tree_apply_filter_process_callback::on_error(HWND p_wnd) {
	on_abort(p_wnd);
}



void on_search_artist_done_process_callback::start(HWND parent) {
	threaded_process::g_run_modeless(
		this,
		threaded_process::flag_show_item |
		threaded_process::flag_show_abort |
		threaded_process::flag_show_delayed,
		parent,
		"Search finished, preparing results..."
	);
}

void on_search_artist_done_process_callback::safe_run(threaded_process_status& p_status, abort_callback& p_abort) {
	p_status.set_item(PFC_string_formatter() << m_p_artist_exact_matches.get_count() << " exact, " << m_p_artist_other_matches.get_count() << "other matches");

	CFindReleaseDialog* find_dlg = g_discogs->find_release_dialog;
	try {
		find_dlg->on_search_artist_done(m_p_artist_exact_matches, m_p_artist_other_matches, m_append, m_out_va_cap);
	}
	catch (foo_discogs_exception e) {
		//..
	}
}

void on_search_artist_done_process_callback::on_success(HWND p_wnd) {

	CFindReleaseDialog* find_dlg = g_discogs->find_release_dialog;

	find_dlg->ilo_get_uilist()->Invalidate(true);
	init_scroolbars(find_dlg->ilo_get_uilist()->m_hWnd);
}

void on_search_artist_done_process_callback::on_abort(HWND p_wnd) {
	//..
}

void on_search_artist_done_process_callback::on_error(HWND p_wnd) {
	//..
}

void on_get_artist_done_process_callback::start(HWND parent) {
	threaded_process::g_run_modeless(
		this,
		threaded_process::flag_show_item |
		threaded_process::flag_show_abort |
		threaded_process::flag_show_delayed,
		parent,
		"Artist ready, preparing results..."
	);
}

void on_get_artist_done_process_callback::safe_run(threaded_process_status& p_status, abort_callback& p_abort) {
	p_status.set_item(m_artist.get() ? m_artist->name.get_ptr() : "");

	CFindReleaseDialog* find_dlg = g_discogs->find_release_dialog;
	try {
		find_dlg->on_get_artist_done(m_updsrc, m_artist);
	}
	catch (foo_discogs_exception e) {
		//..
	}
}

void on_get_artist_done_process_callback::on_success(HWND p_wnd) {
	//..
}

void on_get_artist_done_process_callback::on_abort(HWND p_wnd) {
	//..
}

void on_get_artist_done_process_callback::on_error(HWND p_wnd) {
	//..
}

process_release_callback::process_release_callback(CFindReleaseDialog *dialog, const pfc::string8 &release_id,
	const pfc::string8& offline_artist_id, pfc::string8 inno, const metadb_handle_list &items) :
		m_dialog(dialog), m_release_id(release_id),
		m_offline_artist_id(offline_artist_id), m_inno(inno), m_items(items)
{
	m_finfo_manager = std::make_shared<file_info_manager>(items);
	m_finfo_manager->read_infos();
}

void process_release_callback::start(HWND parent) {
	threaded_process::g_run_modeless(this,
		threaded_process::flag_show_item |
		threaded_process::flag_show_delayed |
		threaded_process::flag_show_abort,
		parent,
		"Processing release...");
}

void process_release_callback::safe_run(threaded_process_status& p_status, abort_callback& p_abort) {

	pfc::string8 base_status = "fetching release information...";
	p_status.set_item(base_status);

	Release_ptr p_release;

	try {

		unsigned long lkey = encode_mr(0, m_release_id);
		p_release = discogs_interface->get_release(lkey, m_offline_artist_id, p_status, p_abort);
		if (p_release) {

			if (p_release->images.get_size() && (CONF.save_album_art || CONF.embed_album_art)) {
				if (!p_release->images[0]->url150.get_length()) {

					foo_discogs_exception ex;
					ex << "Image URLs unavailable - Is OAuth working?";
					add_error(ex, false);
				}

				bool has_thumb;
				try {
					has_thumb = discogs_interface->get_thumbnail_from_cache(p_release, false, 0, p_release->small_art,
						p_status, p_abort);
				}
				catch (foo_discogs_exception& e) {
					has_thumb = false;
					add_error("Reading release artwork preview small album art cache", e, false);
				}
				catch (...) {
					has_thumb = false;
					foo_discogs_exception e;
					e << "Unknown expection. ";
					add_error("Failed operation release reading artwork preview small album art cache", e, false);
				}

				if (!has_thumb) {
					p_status.set_item("fetching small album art...");
					try {
						discogs_interface->fetcher->fetch_url(p_release->images[0]->url150, "", p_release->small_art, p_abort, false);
					}
					catch (foo_discogs_exception& e) {
						add_error("Fetching small album art", e, false);
					}
					catch (...) {
						foo_discogs_exception e;
						e << "Unknown exception.";
						add_error("Failed operation Fetching small album art", e, false);
					}
				}
			}
		}
	}
	catch (foo_discogs_exception e) {
		throw e;
	}
	catch (...) {
		throw;
	}

	// hide find release dlg

	m_dialog->hide();

#ifdef SIM_VA_MA_BETA
	bool bva_as_ma = CONF.find_release_dlg_flags & CFindReleaseDialog::FLG_VA_AS_MA;

	if (bva_as_ma) {

		titleformat_hook_impl_multiformat hook(&p_release);
		hook.set_release(&p_release);

		pfc::string8 rel_artist_ids_tag;
		auto track = m_items.get_item(0);

		service_ptr_t<titleformat_object> tf_script;
		static_api_ptr_t<titleformat_compiler>()->compile_force(tf_script, "[%RELEASE_ARTISTS_ID%]");
		track->format_title(nullptr, rel_artist_ids_tag, tf_script, nullptr);

		std::unordered_set<std::string> unique_ids;

		if (!rel_artist_ids_tag.get_length()) {

			TagWriter tag_writer(m_finfo_manager, p_release);
			tag_mapping_list_type ml;
			tag_mapping_entry entry = { TAG_GUID_ARTIST_ID, "RELEASE ARTISTS ID", true, false, false, false, false, "%<ARTISTS_ID>%" }; ///*"%RELEASE_ARTISTS_ID%"*/ };
			ml.add_item(entry);
			generate_track_mapping(tag_writer, p_release.get());
			tag_writer.generate_tags(&ml, p_status, p_abort);
			tag_result_ptr tag_result = tag_writer.tag_results[0];

			string_encoded_array flat_result;
			flat_result.expand_depth(2); flat_result.encode();
			for (string_encoded_array trk_res : tag_result->value) {
				trk_res.limit_depth(1);

				if (flat_result.get_citem(0).print() == "") {
					flat_result.set_item(0, trk_res);
				}
				else {
					flat_result.shallow_append(trk_res);
				}
			}
			flat_result.flatten();

			for (size_t i = 0; i < flat_result.get_citems().size(); i++) {
				std::string buffer = flat_result.get_citem(i).print().c_str();
				if (buffer.size()) {
					unique_ids.insert(flat_result.get_citem(i).print().c_str());
				}
			}
		}
		else {
			std::vector<pfc::string8> vfake_artists;
			split(rel_artist_ids_tag, ";", 0, vfake_artists);
			for (pfc::string8 w : vfake_artists) {
				w.trim(' ');
				if (w.get_length()) {
					unique_ids.insert(w.c_str());
				}
			}
		}

		if (unique_ids.size()) {
			std::vector<std::string> vtrue_artists;
			for (ReleaseArtist_ptr w : p_release->artists) {
				vtrue_artists.emplace_back(w->id);
			}
			for (std::string stdfw : unique_ids) {
				pfc::string8 fw(stdfw.c_str());
				fw = fw.trim(' ');
				if (!fw.get_length()) {
					continue;
				}
				auto find_it = std::find(vtrue_artists.begin(), vtrue_artists.end(), fw.c_str());
				if (find_it == vtrue_artists.end()) {

					Artist_ptr tmpArtist_ptr = std::make_shared<Artist>(fw);

					bool done = false;
					Artist_ptr tmp_artist_ptr;
					for (ReleaseDisc_ptr wdisc : p_release->discs) {
						if (done) break;
						for (ReleaseTrack_ptr wtrack : wdisc->tracks) {
							if (done) break;
							for (ReleaseArtist_ptr wartist : wtrack->artists) {
								if (done) break;
								if (wartist->id.equals(fw) && !wartist->roles.get_count()) {

									done = true;

									ReleaseArtist_ptr tmpArtistRelease_ptr = std::make_shared<ReleaseArtist>(fw.c_str(), tmp_artist_ptr, true);
									Artist_ptr full_artist = discogs_interface->get_artist(fw, false, p_status, p_abort, false, true);
									tmpArtistRelease_ptr->load(p_status, p_abort, false);
									//fullartist
									tmpArtistRelease_ptr->id = full_artist->id;
									tmpArtistRelease_ptr->name = full_artist->name;
									tmpArtistRelease_ptr->roles.append_single(string_encoded_array("beta credit artwork"));
									p_release->artists.append_single(tmpArtistRelease_ptr);
								}
							}
						}
					}
				}
			}
		}
	}
#endif //SIM_VA_MA_BETA

	m_tag_writer = std::make_shared<TagWriter>(m_finfo_manager, p_release);
	if (p_release) {
		m_tag_writer->match_tracks();
	}
}

void process_release_callback::generate_track_mapping(TagWriter& tagwriter, const Release *p_release) {

		tagwriter.m_track_mappings.force_reset();

		for (size_t i = 0; i < p_release->discs.size(); i++) {

			track_mapping tm_track;
			tm_track.discogs_disc = i;

			for (size_t j = 0; j < p_release->discs[i]->tracks.get_count(); j++) {
				tm_track.discogs_track = j;
				tm_track.file_index = j;
				tm_track.enabled = true;
				tagwriter.m_track_mappings.append_single(tm_track);
			}
		}
}

void process_release_callback::on_success(HWND p_wnd) {
	auto tw_release = m_tag_writer->GetRelease();
	if (tw_release) {
		rppair row = std::pair(std::pair(tw_release->id, tw_release->title),
			std::pair("", ""));

		//release history
		m_dialog->add_history(oplog_type::release, kHistoryProccessRelease, row);
	}

	{
		//todo: trace race condition
		std::lock_guard<std::mutex> ul(g_discogs->create_match_tracking_mutex);

		if (!g_discogs->track_matching_dialog) {
			fb2k::newDialog<CTrackMatchingDialog>(core_api::get_main_window(), m_tag_writer, false);
		}
	}
}

void process_release_callback::on_abort(HWND p_wnd) {
	m_dialog->show();
}

void process_release_callback::on_error(HWND p_wnd) {
	m_dialog->show();
}

process_artwork_preview_callback::process_artwork_preview_callback(CTrackMatchingDialog* dialog, const Release_ptr& release, const size_t img_ndx, const bool bartist, bool onlycache, bool get_mibs) :
	m_dialog(dialog), m_release(release), m_img_ndx(img_ndx), m_bartist(bartist), m_onlycache(onlycache), m_get_mibs(get_mibs) {
	m_musicbrainz_mibs = {};
}

void process_artwork_preview_callback::start(HWND parent) {
	threaded_process::g_run_modeless(this,
		threaded_process::flag_show_item |
		threaded_process::flag_show_delayed |
		threaded_process::flag_show_abort,
		parent,
		"Processing track matching artwork preview..."
	);
}

void process_artwork_preview_callback::safe_run(threaded_process_status& p_status, abort_callback& p_abort) {
	p_status.set_item("Fetching artwork preview information...");
	
	if (m_release) {
		if (m_get_mibs) {
			do {
				try {
					p_status.set_item("Fetching MusicBrainz ids...");
					pfc::string8 html;
					pfc::string8 mib_request_url;
					//RELEASE MIB
					mib_request_url = "https://www.musicbrainz.org/ws/2/url?inc=release-rels&fmt=json&resource=https://www.discogs.com/release/";
					mib_request_url << m_release->id;
					discogs_interface->fetcher->fetch_html_simple_log(mib_request_url, "", html, p_abort);
					
					try {
						JSONParser jp(html);
						if (json_is_object(jp.root)) {
							json_t* s = json_object_get(jp.root, "relations");
							json_t* ar_s = json_array_get(s, 0);
							json_t* t = json_object_get(ar_s, "release");
							m_musicbrainz_mibs.release = JSONAttributeString(t, "id");
						}
					}
					catch (...) {
						log_msg("Error parsing release MBid");
					}

					if (m_release->master_id.get_length()) {
						//release group mib
						mib_request_url = "https://www.musicbrainz.org/ws/2/url?inc=release-group-rels&fmt=json&resource=https://www.discogs.com/master/";
						mib_request_url << m_release->master_id;
						discogs_interface->fetcher->fetch_html_simple_log(mib_request_url, "", html, p_abort);

						try {
							JSONParser jp(html);
							if (json_is_object(jp.root)) {
								json_t* s = json_object_get(jp.root, "relations");
								json_t* ar_s = json_array_get(s, 0);
								json_t* t = json_object_get(ar_s, "release_group");
								m_musicbrainz_mibs.release_group = JSONAttributeString(t, "id");
							}
						}
						catch (...) {
							log_msg("Error parsing release_group MBid");
						}
					}
					if (m_release->artists[0]->full_artist->id.get_length()) {
						//artist mib
						mib_request_url = "https://www.musicbrainz.org/ws/2/url?inc=artist-rels&fmt=json&resource=https://www.discogs.com/artist/";
						mib_request_url << m_release->artists[0]->full_artist->id;
						discogs_interface->fetcher->fetch_html_simple_log(mib_request_url, "", html, p_abort);

						try {
							JSONParser jp(html);
							if (json_is_object(jp.root)) {
								json_t* s = json_object_get(jp.root, "relations");
								json_t* ar_s = json_array_get(s, 0);
								json_t* t = json_object_get(ar_s, "artist");
								m_musicbrainz_mibs.artist = JSONAttributeString(t, "id");
							}
						}
						catch (...) {
							log_msg("Error parsing artist MBid");
						}
					}

					if (m_musicbrainz_mibs.release.get_length()) {
						//check cover artwork
						mib_request_url = "https://musicbrainz.org/release/";
						mib_request_url << m_musicbrainz_mibs.release << "/cover-art";
						discogs_interface->fetcher->fetch_html_simple_log(mib_request_url, "", html, p_abort);
						m_musicbrainz_mibs.coverart = html.get_length() && html.find_first("Cover Art (0)", 0) == pfc_infinite;
					}
				}
				catch (network_exception) {
					break;
				}
				catch (exception_aborted) {
					break;
				}
				catch (...) {
					log_msg("Unhandled error parsing MBid");
					throw;
				}

			} while (false);
		}

		pfc::string8 id;
		pfc::string8 url;
		bool has_thumb;

		try {
			has_thumb = discogs_interface->get_thumbnail_from_cache(m_release, m_bartist, m_img_ndx, m_small_art,
				p_status, p_abort);
		}
		catch (foo_discogs_exception& e) {
			has_thumb = false;
			add_error("Reading artwork preview small album art cache", e, false);
		}
		catch (...) {
			has_thumb = false;
			foo_discogs_exception e;
			e << "Unknown exception";
			add_error("Failed operation reading artwork preview small album art cache", e, false);
		}

		if (!has_thumb) {

			if (!m_onlycache) {
				p_status.set_item("Fetching artwork preview small album art...");
				try {
					pfc::array_t<Image_ptr> images;

					if (m_bartist) {
						Artist_ptr artist;
						size_t artist_img_ndx;
						if (discogs_interface->img_artists_ndx_to_artist(m_release, m_img_ndx, artist, artist_img_ndx)) {
							images = artist->images;
							if (images.get_count()) {
								discogs_interface->fetcher->fetch_url(images[artist_img_ndx]->url150, "", m_small_art, p_abort, false);
							}
						}
					}
					else {
						images = m_release->images;
						if (images.get_count()) {
							discogs_interface->fetcher->fetch_url(images[m_img_ndx]->url150, "", m_small_art, p_abort, false);
						}
					}
				}
				catch (foo_discogs_exception& e) {
					add_error("Fetching artwork preview small album art", e, false);
				}
				catch (...) {
					foo_discogs_exception e;
					e << "Unknown exception.";
					add_error("Failed operation Fetching artwork preview small album art", e, false);
				}
			}
		}
	}
}

void process_artwork_preview_callback::on_success(HWND p_wnd) {
	m_dialog->process_artwork_preview_done(m_img_ndx, m_bartist, m_small_art, m_musicbrainz_mibs);
}

void process_artwork_preview_callback::on_abort(HWND p_wnd) {
	m_dialog->pending_previews_done(1);
}

void process_artwork_preview_callback::on_error(HWND p_wnd) {
	m_dialog->pending_previews_done(1);
}


process_file_artwork_preview_callback::process_file_artwork_preview_callback(CTrackMatchingDialog* dialog, const Release_ptr& release, metadb_handle_list items, const size_t img_ndx, const bool bartist) :
	m_dialog(dialog), m_release(release), m_img_ndx(img_ndx), m_items(items), m_bartist(bartist) {
}

void process_file_artwork_preview_callback::start(HWND parent) {
	threaded_process::g_run_modeless(this,
		threaded_process::flag_show_item |
		threaded_process::flag_show_abort,
		parent,
		"Processing local files track matching artwork preview..."
	);
}

void process_file_artwork_preview_callback::safe_run(threaded_process_status& p_status, abort_callback& p_abort) {
	p_status.set_item("Fetching local files artwork preview information...");

	if (m_release) {
		p_status.set_item("Fetching local files artwork preview small album art...");
		try {
			bool bexists = true;
			pfc::string8 directory;
			file_info_impl info;
			titleformat_hook_impl_multiformat hook(p_status, &m_release);
			CONF.album_art_directory_string->run_hook(m_items[0]->get_location(), &info, &hook, directory, nullptr);
			// hardcode "nullptr" as don't write anything.  ???
			if (STR_EQUAL(directory, "nullptr")) {
				bexists = false;
			}

			pfc::string8 file_name = m_img_ndx < album_art_ids::num_types() ? album_art_ids::query_name(m_img_ndx) : "";
			pfc::chain_list_v2_t<pfc::string8> splitfile;
			pfc::splitStringByChar(splitfile, file_name.get_ptr(), ' ');
			if (splitfile.get_count()) file_name = *splitfile.first();
			file_name << ".jpg";

			pfc::string8 full_path(directory);
			full_path << "\\" << file_name;

#pragma warning(suppress:4996)
			std::filesystem::path p= std::filesystem::u8path(full_path.get_ptr());
			int filesize = -1;
			if (std::filesystem::exists(p)) {
				filesize = std::filesystem::file_size(p);
			}
			else {
				return;
			}

			pfc::stringcvt::string_os_from_utf8 cvt_bitmap(full_path);
			Gdiplus::Bitmap local_bitmap(cvt_bitmap.get_ptr(), false);
			m_small_art = GenerateTmpBitmapsFromRealSize(m_release->id, m_img_ndx, full_path, m_temp_file_names);
			return;
		}
		catch (foo_discogs_exception& e) {
			add_error("Fetching local files artwork preview small album art", e, false);
		}
		catch (...) {
			foo_discogs_exception e;
			e << "Unknown exception.";
			add_error("Operation failed Fetching local files artwork preview small album art", e, false);
		}
	}
}

void process_file_artwork_preview_callback::on_success(HWND p_wnd) {
	//not checking dlg (m_hWnd is the process HWND parent)
	m_dialog->process_file_artwork_preview_done(m_img_ndx, m_bartist, m_small_art, m_temp_file_names);
	BOOL bres = DeleteObject(m_small_art.first.first);
	bres = DeleteObject(m_small_art.first.second);
	bres = DeleteObject(m_small_art.second.first);
	bres = DeleteObject(m_small_art.second.second);
}

void process_file_artwork_preview_callback::on_abort(HWND p_wnd) {
	m_dialog->pending_previews_done(1);
}

void process_file_artwork_preview_callback::on_error(HWND p_wnd) {
	m_dialog->pending_previews_done(1);
}


void test_oauth_process_callback::start(HWND parent) {
	threaded_process::g_run_modal(this,
		threaded_process::flag_show_item |
		threaded_process::flag_show_abort,
		parent,
		"Testing Discogs OAuth support..."
	);
}

void test_oauth_process_callback::safe_run(threaded_process_status &p_status, abort_callback &p_abort) {
	if (test_token.get_length() && test_token_secret.get_length()) {
		p_status.set_item("Testing OAuth identity...");
		discogs_interface->fetcher->test_oauth(test_token, test_token_secret, p_abort);
	}
	else {
		add_error("Error: You must generate OAuth tokens first.");
	}
}

void test_oauth_process_callback::on_success(HWND p_wnd) {
	//not checking dlg (m_hWnd is the process HWND parent)
	CConfigurationDialog* dlg = g_discogs->configuration_dialog;
	dlg->show_oauth_msg("OAuth is working!", false);
	::SetFocus(g_discogs->configuration_dialog->m_hWnd);
}

void test_oauth_process_callback::on_error(HWND p_wnd) {
	CConfigurationDialog* dlg = g_discogs->configuration_dialog;
	dlg->show_oauth_msg("OAuth failed.", true);
	::SetFocus(g_discogs->configuration_dialog->m_hWnd);
}


void authorize_oauth_process_callback::start(HWND parent) {
	threaded_process::g_run_modal(this,
		threaded_process::flag_show_item |
		threaded_process::flag_show_abort,
		parent,
		"Requesting Discogs OAuth Authorization Code..."
	);
}

void authorize_oauth_process_callback::safe_run(threaded_process_status &p_status, abort_callback &p_abort) {
	p_status.set_item("Authorizing OAuth...");
	pfc::string8 auth_url = discogs_interface->fetcher->get_oauth_authorize_url(p_abort);
	display_url(auth_url);
}


void generate_oauth_process_callback::start(HWND parent) {
	threaded_process::g_run_modal(this,
		threaded_process::flag_show_item |
		threaded_process::flag_show_abort,
		parent,
		"Generating Discogs OAuth tokens..."
	);
}

void generate_oauth_process_callback::safe_run(threaded_process_status &p_status, abort_callback &p_abort) {
	p_status.set_item("Generating OAuth tokens...");
	token = discogs_interface->fetcher->generate_oauth(code, p_abort);
	if (token == nullptr) {
		add_error("Error: Invalid PIN Code.");
	}
}

void generate_oauth_process_callback::on_success(HWND p_wnd) {

	CConfigurationDialog* dlg = g_discogs->configuration_dialog;
	if (token) {
		uSetWindowText(dlg->m_hwndTokenEdit, token->key().c_str());
		uSetWindowText(dlg->m_hwndSecretEdit, token->secret().c_str());
		delete token;
		token = nullptr;
	}

}
