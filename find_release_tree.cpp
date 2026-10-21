#include "stdafx.h"

#include "libPPUI\clipboard.h"

#include "discogs_interface.h"
#include "foo_discogs.h"
#include "multiformat.h"

#include "find_release_utils.h"
#include "find_release_dialog.h"
#include "find_release_tree.h"
#include "discogger_tree_snapshot.h"
const size_t WINE_CHILDREN = 99999;

const size_t TRACER_IMG_NDX = 3;
const size_t CHILDREN_DONE_IMG_NDX = 6;
const size_t TRACER_CHILDREN_DONE_IMG_NDX = 7;

//
namespace fs = std::filesystem;

pfc::string8 CFindReleaseTree::run_hook_columns(row_col_data& row_data, int item_data) {

	mounted_param myparam((LPARAM)item_data);
	pfc::string8 search_formatted;
	pfc::string8 id;

	row_data.col_data_list.clear();
    row_data.id = -1;

    const auto unavailable = [&]() -> pfc::string8 {
        pfc::string8 label("[Release unavailable]");
        row_data.col_data_list.emplace_back(0, label);
        return label;
    };
    if (!m_find_release_artist) return unavailable();

    const auto kind = myparam.is_master() ? discogger_tree::row_kind::master :
        myparam.is_release() ? discogger_tree::row_kind::subrelease :
        myparam.is_nmrelease() ? discogger_tree::row_kind::standalone :
                                discogger_tree::row_kind::invalid;
    if (kind == discogger_tree::row_kind::invalid) return unavailable();

    const bool needs_master = kind == discogger_tree::row_kind::master ||
                              kind == discogger_tree::row_kind::subrelease;
    MasterRelease_ptr master;
    if (needs_master) {
        if (myparam.master_ndx >= m_find_release_artist->master_releases.get_count())
            return unavailable();
        master = m_find_release_artist->master_releases[myparam.master_ndx];
        if (!master) return unavailable();
    }
    const size_t child_count = master ? master->sub_releases.get_count() : 0;
    if (!discogger_tree::valid_row_indices(kind, myparam.master_ndx,
            myparam.release_ndx, m_find_release_artist->master_releases.get_count(),
            child_count, m_find_release_artist->releases.get_count()))
        return unavailable();
    if (kind == discogger_tree::row_kind::subrelease &&
        !master->sub_releases[myparam.release_ndx]) return unavailable();
    if (kind == discogger_tree::row_kind::standalone &&
        !m_find_release_artist->releases[myparam.release_ndx]) return unavailable();

	if (myparam.is_master()) {
		CONF.search_master_format_string->run_hook(m_location, m_info_p, m_hook.get(), search_formatted, nullptr);

		id << m_find_release_artist->master_releases[myparam.master_ndx]->id;
	}
	else {

		if (myparam.is_release()) {
			CONF.search_master_sub_format_string->run_hook(m_location, m_info_p, m_hook.get(), search_formatted, nullptr);
			pfc::string8 this_release_id = m_find_release_artist->master_releases[myparam.master_ndx]->sub_releases[myparam.release_ndx]->id;
			id << this_release_id;
		}
		else
		{
			CONF.search_release_format_string->run_hook(m_location, m_info_p, m_hook.get(), search_formatted, nullptr);
			id << m_find_release_artist->releases[myparam.release_ndx]->id;
		}
	}
	row_data.col_data_list.emplace_back(std::make_pair(0/*walk_cfg.icol*/, search_formatted));
	row_data.id = atoi(id);

	return search_formatted;
}

void CFindReleaseTree::rebuild_treeview() {
	//todo: rev setredraw
	//fix crash - dbl click artists (1)
	bool bak_disp_info = m_dispinfo_enabled;
	if (!m_dlg->config().auto_load_releases_on_select_ready()) {
		m_dispinfo_enabled = false;
	}

	SendMessage(m_hwndTreeView, WM_SETREDRAW, 0, 0);
	TreeView_DeleteAllItems(m_hwndTreeView);
	SendMessage(m_hwndTreeView, WM_SETREDRAW, 1, 0);

	std::shared_ptr<vec_t>& priv_vec = m_rt_cache.get_vec_ref();

	SendMessage(m_hwndTreeView, WM_SETREDRAW, 0, 0);

	for (auto& walk : *priv_vec)
	{
		TVINSERTSTRUCT tvis = { 0 };

		tvis.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_CHILDREN | TVIF_PARAM | TVIF_STATE;
		tvis.item.pszText = LPSTR_TEXTCALLBACK;
		tvis.item.iSelectedImage = I_IMAGECALLBACK;

		tvis.item.cChildren = I_CHILDRENCALLBACK;
		tvis.item.lParam = walk.first->first;
		tvis.hParent = TVI_ROOT;
		tvis.hInsertAfter = TVI_LAST;
		mounted_param myparam(walk.first->first);
		bool bbold = (walk.first->second.first.id == m_idtracer_p->master_id) && myparam.is_master();
		bbold |= (walk.first->second.first.id == m_idtracer_p->release_id) && myparam.is_nmrelease();

		int img_ndx;
		on_tree_display_cell_image(myparam.master_ndx, myparam.release_ndx, walk.first->second.first.id, walk.first, img_ndx);

		if (bbold) {
			tvis.item.iImage = TRACER_IMG_NDX;
			tvis.item.iSelectedImage = TRACER_IMG_NDX;
			tvis.item.state = TVIS_BOLD;
			tvis.item.stateMask = TVIS_BOLD;
		}
		else {
			tvis.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_CHILDREN | TVIF_PARAM | TVIF_STATE;
			tvis.item.iImage = -1;
			tvis.item.iSelectedImage = -1;
		}

		HTREEITEM newnode = TreeView_InsertItem(m_hwndTreeView, &tvis);
		walk.second = newnode;
	}

	SendMessage(m_hwndTreeView, WM_SETREDRAW, 1, 0);
	//fix crash - dbl click artists (2)
	if (!m_dlg->config().auto_load_releases_on_select_ready()) {
		m_dispinfo_enabled = bak_disp_info;
	}
}

void release_tree_cache::vec_PushBack(std::pair<cache_iterator_t, HTREEITEM> item) {
	m_vec_ptr->push_back(item);
}
bool release_tree_cache::get_cached_find_release_node(int lparam, pfc::string8& item, row_col_data& row_data) {

	cache_t* cached = &m_cache_ptr->cache;
	auto it = cached->find(lparam);

	if (it != cached->end()) {
		item = it->second.first.col_data_list.begin()->second;
		row_data = it->second.first;
		return true;
	}
	return false;
}

bool check_match(pfc::string8 str, pfc::string8 filter, pfc::array_t<pfc::string> filter_words_lowercase) {
	bool bres = true;
	if (filter.get_length()) {
		pfc::string item_lower = pfc::string(str.get_ptr()).toLower();
		for (size_t j = 0; j < filter_words_lowercase.get_count(); j++) {
			bres = item_lower.contains(filter_words_lowercase[j]);
			if (!bres) {
				break;
			}
		}
	}
	return bres;
}

//

// release_tree_cache :: update releases

//

void fake_hook_undefined_release(Release_ptr r, pfc::string8 &out) {

	out << r->title;
	out << ", " << pfc::format_array(r->search_major_formats);
	out << ", " << r->search_formats;
	out << ", " << r->search_labels;

	if (r->search_catno.get_length())
		out << ", " << r->search_catno;
	if (r->barcode.get_length())
		out << ", " << r->barcode;
	if (r->country.get_length())
		out << ", " << r->country;
	if (r->release_year.get_length())
		out << ", " << r->release_year;

}

void fake_hook_undefined_master(MasterRelease_ptr mr, pfc::string8& out) {

	out << mr->title;

	if (mr->release_year.get_length())
		out << ", " << mr->release_year;

}

std::pair<rppair_t, rppair_t> release_tree_cache::update_releases(const pfc::string8& filter, updRelSrc updsrc, bool init_expand, id_tracer* tracer_p, bool brolemain_filter) {

	std::pair<size_t, size_t> vec_lv_reserve_slow;
	std::pair<size_t, size_t> vec_lv_reserve_fast;
	std::pair<size_t, size_t> master_rel_stat_slow;
	std::pair<size_t, size_t> master_rel_stat_fast;

	titleformat_hook_impl_multiformat_ptr hook = m_rt_manager->get_hook();

	bool delete_on_enter = true;

	bool rebuild_tree_cache = true;

	if (updsrc == updRelSrc::Filter || delete_on_enter) {
		tracer_p->release_reset();
	}

	pfc::array_t<pfc::string> lcf_words;
	bool filtered = filter.get_length() && tokenize_filter(filter, lcf_words);
	Artist_ptr find_release_artist = m_rt_manager->get_find_release_artist();

	if (delete_on_enter) {

		//CALC CACHE - FILTERED
		rppair_t r = init_filter(find_release_artist, filter, brolemain_filter, true, false, true,tracer_p);
		vec_lv_reserve_slow = r.first;
		master_rel_stat_slow = r.second;
		//CLEAR AND RESERVE NEW REF CACHE
		m_vec_ptr->clear();
		m_vec_ptr->reserve(vec_lv_reserve_slow.first + vec_lv_reserve_slow.second);

		if (updsrc != updRelSrc::Filter) {
			//DESTROY BULK CACHE
			if (m_cache_ptr != nullptr)
				m_cache_ptr->cache.clear();

			rebuild_tree_cache = true;
		}
		else {
			rebuild_tree_cache = false;
		}
	}

	//exit on empty source
	if (!find_release_artist) {
		return std::pair(rppair_t(), rppair_t());
	}

	t_size list_index = 0;
	t_size master_index = 0, release_index = 0;

	int item_data;
	std::pair<cache_iterator_t, bool> last_cache_emplaced;
	row_col_data row_data{};

	rppair_t  reserve;
	if (rebuild_tree_cache) {

		//CALC CACHE - NOT FILTERED
		reserve = init_filter(find_release_artist, "", brolemain_filter, true, true, true, tracer_p);
		vec_lv_reserve_fast = reserve.first;
		master_rel_stat_fast = reserve.second;
		m_cache_ptr->cache.reserve(find_release_artist->search_order_master.get_count());
	}

	std::string str_role;

	try {

		Artist_ptr find_release_artist = m_rt_manager->get_find_release_artist();

		for (t_size walk = 0; walk < find_release_artist->search_order_master.get_size(); walk++) {

			bool is_master = find_release_artist->search_order_master[walk];
			pfc::string8 item; 

			if (is_master) {

				//master

				str_role = std::string(find_release_artist->master_releases[master_index]->search_role);
				mounted_param thisparam(master_index, ~0, true, false);
				item_data = thisparam.lparam();

				if (updsrc != updRelSrc::Filter)
				{

					if (search_query::is_undefined_artist(find_release_artist->id, true)) {
						mounted_param myparam((LPARAM)item_data);

						pfc::string8 search_formatted;
						fake_hook_undefined_master(find_release_artist->master_releases[master_index], search_formatted);

						pfc::string8 id = find_release_artist->master_releases[master_index]->id;

						row_data.col_data_list.clear();
						row_data.col_data_list.emplace_back(std::make_pair(0, search_formatted));
						row_data.id = atoi(id);

						item = search_formatted;
					}
					else {
						hook->set_master(&(find_release_artist->master_releases[master_index]));
						item = m_rt_manager->run_hook_columns(row_data, item_data);
					}
				}
				else
					get_cached_find_release_node(item_data, item, row_data);
			}
			else {

				//no master release

				str_role = std::string(find_release_artist->releases[release_index]->search_role);
				mounted_param thisparam(~0, release_index, false, true);
				item_data = thisparam.lparam();
				if (updsrc != updRelSrc::Filter)
				{

					if (search_query::is_undefined_artist(find_release_artist->id, false)) {


						mounted_param myparam((LPARAM)item_data);
						pfc::string8 search_formatted;

						fake_hook_undefined_release(find_release_artist->releases[release_index], search_formatted);

						pfc::string8 id = find_release_artist->releases[release_index]->id;

						row_data.col_data_list.clear();
						row_data.col_data_list.emplace_back(std::make_pair(0/*walk_cfg.icol*/, search_formatted));
						row_data.id = atoi(id);

						item = search_formatted;

					}
					else {

						hook->set_release(&(find_release_artist->releases[release_index]));
						item = m_rt_manager->run_hook_columns(row_data, item_data);
					}

				}
				else
					get_cached_find_release_node(item_data, item, row_data);
			}

			if (rebuild_tree_cache) {

				// *** BULK CACHE EMPLACE (master or no master release) ***

				last_cache_emplaced = m_cache_ptr->cache.emplace(
					item_data, std::pair<row_col_data, flagreg>	{row_data, { m_cache_ptr->_ver, 0 }});
			}

			bool inserted = false; bool expanded = false;

			//>> check
			bool matches = !filtered || check_match(item, filter, lcf_words);
			bool matches_role = !brolemain_filter || (str_role.find("Main") != str_role.npos);
			matches &= matches_role;
			//<< check

			int prev_expanded = 0;
			if (matches) {

				if (is_master)
					tracer_p->release_check(find_release_artist->master_releases[master_index]->id, list_index,
						is_master, list_index, master_index);
				else
					tracer_p->release_check(find_release_artist->releases[release_index]->id, list_index,
						mounted_param(~0, release_index, false, true), list_index, master_index);

				// MASTER OR NO MASTER RELEASES

				if (delete_on_enter) {
					mounted_param myparam((LPARAM)item_data);

					if (myparam.is_master()) {

						cache_iterator_t insert_cache_it = m_cache_ptr->cache.find(myparam.lparam());
						insert_cache_it->second.second.flag = 0;
						insert_cache_it->second.second.ver = m_cache_ptr->_ver;
						int ival = 1;
						m_cache_ptr->SetCacheFlag(insert_cache_it, NodeFlag::none, &ival);
						std::pair<cache_iterator_t, HTREEITEM> vec_row(insert_cache_it, nullptr);

						// *** VEC PUSH (MASTER) ***

						m_vec_ptr->push_back(std::pair<cache_iterator_t, HTREEITEM>(insert_cache_it, (HTREEITEM)nullptr));
					}
					else {

						if (myparam.is_release()) {

							//  no push here for releases (see below)

						}
						else {

							tracer_p->release_check(find_release_artist->releases[release_index]->id, list_index,
								myparam, ~0, ~0);

							cache_iterator_t insert_cache_it = m_cache_ptr->cache.find(myparam.lparam());
							insert_cache_it->second.second.flag = 0;
							insert_cache_it->second.second.ver = m_cache_ptr->_ver;
							int ival = 0;
							m_cache_ptr->SetCacheFlag(insert_cache_it, NodeFlag::added, &ival);
							flagreg& insfr = insert_cache_it->second.second;
							std::pair<cache_iterator_t, HTREEITEM> vecrow(insert_cache_it, (HTREEITEM)nullptr);

							// *** VEC PUSH (non master release) ***

							m_vec_ptr->push_back(std::pair<cache_iterator_t, HTREEITEM>(vecrow));
						}
					}
				}
				list_index++;
				inserted = true;
			}

			if (is_master) {

				// SUB-RELEASES LOOP

				const MasterRelease_ptr* master_release = &(find_release_artist->master_releases[master_index]);

				for (size_t j = 0; j < master_release->get()->sub_releases.get_size(); j++) {

					mounted_param thisparam = { master_index, j, true, true };

					item_data = thisparam.lparam();

					pfc::string8 sub_item;
					if (updsrc != updRelSrc::Filter)
					{

						if (search_query::is_undefined_artist(find_release_artist->id, true)) {

							mounted_param myparam((LPARAM)item_data);

							pfc::string8 search_formatted;

							fake_hook_undefined_release(find_release_artist->master_releases[master_index]->sub_releases[j], search_formatted);

							pfc::string8 id = find_release_artist->master_releases[master_index]->sub_releases[j]->id;

							row_data.col_data_list.clear();
							row_data.col_data_list.emplace_back(std::make_pair(0/*walk_cfg.icol*/, search_formatted));
							row_data.id = atoi(id);

							item = search_formatted;

						}
						else {

							//set hook release
							hook->set_release(&(find_release_artist->master_releases[master_index]->sub_releases[j]));
							sub_item = m_rt_manager->run_hook_columns(row_data, item_data);
						}
					}
					else
						get_cached_find_release_node(item_data, sub_item, row_data);

					std::pair<cache_iterator_t, bool> last_cache_subitem_emplaced =
					{ m_cache_ptr->cache.end(), false };

					if (rebuild_tree_cache) {

						// *** BULK CACHE EMPLACE (sub-release) ***

						last_cache_subitem_emplaced =
							m_cache_ptr->cache.emplace(
								item_data, std::pair<row_col_data, flagreg> {row_data, { m_cache_ptr->_ver, 0 }}
						);
					}
					bool matches = !filtered || check_match(sub_item, filter, lcf_words);
					Release_ptr release = find_release_artist->master_releases[master_index]->sub_releases[j];
					str_role = std::string(release->search_role);
					bool release_roles_todo = !str_role.size();
					matches_role = release_roles_todo || !brolemain_filter || (str_role.find("Main") != str_role.npos);
					matches &= matches_role;

					if (matches) {

						int master_id = atoi(find_release_artist->master_releases[master_index]->id);

						vec_iterator_t parent;
						find_vec_by_id(*m_vec_ptr, master_id, parent);
						inserted = parent != m_vec_ptr->end();

						bool master_match_role = inserted;

						if (!inserted) {

							// ADD NOT INSERTED PARENT
							if (delete_on_enter) {

								std::string master_str_rol = find_release_artist->master_releases[master_index]->search_role.get_ptr();
								master_match_role = !brolemain_filter || (master_str_rol.find("Main") != master_str_rol.npos);
								if (master_match_role) {

									mounted_param not_matching_master_param(master_index, ~0, true, false);
									cache_iterator_t insert_cache_it = m_cache_ptr->cache.find(not_matching_master_param.lparam());
									insert_cache_it->second.second.flag = 0;
									insert_cache_it->second.second.ver = m_cache_ptr->_ver;
									int ival = 1;
									m_cache_ptr->SetCacheFlag(insert_cache_it, NodeFlag::none, &ival);

									// *** VEC PUSH (not matching master) ***

									std::pair<cache_iterator_t, HTREEITEM> vec_row(insert_cache_it, nullptr);
									m_vec_ptr->push_back(vec_row);

								}

							}
						}

						// SUBRELEASE

						if (master_match_role && init_expand && prev_expanded == 1) {

							tracer_p->release_check(master_release->get()->sub_releases[j]->id, j, (mounted_param((LPARAM)item_data)), ~0, master_index);

							// insert

							if (delete_on_enter) {
								mounted_param myparam((LPARAM)item_data);
								if (myparam.is_master()) {
									cache_iterator_t insit = last_cache_emplaced.first;
									flagreg& insfr = last_cache_emplaced.first->second.second;

									// *** VEC PUSH (release) ***

									m_vec_ptr->push_back(std::pair<cache_iterator_t, HTREEITEM>
										(last_cache_emplaced.first, nullptr));
								}
								else {
									if (myparam.is_release()) {
										;
									}
									else {
										cache_iterator_t insit = last_cache_emplaced.first;
										flagreg& insfr = last_cache_emplaced.first->second.second;
									}
								}
							}
							list_index++;
						}
					}
				}
			}

			if (is_master) {
				master_index++;
			}
			else {
				release_index++;
			}
		}
	}
	catch (foo_discogs_exception& e) {

		throw e;
	}

	int list_count = m_vec_ptr->size();

	rppair_t full_master_rel_stat = { master_rel_stat_slow, master_rel_stat_fast };
	rppair_t full_lv_reserve = rppair_t(vec_lv_reserve_slow, vec_lv_reserve_fast);

	return std::pair(full_lv_reserve, full_master_rel_stat);
}

//

// CFindReleaseTree :: update_releases

// from get_artist_done() and apply_filter()

std::pair<rppair_t, rppair_t> CFindReleaseTree::update_releases(const pfc::string8& filter, updRelSrc updsrc, bool init_expand, bool brolemain_filter) {

	std::pair<rppair_t, rppair_t> res;

	try {

		//

		// forward UPDATE RELEASES

		//

		res = m_rt_cache.update_releases(filter, updsrc, init_expand, m_idtracer_p, brolemain_filter);

	}
	catch (foo_discogs_exception& e) {

		foo_discogs_exception ex;
		ex << "Error formating release list item [" << e.what() << "]";
		throw ex;
	}

	rebuild_treeview();

	return res;
}

void CFindReleaseTree::init_tracker_i(Artist_ptr artist, pfc::string8 filter_master, pfc::string8 filter_release, bool expanded, bool fast) {

	t_size ires = 0;
	bool filtered = filter_master.get_length() > 0;
	bool matches_master = true;
	bool matches_release = true;
	m_idtracer_p->master_i = pfc_infinite;
	m_idtracer_p->release_i = pfc_infinite;
	if (!artist) {
		return;
	}
	int pos = -1;
	if (m_idtracer_p->has_amr() || m_idtracer_p->release_tag) {
		int master_ndx = 0; int release_ndx = 0; bool master_done = false; bool release_done = false;
		for (size_t walk = 0; walk < artist->search_order_master.get_size(); walk++) {
			if (master_done && release_done && fast) break;
			bool bmaster = artist->search_order_master[walk];
			if (bmaster) {
				MasterRelease_ptr mr_p = artist->master_releases[master_ndx];
				if (filtered)
					matches_master = !stricmp_utf8(mr_p->title, filter_master);

				if (matches_master && atoi(artist->master_releases[master_ndx]->id) == m_idtracer_p->master_id) {
					pos++;
					m_idtracer_p->master_i = master_ndx;
					m_idtracer_p->master_pos = pos;

					master_done = true;
				}
				for (size_t j = 0; j < artist->master_releases[master_ndx]->sub_releases.size(); j++) {
					Release_ptr r_p = artist->master_releases[master_ndx]->sub_releases[j];
					if (filtered) {
						matches_release = !stricmp_utf8(r_p->title, filter_release);
					}

					if (matches_release && atoi(artist->master_releases[master_ndx]->sub_releases[j]->id) == m_idtracer_p->release_id) {
						bool bmaster_fault = m_idtracer_p->master_i != master_ndx;
						if (bmaster_fault) {
								pfc::string8 logmsg = "Error: Master/Release mismatch";
								log_msg(logmsg);
						}
						m_idtracer_p->release_i = mounted_param(master_ndx, j, true, true);
						m_idtracer_p->release_pos = pos + j;
						if (!m_idtracer_p->master_tag || bmaster_fault) {
							m_idtracer_p->master_i = master_ndx;
							m_idtracer_p->master_pos = pos;
							m_idtracer_p->master_tag = true;
						}
						release_done = true;
						break;
					}
				}
			}
			else {
				Release_ptr r_p = artist->releases[release_ndx];
				if (filtered)
					matches_release = !stricmp_utf8(r_p->title, filter_release);

				if (matches_release && atoi(artist->releases[release_ndx]->id) == m_idtracer_p->release_id) {
					pos++;
					m_idtracer_p->release_i = mounted_param(pfc_infinite, release_ndx, false, true);
					m_idtracer_p->release_pos = pos;
					m_idtracer_p->master_i = pfc_infinite;
					m_idtracer_p->master_pos = -1;

					release_done = true; master_done = true;
				}
			}
			if (bmaster)
				master_ndx++;
			else
				release_ndx++;
		}
	}
}

void CFindReleaseTree::init_titles(Artist_ptr artist, pfc::string8 & filter_hint) {

	if (!artist) {
		init_tracker_i(artist, "", "", false, true);
		m_init_master_title = "";
		m_init_release_title = "";
		return;
	}

	m_hook = std::make_shared<titleformat_hook_impl_multiformat>(&artist);

	pfc::string8 frm_album;
	pfc::string8 frm_artist_from_album;
	pfc::string8 frm_title_from_album;

	service_ptr_t<titleformat_object> m_album_name_script;
	static_api_ptr_t<titleformat_compiler>()->compile_force(m_album_name_script, "[%album%]");
	metadb_handle_ptr item = m_items[0];
	item->format_title(nullptr, frm_album, m_album_name_script, nullptr);	//ALBUM

	pfc::string8 filter;
	bool bart_title = false;
	std::pair < pfc::string8, pfc::string8> at_pair;
	if (bart_title = title_seams_artist_title(artist->name, frm_album, at_pair)) {
		frm_artist_from_album = at_pair.first;
		frm_title_from_album = at_pair.second;
		filter = frm_title_from_album;
	}
	else {
		filter = frm_album;
	}

	if (m_idtracer_p->has_amr() && find_release_artist_releases_available()) {

		init_tracker_i(artist, "", "", false, true);
		//m_inits empty on first call
		init_tracker_i(artist, m_init_master_title, m_init_release_title, false, true);

		pfc::string8 mtitle;
		pfc::string8 rtitle;

		if (m_idtracer_p->master_i != ~0) {
			MasterRelease_ptr master_p = artist->master_releases[m_idtracer_p->master_i];
			mtitle = master_p->title;
			if (m_idtracer_p->release_i.lparam() != -1) {
				size_t rel_ndx = m_idtracer_p->release_i.release_ndx;
				rtitle = master_p->sub_releases[rel_ndx]->title;
			}
			else {
				rtitle = "";
			}
		}
		else {
			if (m_idtracer_p->release_i.lparam() != -1) {
				size_t rel_ndx = m_idtracer_p->release_i.release_ndx;
				Release_ptr release_p = artist->releases[rel_ndx];
				rtitle = release_p->title;
			}
		}

		if (mtitle.get_length()) {
			if (stricmp_utf8(filter, mtitle))
			{
				pfc::string8 buffer(filter);

				if (buffer.has_prefix(mtitle))
				{
					filter.set_string(mtitle);
				}
				else {

					//until disabled filter in edit box amended
					m_filter_disabled_title_mismatch = true;
					//todo: rev title mismatch disabled filter != empty filter
					filter = " ";

					log_msg("Please, amend release title mismatch... tree filter is now DISABLED.");
				}
			}
		}

		m_init_master_title = mtitle;
		m_init_release_title = rtitle;
	}

	filter_hint = filter;
}

void CFindReleaseTree::on_get_artist_done(cupdRelSrc cupdsrc, const Artist_ptr artist) {

	if (CONF.awt_get_alt_mode()) {
		return;
	}

	size_t artist_id = artist ? atoi(artist->id) : ~0;
	size_t frartist_id = m_find_release_artist ? atoi(m_find_release_artist->id) : ~0;

	bool b_same_id = artist_id == frartist_id;
	bool b_same_artist_instance = m_find_release_artist.get() == artist.get();

	FieldValPair fvp_curr_qdm;
	FieldValPair fvp_new_qdm;

	bool b_same_qdm = false;
	bool b_qdm_skip = false;
	bool b_no_qdm_skip = false;

	search_query::MapToText(m_current_qdm,fvp_curr_qdm, false);
	search_query::MapToText(m_dlg->m_qdm_search_query,fvp_new_qdm, false);

	b_same_qdm = !fvp_curr_qdm.first.compare(fvp_new_qdm.first);
	b_qdm_skip = m_current_qdm.size() && Get_Size() && m_find_release_artist.get() && m_find_release_artist.get()->releases.get_count() && b_same_qdm;
	b_qdm_skip &= b_same_id;

	b_no_qdm_skip = Get_Size() && m_find_release_artist.get() && m_find_release_artist.get()->loaded_releases && b_same_id;

	bool bskip = m_dlg->m_qdm_search_query.size() ? b_qdm_skip : b_no_qdm_skip;
	bskip |= b_same_artist_instance;

	if (bskip) {
		//Still same 194 ?
		bool check_same_various = true;
		if (artist.get() && atoi(m_find_release_artist->id) == 194) {
			check_same_various = b_same_artist_instance;
		}
		if (check_same_various) {
			return;
		}
	}

	if (cupdsrc == updRelSrc::ArtistProfile && !cupdsrc.extended) {

		if (artist.get()) {

			g_discogs->find_release_dialog->UpdateArtistProfile(artist);
		}
		return;
	}

	m_current_qdm = m_dlg->m_qdm_search_query;

	{
		try {
			std::lock_guard<std::mutex> guard(g_discogs->tree_locked_mutex);

			m_rt_cache.vec_Clear();
			m_rt_cache.bulk_Clear();
			m_rt_cache.new_Version();

		}
		catch (...) {
			pfc::string8 kk;
			kk = kk;
			return;
		}
	}

	if (artist) {
		m_idtracer_p->set_artist_ovr_id(atoi(artist->id));
	}
	else {
		m_idtracer_p->set_artist_ovr_id(~0);
	}

	g_discogs->find_release_dialog->UpdateArtistProfile(artist);

	set_find_release_artist(artist);

	// INIT HOOK
	pfc::string8 hint;
	init_titles(artist, hint);
	//
	if (m_dlg->is_filter_autofill_enabled() && artist.get()) {

		m_results_filter.set_string(hint);

        if (!m_filter_disabled_title_mismatch && hint.get_length()) {

			CFindReleaseDialog* dlg = (CFindReleaseDialog*)m_dlg;

			if (dlg->is_filter_autofill_enabled()) {

				dlg->block_filter_box_events(true);

				uSetWindowText(m_edit_filter, hint/*filter*/);

				dlg->block_filter_box_events(false);
			}
		}

	}
	else {
		if (cupdsrc == updRelSrc::Undef || cupdsrc == updRelSrc::UndefFast) {
			m_results_filter.set_string(hint);
			uSetWindowText(m_edit_filter, hint/*filter*/);
		}
	}

	//filter string...
	if (m_idtracer_p->has_amr()) {

		init_tracker_i(artist, m_results_filter, m_results_filter, false, true);
	}

	bool rolemainAT = m_dlg->m_query_mode & SearchMode::AT || m_dlg->m_query_mode & SearchMode::VA;  //m_dlg->config().find_release_dlg_flags & FilterFlag::RoleMainAT;
	bool brolemain_filter = !rolemainAT &&  (m_dlg->config().find_release_filter_flag & FilterFlag::RoleMain);

	//

	// update releases

	//

	std::pair<rppair_t, rppair_t>  res = {};

	{
		try {
			std::lock_guard<std::mutex> guard(g_discogs->tree_locked_mutex);
			bool bak_disp_info = m_dispinfo_enabled;
			if (!m_dlg->config().auto_load_releases_on_select_ready()) {
				m_dispinfo_enabled = false;
			}

				if (m_results_filter.get_length() && cupdsrc == updRelSrc::ArtistProfile/* && init_expand*/) {

					res = update_releases(m_results_filter, cupdsrc, false, brolemain_filter);

					//moved from another artist with an active filter
					//TODO!
					abort_callback_dummy dummy_abort;
					fake_threaded_process_status fake_ps_status;
					apply_filter(m_results_filter, brolemain_filter, true, true, fake_ps_status, dummy_abort);
				}
				else {
					res = update_releases(m_results_filter, cupdsrc, true, brolemain_filter);
				}

				if (!m_dlg->config().auto_load_releases_on_select_ready()) {
					m_dispinfo_enabled = bak_disp_info;
				}

		}
		catch (...) {
			return;
		}
	}

	std::pair<pfc::string8, pfc::string8> pair_name_id = std::pair("", "");
	if (artist) {
		pair_name_id = std::pair(artist->name, artist->id);
		if (!artist->loaded_releases) {
			bool bpartial = cupdsrc == updRelSrc::ArtistSearchAT || (artist->master_releases.get_count() || artist->releases.get_count());
			pair_name_id.first << (bpartial ? " (partial)" : "");
		}
	}

	auto ut = [res, pair_name_id] {
		try {
			g_discogs->find_release_dialog->InvalidateSearchDisplay(true);

			rppair rp = rppair(std::pair(std::to_string(res.second.first.first).c_str(), std::to_string(res.second.first.second).c_str()),
				pair_name_id);
			g_discogs->find_release_dialog->print_root_stats(rp);
		}
		catch (...) {}
	};

	fb2k::inMainThread(ut);

}

const foo_conf* release_tree_cache::get_conf() { return &m_rt_manager->m_dlg->config(); }

rppair_t release_tree_cache::init_filter(const Artist_ptr artist, pfc::string8 filter, bool brolemain_filter, bool expanded, bool fast, bool no_alloc, id_tracer* tracer_p) {

	int pos = -1;
	int master_ndx = 0;
	int release_ndx = 0;
	bool master_done = false;
	bool release_done = false;
	std::pair<t_size, t_size> reserve = std::pair(0, 0);

	if (!artist) {
		return rppair_t(reserve, { master_ndx, release_ndx });
	}

	titleformat_hook_impl_multiformat_ptr hook = m_rt_manager->get_hook();

	bool bversions_filter = get_conf()->find_release_filter_flag & FilterFlag::Versions;

	if (no_alloc) {
		fast = false; expanded = true;
	}

	m_vec_filter.clear();

	pfc::array_t<pfc::string> lcf_words;
	bool filtered = filter.get_length() && tokenize_filter(filter, lcf_words);


	for (size_t walk = 0; walk < artist->search_order_master.get_size(); walk++) {

		bool matches_master = false;
		bool matches_release = false;

		if (master_done && release_done && fast)
			break;

		bool bmaster = artist->search_order_master[walk];

		if (bmaster) {

			// ---- MASTERS ----

			MasterRelease_ptr mr_p = artist->master_releases[master_ndx];
			//>check
			matches_master = true;
			if (filtered) {

				row_col_data row_data{};

				pfc::string8 item;

				if (search_query::is_undefined_artist(artist->id, true)) {

					pfc::string8 search_formatted;

					fake_hook_undefined_master(mr_p, search_formatted);

					pfc::string8 id = mr_p->id;
					row_data.col_data_list.emplace_back(std::make_pair(0, search_formatted));
					row_data.id = atoi(id);

					item = search_formatted;
					hook->set_master(&mr_p);
				}
				else {
					hook->set_master(&mr_p);
					item = m_rt_manager->run_hook_columns(row_data, (master_ndx << 16) | 9999);
				}

				matches_master = check_match(item, filter, lcf_words);
			}

			if (brolemain_filter) {
				std::string str_role = std::string(mr_p->search_role);
				matches_master &= (str_role.find("Main") != str_role.npos);
			}
			//<check

			if (matches_master) {
				pos++;

				if (atoi(artist->master_releases[master_ndx]->id) == tracer_p->master_id) {
					tracer_p->master_pos = pos;
				}

				// ** EMPLACE MASTER FILTER

				if (!no_alloc)
					m_vec_filter.emplace_back(std::pair<int, int>(master_ndx, pfc_infinite));

				reserve.first++;

				master_done = true;
			}

			for (size_t j = 0; j < artist->master_releases[master_ndx]->sub_releases.size(); j++) {

				// ---- SUB-RELEASES ----

				Release_ptr r_p = artist->master_releases[master_ndx]->sub_releases[j];

				matches_release = true;
				//>check sub-release
				if (bversions_filter && filtered) {

					pfc::string8 sub_item;
					row_col_data row_data{};
					if (search_query::is_undefined_artist(artist->id, true)) {
						pfc::string8 search_formatted;
						fake_hook_undefined_release(artist->master_releases[master_ndx]->sub_releases[j], search_formatted);

						pfc::string8 id = artist->master_releases[master_ndx]->sub_releases[j]->id;
						row_data.col_data_list.emplace_back(std::make_pair(0, search_formatted));
						row_data.id = atoi(id);

						sub_item = search_formatted;
						hook->set_release(&(artist->master_releases[master_ndx]->sub_releases[j]));
					}
					else {
						hook->set_release(&(artist->master_releases[master_ndx]->sub_releases[j]));
						sub_item = m_rt_manager->run_hook_columns(row_data, (master_ndx << 16) | j);
					}

					matches_release = check_match(sub_item, filter, lcf_words);
				}
				//<end check

				if (matches_release) {

					pos++;

					if (atoi(artist->master_releases[master_ndx]->sub_releases[j]->id) == tracer_p->release_id) {
						tracer_p->release_pos = pos + j;
					}

					// ** EMPLACE SUB-RELEASE FILTER

					if (!no_alloc)
						m_vec_filter.emplace_back(std::pair<size_t, size_t>(master_ndx, j));

					reserve.second++;

					release_done = true;
				}
			}
		}
		else {

			// ---- NON-MASTER RELEASES

			Release_ptr non_master = artist->releases[release_ndx];

			//>check non-master release
			matches_release = true;
			if (filtered) {

				row_col_data row_data{};
				pfc::string8 item;

				if (search_query::is_undefined_artist(artist->id, false)) {
					pfc::string8 search_formatted;

					fake_hook_undefined_release(artist->releases[release_ndx], search_formatted);

					pfc::string8 id = artist->releases[release_ndx]->id;
					row_data.col_data_list.emplace_back(std::make_pair(0, search_formatted));
					row_data.id = atoi(id);

					item = search_formatted;
					hook->set_release(&(artist->releases[release_ndx]));
				}
				else {

					hook->set_release(&(artist->releases[release_ndx]));
					item = m_rt_manager->run_hook_columns(row_data, (9999 << 16) | release_ndx);
				}

				matches_release = check_match(item, filter, lcf_words);
			}

			if (brolemain_filter) {
				std::string str_role = std::string(non_master->search_role);
				pfc::array_t<pfc::string8> search_roles = non_master->search_roles;
				for (auto it : search_roles) {
					if (it.equals("Main"))
						break;
				}

				matches_release &= (str_role.find("Main") != str_role.npos);
			}
			//<end check

			if (matches_release) {

				pos++;

				if (tracer_p->need_release()) {
					tracer_p->release_check(non_master->id, pos, mounted_param(pfc_infinite, release_ndx, false, true), pfc_infinite, pfc_infinite);
				}
				// ** EMPLACE NON-MASTER RELEASE FILTER

				if (!no_alloc)
					m_vec_filter.emplace_back(std::pair<int, int>(pfc_infinite, release_ndx));

				reserve.second++;
				release_done = true; master_done = true;
			}
		}

		if (bmaster)
			master_ndx++;
		else
			release_ndx++;
	}
	return rppair_t(reserve, { master_ndx, release_ndx });
}

pfc::string8 CFindReleaseTree::get_param_id(mounted_param myparam) {

    pfc::string8 id;
    if (!m_find_release_artist) return id;

    const auto kind = myparam.is_master() ? discogger_tree::row_kind::master :
        myparam.is_release() ? discogger_tree::row_kind::subrelease :
        myparam.is_nmrelease() ? discogger_tree::row_kind::standalone :
                                discogger_tree::row_kind::invalid;
    if (kind == discogger_tree::row_kind::invalid) return id;

    MasterRelease_ptr master;
    if (kind == discogger_tree::row_kind::master ||
        kind == discogger_tree::row_kind::subrelease) {
        if (myparam.master_ndx >= m_find_release_artist->master_releases.get_count())
            return id;
        master = m_find_release_artist->master_releases[myparam.master_ndx];
        if (!master) return id;
    }
    const size_t child_count = master ? master->sub_releases.get_count() : 0;
    if (!discogger_tree::valid_row_indices(kind, myparam.master_ndx,
            myparam.release_ndx, m_find_release_artist->master_releases.get_count(),
            child_count, m_find_release_artist->releases.get_count()))
        return id;

    if (kind == discogger_tree::row_kind::master)
        return master->main_release_id;
    if (kind == discogger_tree::row_kind::subrelease) {
        const auto& child = master->sub_releases[myparam.release_ndx];
        return child ? child->id : id;
    }
    const auto& release = m_find_release_artist->releases[myparam.release_ndx];
    return release ? release->id : id;
}

int release_tree_cache::get_src_param(updRelSrc updsrc, id_tracer* tracer_p) {

	int src_lparam = -1;

	if (m_vec_ptr->size()) {

		mounted_param myparam;
		if (tracer_p->has_amr()) {
			myparam = tracer_p->release_i.lparam();

			vec_iterator_t vec_lv_it;
			find_vec_by_lparam(*m_vec_ptr, src_lparam, vec_lv_it);

			if (vec_lv_it != m_vec_ptr->end()) {
				src_lparam = myparam.lparam();
				return src_lparam;
			}
			else {

				//release not loaded, try to select master and expand

				mounted_param not_found;
				myparam = mounted_param(tracer_p->master_i, ~0, tracer_p->master_tag, false);
				src_lparam = myparam.lparam();
				vec_iterator_t vec_lv_it;
				find_vec_by_lparam(*m_vec_ptr, src_lparam, vec_lv_it);

				if (vec_lv_it != m_vec_ptr->end()) {

					src_lparam = myparam.lparam();

					int ival = 1;
					if (updsrc == updRelSrc::Undef) {

						m_cache_ptr->SetCacheFlag(vec_lv_it->first, NodeFlag::expanded, &ival);

						m_rt_manager->SetHit(src_lparam);
					}

					return src_lparam;
				}
				else {
					//release not loaded, trying to select master to expand
					src_lparam = tracer_p->release_i.lparam();
					vec_iterator_t vec_lv_it;
					find_vec_by_lparam(*m_vec_ptr, src_lparam, vec_lv_it);

					if (vec_lv_it != m_vec_ptr->end()) {
						src_lparam = myparam.lparam();
						int ival = 1;
						m_cache_ptr->SetCacheFlag(vec_lv_it->first, NodeFlag::expanded, &ival);
						return src_lparam;
					}
				}
			}
		}
		else {
			vec_iterator_t vec_lv_it;
			find_vec_by_id(*m_vec_ptr, tracer_p->master_id, vec_lv_it);

			if (vec_lv_it != m_vec_ptr->end()) {
				src_lparam = vec_lv_it->first->first;

				return src_lparam;
			}
		}
	}
	return src_lparam;
}

bool CFindReleaseTree::on_tree_display_cell_image(size_t item, size_t subitem, size_t id, cache_iterator_t cache_it, int& result) {

	bool children_done = false;
	m_rt_cache.get_bulk()->GetCacheFlag(cache_it, NodeFlag::added, &children_done);

	bool tree_children_done = false;
	bool tree_children_done_ver = m_rt_cache.get_bulk()->GetCacheFlag(cache_it, NodeFlag::tree_created, &tree_children_done);

	bool is_traced;
	if (item != pfc_infinite) {
		if (subitem != pfc_infinite)
			is_traced = (id == m_idtracer_p->release_id);
		else
			is_traced = (id == m_idtracer_p->master_id);
	}
	else {
		is_traced = (id == m_idtracer_p->release_id);
	}
	result = is_traced ? TRACER_IMG_NDX + (tree_children_done ? 1: 0) : tree_children_done ? CHILDREN_DONE_IMG_NDX : I_IMAGENONE/*-1*/;
	return true;
}

void CFindReleaseTree::set_image_list() {
	bool bdark = fb2k::isDarkMode() || check_os_wine_dark_no_theme();
	m_hImageList.Create(bdark ? IDB_BITMAP_TRACER_EXT_DRK : IDB_BITMAP_TRACER_EXT, 16, ILC_COLOR4, bdark ? RGB(0, 0, 0) : RGB(255, 255, 255));
	SendMessage(m_hwndTreeView, TVM_SETIMAGELIST, 0, (LPARAM)(HIMAGELIST)m_hImageList);
}

void CFindReleaseTree::Inititalize(HWND treeview, HWND filter_edit, HWND url_edit,
	metadb_handle_list items, pfc::string8 filter_hint) {

	m_idtracer_p->enabled = true;
	m_edit_filter = filter_edit;
	m_edit_release = url_edit;
	m_hwndTreeView = treeview;

	m_items = items;
	m_results_filter.set_string(filter_hint);

	set_image_list();

	CWindow(m_hwndTreeView).ModifyStyle(0, TVS_INFOTIP, TVS_INFOTIP);
}

int CFindReleaseTree::get_param_id_master(mounted_param myparam) {
    if (!myparam.is_master() || !m_find_release_artist ||
        myparam.master_ndx >= m_find_release_artist->master_releases.get_count())
        return pfc_infinite;
    const auto& master = m_find_release_artist->master_releases[myparam.master_ndx];
    return master ? atoi(master->id) : pfc_infinite;
}

// ON RELEASE SELECTED

void CFindReleaseTree::on_release_selected(int src_lparam) {

	const std::shared_ptr<vec_t> vec_items = m_rt_cache.get_vec();

	vec_iterator_t vecpos;
	find_vec_by_lparam(*vec_items, src_lparam, vecpos);

	if (vecpos == vec_items->end()) {
		//..
		return;
		//..
	}

	int selection_index = std::distance(vec_items->begin(), vecpos);
	mounted_param myparam(vec_items->at(selection_index).first->first);

    if (myparam.is_master()) {
        // Selection notifications can arrive after the tree was rebuilt.
        // Never expand an evicted master from a stale cached row.
        if (!m_find_release_artist ||
            myparam.master_ndx >= m_find_release_artist->master_releases.get_count() ||
            !m_find_release_artist->master_releases[myparam.master_ndx])
            return;
        MasterRelease_ptr master_release = m_find_release_artist->master_releases[myparam.master_ndx];

		pfc::string8 input_edit_release;
		if (m_idtracer_p->has_amr() && (get_param_id_master(myparam) == m_idtracer_p->master_id))
			input_edit_release << m_idtracer_p->release_id;
		else
			input_edit_release << get_param_id(myparam);

		if (m_rt_cache.get_bulk()->GetCacheFlag(src_lparam, NodeFlag::expanded)) {

            if (master_release->sub_releases.get_size()) {

				abort_callback_dummy dummy;
				fake_threaded_process_status fstatus;

				on_expand_master_release_done(master_release, selection_index, fstatus, dummy);
			}
			else {

				//SPAWN

                // EXPAND the validated local master snapshot.
				m_dlg->call_expand_master_service(master_release, selection_index);

				// NOTHING MORE TO DO
			}
		}
		else
		{
			//closing master branch
			uSetWindowText(m_edit_release, input_edit_release);
		}
	}
	else {
		uSetWindowText(m_edit_release, get_param_id(myparam).get_ptr());
	}
}

LRESULT CFindReleaseTree::apply_filter(pfc::string8 filter, bool brolemain, bool force_redraw, bool force_rebuild, threaded_process_status& p_status, abort_callback& p_abort) {

	try {


		std::pair<rppair_t, rppair_t> resource_stat;

		size_t cc_filtered_root = 0;
		size_t cc_filtered_expanded = 0;
		size_t cc_non_master = 0;
		size_t cc_filtered_masters = 0;

		filter = trim(filter);

		//pre-condition: artist and filter changed

		bool bfilter_changed = stricmp_utf8(m_results_filter, filter);
		bool has_ra = m_find_release_artist.get();
		bool has_rar_av = find_release_artist_releases_available();
		if (!m_find_release_artist.get() || !find_release_artist_releases_available() || (!bfilter_changed && !m_filter_disabled_title_mismatch && !force_redraw)) {

			m_results_filter.set_string(filter);

			//nothing more to do

			return FALSE;
		}

		m_filter_disabled_title_mismatch = false;

		// VER ++

		m_rt_cache.get_bulk()->_ver++;

		if (!force_rebuild && filter.get_length() == 0) {

			m_rt_cache.vec_Clear();

			m_idtracer_p->release_reset();

			// * UPDATE RELELASES *

			resource_stat = update_releases(filter, updRelSrc::Filter, true, brolemain);

			cc_filtered_root = resource_stat.first.first.first;
			cc_filtered_expanded = resource_stat.first.first.second;
		}
		else {

			// * FILTER RELEASES *


			SendMessage(m_hwndTreeView, WM_SETREDRAW, 0, 0);

			TreeView_DeleteAllItems(m_hwndTreeView);

			SendMessage(m_hwndTreeView, WM_SETREDRAW, 1, 0);

			m_rt_cache.vec_Clear();

			m_rt_cache.new_Version();


			//reset tracer
			m_idtracer_p->release_reset();

			//init vfilter
			rppair_t res = m_rt_cache.init_filter(m_find_release_artist, filter, brolemain, false, false, false, m_idtracer_p);

			//reset release position (keep ids)
			m_idtracer_p->release_reset();

			pfc::array_t<pfc::string> lcf_words;
			bool filtered = tokenize_filter(filter, lcf_words);

			std::vector<std::pair<size_t, size_t>>::iterator it;

			// APPLY VFILTER 

			const std::shared_ptr<vec_t> vec_items = m_rt_cache.get_vec();
			std::vector<std::pair<size_t, size_t>> vec_filter = m_rt_cache.get_filter_vec();

			for (it = vec_filter.begin(); it != vec_filter.end(); it++) {

				if (p_abort.is_aborting()) {
					return FALSE;
				}

				t_size master_ndx = it->first;
				t_size release_ndx = it->second;

				const mounted_param myparam = { master_ndx, release_ndx, master_ndx != -1, release_ndx != -1 };

				pfc::string8 item;
				row_col_data row_data;

				if (myparam.is_master()) {
					//..
				}
				else {
					cache_iterator_t parent_it;
					if (myparam.is_release()) {
						std::vector<std::pair<int, int>>::iterator it_parent;
						int master_id = atoi(m_find_release_artist->master_releases[myparam.master_ndx]->id);
						mounted_param parent_mp(myparam.master_ndx, ~0, true, false);
						int parent_lparam = parent_mp.lparam();

						vec_iterator_t parent;
						find_vec_by_lparam(*vec_items, parent_lparam, parent);
						if (parent != vec_items->end()) {

							parent_it = parent->first;

							//PARENT FOUND

							bool bexp = false;
							bool bthisver = m_rt_cache.get_bulk()->GetCacheFlag(parent->first, NodeFlag::expanded, &bexp);
							int ival = bexp ? 1 : 0;
							m_rt_cache.get_bulk()->SetCacheFlag(parent->first, NodeFlag::expanded, &ival);

						}
						else {
							//INSERT NOT-MATCHING PARENT
							mounted_param mp_master(master_ndx, ~0, true, false);

							bool bversions_filter = m_dlg->config().find_release_filter_flag & FilterFlag::Versions;
							if (!bversions_filter) {
								MasterRelease_ptr mr_p = m_find_release_artist->master_releases[master_ndx];
								if (filtered) {
									std::string str_role = std::string(mr_p->search_role);
									bool matches_master = (!brolemain || (str_role.find("Main") != str_role.npos)) && check_match(mr_p->title, filter, lcf_words);
									if (!matches_master) {
										int ival = 0;
										m_rt_cache.get_bulk()->SetCacheFlag(it->first, NodeFlag::filterok, &ival);
										continue;
									}
								}
							}

							m_rt_cache.get_cached_find_release_node(mp_master.lparam(), item, row_data);
							parent_it = m_rt_cache.get_bulk()->cache.find(mp_master.lparam());

							bool bexp = false;
							bool bthisver = m_rt_cache.get_bulk()->GetCacheFlag(parent_it, NodeFlag::expanded, &bexp);
							int ival = bexp ? 1 : 0;
							m_rt_cache.get_bulk()->SetCacheFlag(parent_it, NodeFlag::expanded, &ival);

							ival = 1;
							m_rt_cache.get_bulk()->SetCacheFlag(parent_it, NodeFlag::added, &ival);
							//in filter
							m_rt_cache.get_bulk()->SetCacheFlag(parent_it, NodeFlag::filterok, &ival);

							flagreg& insfr = parent_it->second.second;

							// ** VEC PUSH **

							m_rt_cache.vec_PushBack(std::pair<cache_iterator_t, HTREEITEM>(parent_it, nullptr));
							vec_iterator_t  master_row;
							find_vec_by_id(*vec_items, master_id, master_row);
						}
					}
				}
				if (m_rt_cache.get_cached_find_release_node(myparam.lparam(), item, row_data) != true) {
					return FALSE;
				}

				//

				cache_iterator_t filtered_it = m_rt_cache.get_bulk()->cache.find(myparam.lparam());
				int ival = 1;
				m_rt_cache.get_bulk()->SetCacheFlag(filtered_it, NodeFlag::filterok, &ival);

				// ** VEC_PUSH **
				m_rt_cache.vec_PushBack(std::pair<cache_iterator_t, HTREEITEM>(filtered_it, nullptr));

				cc_filtered_masters = m_rt_cache.vec_Size();
				cc_non_master = m_rt_cache.bulk_Size() - cc_filtered_masters;
			}

			// BUILD TREE ITEMS

			int counter = 0;

			std::vector<HTREEITEM> v_hItem_expanded_masters;
			std::shared_ptr<vec_t>& nu_vec_items = m_rt_cache.get_vec_ref();

			//

			SendMessage(m_hwndTreeView, WM_SETREDRAW, 0, 0);

			//


			for (auto& walk : *nu_vec_items)
			{
				mounted_param wmyparam(walk.first->first);
				if (!wmyparam.is_master() &&
					!wmyparam.is_nmrelease()) {
					continue;
				}

				TVINSERTSTRUCT tvis = { 0 };
				tvis.item.pszText = LPSTR_TEXTCALLBACK;
				tvis.item.iSelectedImage = I_IMAGECALLBACK;
				tvis.item.cChildren = I_CHILDRENCALLBACK;
				tvis.item.lParam = walk.first->first;

				tvis.hParent = TVI_ROOT;
				tvis.hInsertAfter = TVI_LAST;

				bool bbold = wmyparam.is_master() && (walk.first->second.first.id == m_idtracer_p->master_id);
				bbold |= wmyparam.is_nmrelease() && (walk.first->second.first.id == m_idtracer_p->release_id);

				if (bbold) {
					tvis.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_CHILDREN | TVIF_PARAM | TVIF_STATE;
					int img_ndx;
					on_tree_display_cell_image(wmyparam.master_ndx, wmyparam.release_ndx, walk.first->second.first.id, walk.first, img_ndx);

					tvis.item.iImage = TRACER_IMG_NDX/*img_ndx*/;
					tvis.item.iSelectedImage = TRACER_IMG_NDX/*img_ndx*/;
					tvis.item.state = TVIS_BOLD;
					tvis.item.stateMask = TVIS_BOLD;
				}
				else {
					tvis.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_CHILDREN | TVIF_PARAM | TVIF_STATE;
					tvis.item.iImage = -1;
					tvis.item.iSelectedImage = -1;
				}

				HTREEITEM newnode = TreeView_InsertItem(m_hwndTreeView, &tvis);
				walk.second = newnode;
				int ival = 0;
				std::shared_ptr<filter_cache>& cache_ptr = m_rt_cache.get_bulk();
				cache_ptr->SetCacheFlag(walk.first, NodeFlag::tree_created, &ival);
				bool val = false;
				if (cache_ptr->GetCacheFlag(walk.first, NodeFlag::expanded, &val)) {
					v_hItem_expanded_masters.emplace_back(newnode);
				}
			}

			for (auto hnode : v_hItem_expanded_masters) {
				TreeView_Expand(m_hwndTreeView, hnode, TVM_EXPAND);
			}

			//

			SendMessage(m_hwndTreeView, WM_SETREDRAW, 1, 0);

			//
		}

		m_results_filter.set_string(filter);
	}
	catch (foo_discogs_exception e) {
		//..
	}

	return FALSE;
}

void CFindReleaseTree::expand_releases(const pfc::string8& filter, t_size master_index, t_size master_list_pos) {

	try {
		//forward expand

		m_rt_cache.expand_releases(filter, master_index, master_list_pos, m_idtracer_p);

		//signal branch hit node to TVM_EXPAND
		ExpandHit();
	}
	catch (foo_discogs_exception& e) {
		foo_discogs_exception ex;
		ex << "Error formating release list subitem [" << e.what() << "]";
		throw ex;
	}
}


void release_tree_cache::expand_releases(const pfc::string8& filter, t_size master_index, t_size master_list_pos, id_tracer* tracer_p) {

	titleformat_hook_impl_multiformat_ptr hook = m_rt_manager->get_hook();

	pfc::array_t<Artist_ptr> m_find_release_artists = m_rt_manager->get_find_release_artists();
	Artist_ptr m_find_release_artist = m_rt_manager->get_find_release_artist();

	bool bversions_filter = get_conf()->find_release_filter_flag & FilterFlag::Versions;

	pfc::array_t<pfc::string> lcf_words;
	bool filtered = tokenize_filter(filter, lcf_words);

	int list_index = master_list_pos + 1;

	int walk_lparam;
	row_col_data row_data;

	try {
		int skipped = 0;
		mounted_param parent_mp(master_index, ~0, true, false);
		int parent_lparam = parent_mp.lparam();
		cache_t::const_iterator parent_cache = m_cache_ptr->cache.find(parent_lparam);
		cache_t::const_iterator new_release_cache;
		if (parent_cache == m_cache_ptr->cache.end()) {
			int debug = 1;
		}
		try {

			m_rt_manager->get_hook()->set_master(nullptr);
			for (size_t j = 0; j < m_find_release_artist->master_releases[master_index]->sub_releases.get_size(); j++) {

				row_col_data row_data_subitem;
				hook->set_release(&(m_find_release_artist->master_releases[master_index]->sub_releases[j]));
				hook->set_master(&(m_find_release_artist->master_releases[master_index]));

				mounted_param thisparam = { master_index, j, true, true };
				walk_lparam = thisparam.lparam();
				pfc::string8 sub_item;
				if (!get_cached_find_release_node(thisparam.lparam(), sub_item, row_data)) {

					// CACHE MAP EMPLACE
					sub_item = m_rt_manager->run_hook_columns(row_data, walk_lparam);
					cache_t& cache_map = m_cache_ptr->cache;
					cache_map.emplace(walk_lparam,
						std::pair<row_col_data, flagreg> { row_data, { get_Version(), 0 } });
					//
				}

				bool filter_match = !bversions_filter || !filtered || (filtered && check_match(sub_item, filter, lcf_words));

				if (filter_match) {

					cache_t& cache_map = m_cache_ptr->cache;
					cache_iterator_t walk_children = cache_map.find(walk_lparam);

					int ival = 1;
					//leaf spawned
					m_cache_ptr->SetCacheFlag(walk_children, NodeFlag::spawn, &ival);
					//leaf matched filter
					m_cache_ptr->SetCacheFlag(walk_children, NodeFlag::filterok, &ival);

					//tracer
					int row_pos = list_index + j - skipped;
					Release_ptr release = m_find_release_artist->master_releases[master_index]->sub_releases[j];
					tracer_p->release_check(release->id, row_pos, walk_lparam, master_list_pos, master_index);
				}
				else {

					skipped++;
				}
			}
		}
		catch (foo_discogs_exception& e) {
			foo_discogs_exception ex;
			ex << "Error formating release list item [" << e.what() << "]";
			throw ex;
		}

		//mark as pending (children not done) so branches are generated by OnTreeGetInfo->I_CHILDRENCALLBACK->OnTreeExpanding
		m_cache_ptr->SetCacheFlag(parent_lparam, NodeFlag::tree_created, false);
	}
	catch (foo_discogs_exception& e) {
		foo_discogs_exception ex;
		ex << "Error formating release list subitem [" << e.what() << "]";
		throw ex;
	}
	return;
}

// from dlg callback (expand_master_release_process_callback)
// or from local on_release_selected
void CFindReleaseTree::on_expand_master_release_done(const MasterRelease_ptr& master_release, int list_index, threaded_process_status& p_status, abort_callback& p_abort) {

	int master_i = -1;
	for (size_t i = 0; i < m_find_release_artist->master_releases.get_size(); i++) {
		if (m_find_release_artist->master_releases[i]->id == master_release->id) {
			master_i = i;
			break;
		}
	}

	if (master_i == -1) {
		//do not expand artist selection changed
		return; 
	}

	pfc::string8 filter = get_edit_filter_string();

	int state_expanded = 1;
	bool bnodeset = set_node_expanded(atoi(master_release->id), state_expanded, false);

	if (bnodeset) {

		m_rt_cache.expand_releases(filter.c_str(), master_i, list_index, m_idtracer_p);

		//signal branch node to TVM_EXPAND
		ExpandHit();

		mounted_param mparam(master_i, ~0, true, false);
		t_size lparam = mparam.lparam();

		std::shared_ptr<filter_cache>& cache_ptr = m_rt_cache.get_bulk();

		int ival = 1; // * NODE EXPANDED
		cache_ptr->SetCacheFlag(lparam, NodeFlag::expanded, ival);

		//todo: fix patch empty/duplicated entries
		if (g_os_is_wine) {
			::InvalidateRect(m_hwndTreeView, {0}, true);
		}

	}

	const std::shared_ptr<vec_t> vec_items = m_rt_cache.get_vec();

	mounted_param myparam(vec_items->at(list_index).first->first);

	pfc::string8 release_url;

	//autofill release_id textbox ...
	size_t autofill_id = get_autofill_release_id(myparam, atoi(master_release->id));

	if (autofill_id != ~0)
		uSetWindowText(m_edit_release, std::to_string(autofill_id).c_str());

}

bool CFindReleaseTree::set_node_expanded(t_size master_id, int& state, bool build) {

	vec_iterator_t master_row;
	std::shared_ptr<vec_t>& vec_items = m_rt_cache.get_vec_ref();

	find_vec_by_id(*vec_items, master_id, master_row);

	if (master_row != vec_items->end()) {

		std::shared_ptr<filter_cache>& cache_ptr = m_rt_cache.get_bulk();

		int ival_expanded = state;
		cache_ptr->SetCacheFlag(master_row->first, NodeFlag::expanded, &ival_expanded);

		if (ival_expanded) {
			int ival_added = 1;
			cache_ptr->SetCacheFlag(master_row->first, NodeFlag::added, &ival_added);
		}
		return true;
	}
	else {
		state = -1;
	}
	return false;
}

// TVN_GETDISPINFO
// expanding...

LRESULT CFindReleaseTree::OnReleaseTreeExpanding(int, LPNMHDR hdr, BOOL&) {

	const std::shared_ptr<vec_t> vec_items = m_rt_cache.get_vec();
	std::shared_ptr<filter_cache> cache_ptr = m_rt_cache.get_bulk();

	NMTREEVIEW* pNMTreeView = (NMTREEVIEW*)hdr;
	TVITEMW* pItemExpanding = &(pNMTreeView)->itemNew;
	HTREEITEM* hItemExpanding = &(pNMTreeView->itemNew.hItem);

	// early leaving

	if ((pNMTreeView->action & TVE_EXPAND) != TVE_EXPAND)
	{
		vec_iterator_t master_it;
		find_vec_by_lparam(*vec_items, pItemExpanding->lParam, master_it);
		if (master_it != vec_items->end()) {
			cache_iterator_t cache_it = master_it->first;
			int ival = 0;  // * NOT EXPANDED
			cache_ptr->SetCacheFlag(cache_it, NodeFlag::expanded, &ival);
		}
		return FALSE;
	}

	bool fix_wine = g_os_is_wine;

	if (pItemExpanding->mask & TVIF_CHILDREN || fix_wine)
	{
		mounted_param myparam(pItemExpanding->lParam);
		auto cache_parent = cache_ptr->cache.find(pItemExpanding->lParam);

		bool children_done = false;
		cache_ptr->GetCacheFlag(cache_parent, NodeFlag::added, &children_done);

		bool tree_children_done = false;
		bool tree_children_done_ver = cache_ptr->GetCacheFlag(cache_parent, NodeFlag::tree_created, &tree_children_done);

		if (children_done) {

			if (!tree_children_done_ver) {
				if (myparam.is_master()) {
					t_size try_release_ndx = 0;
					std::vector<TVINSERTSTRUCT> vchildren = {};

					int walk_param = mounted_param(myparam.master_ndx, try_release_ndx, true, true).lparam();
					auto walk_children = cache_ptr->cache.find(walk_param);
					int newindex = 0;
					while (walk_children != cache_ptr->cache.end()) {

						bool ival = false;
						cache_ptr->GetCacheFlag(walk_children, NodeFlag::spawn, &ival);
						bool bfilter = false;
						bool bfilter_ver = cache_ptr->GetCacheFlag(walk_children, NodeFlag::filterok, &bfilter);

						//..

						if (vec_items->size() == 0 || (vec_items->size() && bfilter_ver)) {

							vchildren.resize(newindex + 1);
							int lparam = walk_children->first;
							mounted_param mp(lparam);

							TV_INSERTSTRUCT* tvis = (TV_INSERTSTRUCT*)&vchildren.at(newindex);
							tvis->hParent = (HTREEITEM)pItemExpanding->hItem;
							tvis->hInsertAfter = TVI_LAST;
							TV_ITEM tvi;
							memset(&tvi, 0, sizeof(tvi));

							tvis->item.pszText = LPSTR_TEXTCALLBACK;
							tvis->item.iSelectedImage = I_IMAGECALLBACK;
							tvis->item.cChildren = 0;
							tvis->item.lParam = walk_children->first;

							int walk_id = walk_children->second.first.id;

							if (walk_id == m_idtracer_p->release_id) {
								tvis->item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_CHILDREN | TVIF_PARAM | TVIF_STATE;
								tvis->item.iImage = TRACER_IMG_NDX;
								tvis->item.iSelectedImage = TRACER_IMG_NDX;
								tvis->item.state = TVIS_BOLD;
								tvis->item.stateMask = TVIS_BOLD;
							}
							else {
								tvis->item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_CHILDREN | TVIF_PARAM | TVIF_STATE; //TVIF_IMAGE | TVIF_TEXT | TVIF_SELECTEDIMAGE;
								tvis->item.iImage = -1;
								tvis->item.iSelectedImage = -1;
							}

							newindex++;
						}
						walk_param = mounted_param(myparam.master_ndx, ++try_release_ndx, true, true).lparam();
						walk_children = cache_ptr->cache.find(walk_param);
					}

					for (const auto& tvchild : vchildren) {
						HTREEITEM node = TreeView_InsertItem(m_hwndTreeView, &tvchild);
					}

					int ival = 1; //node added, set flag tree_created 
					cache_ptr->SetCacheFlag(pItemExpanding->lParam, NodeFlag::tree_created, ival);

					int ivalex = 1; //and expanded
					cache_ptr->SetCacheFlag(pItemExpanding->lParam, NodeFlag::expanded, &ivalex);

					//children #
					pItemExpanding->cChildren = vchildren.size();
				}
				else {

					// END EXPANDING NON MASTER
					size_t ctracks = m_rt_cache.get_level_one_vec_track_count(pItemExpanding->lParam).second;

					//todo: complete level 2 with release tracks
					pItemExpanding->cChildren = ctracks; // ? ctracks /*I_CHILDRENCALLBACK*/ : 0;
					//..
				}
			}
			else {

				//children were already available, just mark as expanded

				int ival = 1;
				cache_ptr->SetCacheFlag(cache_parent, NodeFlag::expanded, &ival);

				if (myparam.is_nmrelease()) {
					size_t ctracks = m_rt_cache.get_level_one_vec_track_count(pItemExpanding->lParam).second;
					pItemExpanding->cChildren = ctracks;
				}

				return FALSE;
			}
		}
		else {

			if (myparam.is_master()) {

				//children not done -> spawn
				//store hItem to expand after spawn

				m_hit = pItemExpanding->hItem;
				//fix for compatibility with virtual listview

				vec_iterator_t master_it;
				find_vec_by_lparam(*vec_items, pItemExpanding->lParam, master_it);

				if (master_it == vec_items->end())
					return TRUE;

				int selection_index = std::distance(vec_items->begin(), master_it);

				if (master_it != vec_items->end()) {
					int ival_expanded = 1;
					cache_ptr->SetCacheFlag(master_it->first, NodeFlag::expanded, &ival_expanded);

					if (ival_expanded) {
						int ival_added = 1;
						cache_ptr->SetCacheFlag(master_it->first, NodeFlag::added, &ival_added);
					}
				}
				on_release_selected(pItemExpanding->lParam);
			}
		}
	}
	return FALSE;
}

//

// TVN_GETDISPINFO

//

LRESULT CFindReleaseTree::OnReleaseTreeGetInfo(WORD /*wNotifyCode*/, LPNMHDR hdr, BOOL& /*bHandled*/) {

	if (hdr == nullptr || hdr->hwndFrom != m_hwndTreeView || !g_discogs ||
		!m_rt_cache.vec_Size() || !m_dispinfo_enabled) return FALSE;

	std::lock_guard<std::mutex> guard(g_discogs->tree_locked_mutex);

	std::shared_ptr<filter_cache>& cache_ptr = m_rt_cache.get_bulk();

	NMTVDISPINFO* pDispInfo = reinterpret_cast<NMTVDISPINFO*>(hdr);
	TVITEMW* pItem = &(pDispInfo)->item;
	HTREEITEM* hItem = &(pItem->hItem);

	int lparam = pItem->lParam;
	mounted_param myparam(lparam);

	// Async refresh can evict rows before pending TEXT/IMAGE/CHILDREN callbacks.
	const auto cit = cache_ptr->cache.find(lparam);
	if (cit == cache_ptr->cache.end() || cit->second.first.col_data_list.empty()) {
		if ((pItem->mask & TVIF_TEXT) && pItem->pszText && pItem->cchTextMax > 0)
			pItem->pszText[0] = 0;
		if (pItem->mask & TVIF_IMAGE) {
			pItem->iImage = 0;
			pItem->iSelectedImage = 0;
		}
		if (pItem->mask & TVIF_CHILDREN) pItem->cChildren = 0;
		return FALSE;
	}

	std::pair<size_t, size_t>pdiscs_tracks;
	size_t ctracks = 0;
	if (myparam.is_nmrelease()) {
		pdiscs_tracks = m_rt_cache.get_level_one_vec_track_count(lparam);
		ctracks = pdiscs_tracks.second;
	}
	else if (myparam.is_release()) {
		pdiscs_tracks = m_rt_cache.get_level_two_cache_track_count(lparam);
		ctracks = pdiscs_tracks.second;
	}

	// asking for virtual children ?

	if (pItem->mask & TVIF_CHILDREN)
	{
		if (myparam.is_master()) {

			bool fix_wine = g_os_is_wine;

			//generate them for wine, otherwise leave then virtual

			if (fix_wine) {
				if (pDispInfo->item.hItem == m_hit) {

					int count = 0;
					int master_i = mounted_param(lparam).master_ndx;
					auto fit = std::find_if(cache_ptr->cache.begin(), cache_ptr->cache.end(), [=](const auto& au) {
						mounted_param thisparam = mounted_param(au.first);
						return thisparam.master_ndx == myparam.master_ndx; });

					while (fit != cache_ptr->cache.end()) {
						count++;
						fit = std::find_if(++fit, cache_ptr->cache.end(), [=](const auto& au) {
							mounted_param thisparam = mounted_param(au.first);
							return thisparam.master_ndx == myparam.master_ndx; });
					}

					pItem->cChildren = count;
				}
				else {
					pItem->cChildren = WINE_CHILDREN;
				}
			}
			else {

				pItem->cChildren = I_CHILDRENCALLBACK;
			}
		}
		else {
			// (tracks)
			pItem->cChildren = 0;
		}
	}

	if (pItem->mask & TVIF_TEXT)
	{
		// The cache row and its first display column were validated above.
    TCHAR outBuffer[MAX_PATH + 1] = {};
		auto row_data = cit->second.first.col_data_list.begin();

		//node literal...
    pfc::string8 nodetext(row_data->second);

		//total tracks...
		if (ctracks) {
			if (pdiscs_tracks.first > 1) {
				nodetext << " [x" << std::to_string(pdiscs_tracks.first).c_str() << ":";
				nodetext << std::to_string(ctracks).c_str() << "]";
			}
			else {
				nodetext << " [" << std::to_string(ctracks).c_str() << "]";
			}
		}

		//to wide and transfer
    pfc::stringcvt::convert_utf8_to_wide(outBuffer, MAX_PATH,
			nodetext.get_ptr(), nodetext.get_length());

		_tcscpy_s(pItem->pszText, pItem->cchTextMax, const_cast<TCHAR*>(outBuffer));
	}

	if (pItem->mask & TVIF_IMAGE) {


		int img_ndx;

		if (on_tree_display_cell_image(myparam.master_ndx, myparam.release_ndx, cit->second.first.id, cit, img_ndx)) {

			pItem->iImage = img_ndx;
			pItem->iSelectedImage = img_ndx;
		}
		else {
			pItem->iImage = I_IMAGECALLBACK;
			pItem->iSelectedImage = I_IMAGECALLBACK;
		}
	}
	return FALSE;
}

// ..

std::pair<t_size, t_size> release_tree_cache::get_level_one_vec_track_count(LPARAM lParam) {
    vec_iterator_t find_it;
    find_vec_by_lparam(*m_vec_ptr, lParam, find_it);
    if (find_it == m_vec_ptr->end() || !discogs_interface) return {0, 0};
    pfc::string8 release_id;
    release_id << std::to_string(find_it->first->second.first.id).c_str();
    const auto release = discogs_interface->get_release(encode_mr(0, release_id), false);
    return discogger_tree::count_cached_tracks(release);
}

std::pair<t_size, t_size> release_tree_cache::get_level_two_cache_track_count(LPARAM lparam) {
    const auto cache_it = m_cache_ptr->cache.find(lparam);
    if (cache_it == m_cache_ptr->cache.end() || !discogs_interface) return {0, 0};
    pfc::string8 release_id;
    release_id << std::to_string(cache_it->second.first.id).c_str();
    const auto release = discogs_interface->get_release(encode_mr(0, release_id), false);
    return discogger_tree::count_cached_tracks(release);
}

// release tree selection changed

LRESULT CFindReleaseTree::OnReleaseTreeSelChanged(int, LPNMHDR hdr, BOOL& bHandled) {

    if (!m_dispinfo_enabled || !hdr || hdr->hwndFrom != m_hwndTreeView)
        return FALSE;
    const std::shared_ptr<filter_cache> cache_ptr = m_rt_cache.get_bulk();
    if (!cache_ptr) return FALSE;

	LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)hdr;
    if (!pnmtv->itemNew.hItem) return FALSE;
	TVITEM pItemMaster = { 0 };
	pItemMaster.mask = TVIF_CHILDREN | TVIF_PARAM;
	pItemMaster.hItem = pnmtv->itemNew.hItem;

    if (!TreeView_GetItem(m_hwndTreeView, &pItemMaster)) return FALSE;

	mounted_param myparam = mounted_param(pItemMaster.lParam);
	cache_iterator_t it = cache_ptr->cache.find(pItemMaster.lParam);

	if (it != cache_ptr->cache.end()) {

		size_t autofill_id = get_autofill_release_id(myparam, it->second.first.id);

		if (autofill_id != ~0)
			uSetWindowText(m_edit_release, std::to_string(autofill_id).c_str());
	}

	bHandled = FALSE;
	return 0;
}

//note:	some_id can be a master, release or nm-release
//			includes tracer checking (!master_id and release id)
size_t CFindReleaseTree::get_autofill_release_id(mounted_param myparam, size_t some_id) {

    // No stale row (master or standalone) may overwrite the release ID edit.
    if (!myparam.is_master()) {
        return get_param_id(myparam).get_length() ? some_id : ~size_t(0);
    }
    if (!m_find_release_artist ||
        myparam.master_ndx >= m_find_release_artist->master_releases.get_count())
        return ~size_t(0);
    const MasterRelease_ptr master = m_find_release_artist->master_releases[myparam.master_ndx];
    if (!master) return ~size_t(0);

    // Missing main_release_id is not the numeric release ID 0.
    const size_t main_id = is_number(master->main_release_id.c_str()) ?
        static_cast<size_t>(atoi(master->main_release_id.c_str())) : ~size_t(0);

    // Artist/master/release tracer: prefer the user's exact release.
    if (m_idtracer_p->has_master() && m_idtracer_p->has_release() &&
        m_idtracer_p->master_id == some_id)
        return m_idtracer_p->release_id;

    // Release-only tracer: inspect loaded children, skipping partial nulls.
    if (!m_idtracer_p->has_master() && m_idtracer_p->has_release()) {
        for (size_t j = 0; j < master->sub_releases.get_count(); ++j) {
            const auto& child = master->sub_releases[j];
            if (child && is_number(child->id.c_str()) &&
                atoi(child->id.c_str()) == m_idtracer_p->release_id)
                return m_idtracer_p->release_id;
        }
    }
    return main_id;
}

size_t CFindReleaseTree::test_getselected(TVITEM& out) {

	if (HTREEITEM hsel = TreeView_GetSelection(m_hwndTreeView)) {

		out.mask = TVIF_PARAM;
		out.hItem = hsel;

		TreeView_GetItem(m_hwndTreeView, &out);
		size_t lparam = out.lParam;

		return lparam;
	}
	return ~0;
}

size_t CFindReleaseTree::test_getatcursor(CPoint screen_pos, TVITEM& out, bool require_item_body) {

	TVHITTESTINFO tvhitinfo = { 0 };
	tvhitinfo.pt = screen_pos;
	::ScreenToClient(m_hwndTreeView, &tvhitinfo.pt);

	HTREEITEM hit = TreeView_HitTest(m_hwndTreeView, &tvhitinfo);

	if (hit && (!require_item_body || (tvhitinfo.flags & TVHT_ONITEM))) {
		out.mask = TVIF_PARAM;
		out.hItem = tvhitinfo.hItem;

		TreeView_GetItem(m_hwndTreeView, &out);

		if (out.lParam != ~0) {

			return out.lParam;
		}
	}
	return ~0;
}


void CFindReleaseTree::hit_test_release_proceed() {

	TVITEM tvi = { 0 };
	int lparam;
	POINT p;
	GetCursorPos(&p);

	if ((lparam = test_getatcursor(p, tvi, true)) != ~0) {

		mounted_param myparam(lparam);

		if (!myparam.is_master()) {

			pfc::string8 release_id = get_param_id(myparam);
            // A stale tree node is not a valid release to open.
            if (release_id.get_length())
                m_dlg->trigger_release(release_id);
		}
	}
}

// default action for release tree items

void CFindReleaseTree::vkreturn_test_master_expand_release() {

	TVITEM tvitem = { 0 };
	size_t lparam = test_getselected(tvitem);

	if (lparam != ~0) {

		mounted_param myparam(lparam);

		if (!myparam.is_master()) {

			pfc::string8 release_id = get_param_id(myparam);
            // A stale tree node is not a valid release to open.
            if (release_id.get_length())
                m_dlg->trigger_release(release_id);
		}
		else {

			HTREEITEM hsel = TreeView_GetSelection(m_hwndTreeView);
			TVITEM tvi = {};
			tvi.hItem = hsel;
			tvi.mask = TVIF_STATE;
			TreeView_GetItem(m_hwndTreeView, &tvi);
			if ((tvi.state & TVIS_EXPANDED) == TVIS_EXPANDED) {
				TreeView_Expand(m_hwndTreeView, hsel, TVE_COLLAPSE);
			}
			else {
				TreeView_Expand(m_hwndTreeView, hsel, TVE_EXPAND); //TVE_TOGGLE
			}

		}
	}
	return;
}

// double click on release proceeds to match tracks

LRESULT CFindReleaseTree::OnReleaseTreeDoubleClickRelease(int, LPNMHDR hdr, BOOL& bHandled) {
	if (!hdr || hdr->hwndFrom != m_hwndTreeView || !m_dispinfo_enabled) return 0;

	hit_test_release_proceed();
	return 0;
}

// context menu

LRESULT CFindReleaseTree::OnContextMenu(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled) {
	bHandled = FALSE; 

	HWND hwndCtrl = (HWND)wParam;

	if (hwndCtrl != m_hwndTreeView) return FALSE;

	CPoint screen_position;
	bool vkapps = lParam == -1;

	if (vkapps)
	{
		::GetCursorPos(&screen_position);
	}
	else {
		screen_position = CPoint(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
	}

	TVITEM tviSelection = { 0 };
	TVITEM tviCursor = { 0 };

	size_t param_selection = test_getselected(tviSelection);
	size_t param_cursor = test_getatcursor(screen_position, tviCursor);
	if (param_selection  == ~0) {
		//second bug: no prev selection (right click as first tree event)
		if (vkapps && param_cursor == ~0) {

			// forward to context menu (empty selection)

			CRect rc;
			::GetWindowRect(hwndCtrl, &rc);
			screen_position = rc.CenterPoint();
			context_menu(~0, screen_position);
			return FALSE;
		}
		else
		{
			// needs selection
			m_post_selection_param = param_cursor;
		}
	}
	size_t the_param = ~0;

	if (vkapps) {
		the_param = param_selection;
	}
	else {

		//first bug: right click on unselected node

		if (param_cursor != ~0 && param_cursor != param_selection) {
			the_param = param_cursor;
			m_post_selection_param = param_cursor;
			TreeView_SelectItem(m_hwndTreeView, tviCursor.hItem);
		}
		else {
			the_param = param_selection;
		}
	}

	context_menu(the_param, screen_position);

	bHandled = TRUE;
	return FALSE;
}

void CFindReleaseTree::context_menu(size_t param_mr, POINT screen_pos) {

    mounted_param myparam(param_mr);
    // Popup menus are modal: an artist refresh or another UI action may
    // evict the currently displayed tree nodes while this menu is open.
    // Capture a checked, shared ownership snapshot before offering any
    // release-specific command, then use only these snapshot values.
    Artist_ptr artist_snapshot = m_find_release_artist;
    MasterRelease_ptr master_snapshot;
    Release_ptr release_snapshot;
    if (artist_snapshot && (myparam.is_master() || myparam.is_release()) &&
        myparam.master_ndx < artist_snapshot->master_releases.get_count()) {
        master_snapshot = artist_snapshot->master_releases[myparam.master_ndx];
    }
    if (master_snapshot && myparam.is_release() &&
        myparam.release_ndx < master_snapshot->sub_releases.get_count()) {
        release_snapshot = master_snapshot->sub_releases[myparam.release_ndx];
    } else if (artist_snapshot && myparam.is_nmrelease() &&
               myparam.release_ndx < artist_snapshot->releases.get_count()) {
        release_snapshot = artist_snapshot->releases[myparam.release_ndx];
    }
    const bool empty_sel = !(myparam.is_master() && master_snapshot) &&
                           !release_snapshot;
    pfc::string8 context_id;
    pfc::string8 context_title;
    if (!empty_sel) {
        if (myparam.is_master()) {
            context_id = master_snapshot->id;
            context_title = master_snapshot->title;
        } else {
            context_id = release_snapshot->id;
            context_title = release_snapshot->title;
        }
    }
    const pfc::string8 artist_id = artist_snapshot ? artist_snapshot->id : pfc::string8();


	pfc::string8 sourcepage = myparam.brelease ? "Web &release page" : "Web mas&ter release page";
	pfc::string8 copytitle = "Copy t&itle";
	pfc::string8 copyrow = "&Copy";
	pfc::string8 delReleaseCache = "&Delete release cache";
	pfc::string8 filterversions = "&filter loaded versions";
	pfc::string8 clearfilter = "C&lear filter";
	pfc::string8 mainrole = "&main role";
	try {

		enum { ID_CMD_NEXT = 1, ID_VIEW_PAGE, ID_CLP_COPY_TITLE, ID_CLP_COPY_ROW, ID_DLG_CLEAR_REL_CACHE, ID_DLG_CLEAR_FILTER, ID_DLG_FILTER_TOGGLE,  ID_DLG_MAIN_ROLE_TOGGLE };
		HMENU menu = CreatePopupMenu();

		foo_conf cfg = m_dlg->config();

		bool enabled_versions = cfg.find_release_filter_flag & FilterFlag::Versions;
		bool rolemainAT = m_dlg->m_query_mode & SearchMode::AT || m_dlg->m_query_mode & SearchMode::VA;
		bool enabled_rolemain = !rolemainAT && (cfg.find_release_filter_flag & FilterFlag::RoleMain);
		bool enabled_filter = uGetDlgItemText(m_dlg->m_hWnd, IDC_EDIT_FILTER).get_length();

        const pfc::string8 release_id = !empty_sel && !myparam.is_master()
            ? context_id : pfc::string8();

		bool boffExists = false;
		if (release_id.get_length()) {
			pfc::string8 parent_path = ol::get_offline_path(artist_id, ol::GetFrom::Release, release_id, true);
			std::filesystem::path os_path = std::filesystem::u8path(parent_path.c_str());
			boffExists = fs::exists(os_path);
		}

		uAppendMenu(menu, MF_STRING, ID_CMD_NEXT, "&Next");
		uAppendMenu(menu, MF_SEPARATOR, 0, 0);

		if (!empty_sel) {

			uAppendMenu(menu, MF_STRING, ID_CLP_COPY_ROW, copyrow);
			uAppendMenu(menu, MF_STRING, ID_CLP_COPY_TITLE, copytitle);
			uAppendMenu(menu, MF_SEPARATOR, 0, 0);
		}

		uAppendMenu(menu, MF_STRING | (enabled_versions ? MF_CHECKED: 0), ID_DLG_FILTER_TOGGLE, filterversions);
		uAppendMenu(menu, MF_STRING | (rolemainAT ? MF_DISABLED | MF_GRAYED : 0) |(enabled_rolemain ?  MF_CHECKED : 0), ID_DLG_MAIN_ROLE_TOGGLE, mainrole);
		uAppendMenu(menu, MF_SEPARATOR, 0, 0);

		if (boffExists && !empty_sel && release_id.get_length()) {

			uAppendMenu(menu, MF_STRING, ID_DLG_CLEAR_REL_CACHE, delReleaseCache);
			uAppendMenu(menu, MF_SEPARATOR, 0, 0);
		}
		uAppendMenu(menu, MF_STRING | (enabled_filter ? 0 : MF_DISABLED | MF_GRAYED), ID_DLG_CLEAR_FILTER, clearfilter);

		if (!empty_sel) {

			uAppendMenu(menu, MF_SEPARATOR, 0, 0);
			uAppendMenu(menu, MF_STRING, ID_VIEW_PAGE, sourcepage);
		}

		int cmd = TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD,
			screen_pos.x, screen_pos.y, 0, core_api::get_main_window(), 0);
		DestroyMenu(menu);

		switch (cmd)
		{
		case ID_CMD_NEXT: {
			CFindReleaseDialog* dlg = (CFindReleaseDialog*)m_dlg;
			BOOL bDummy;
			dlg->OnButtonNext(0, 0, NULL, bDummy);
			break;
		}
        case ID_VIEW_PAGE: {
            if (empty_sel || !context_id.get_length()) break;
            pfc::string8 url;
            url << (myparam.is_master() ? "https://www.discogs.com/master/" :
                                          "https://www.discogs.com/release/");
            url << context_id;
            display_url(url);
            break;
        }
        case ID_CLP_COPY_TITLE:
        {
            if (empty_sel) break;
            pfc::string8 buffer = context_title;
            if (buffer.get_length()) {

				ClipboardHelper::OpenScope scope;
				scope.Open(core_api::get_main_window());
				ClipboardHelper::SetString(trim(buffer).get_ptr());
				scope.Close();
			}
			break;
		}
		case ID_CLP_COPY_ROW:
		{
			pfc::string8 buffer;
			row_col_data rcd_out;
            if (empty_sel ||
                !m_rt_cache.get_cached_find_release_node(myparam.lparam(), buffer, rcd_out))
                break;
			ClipboardHelper::OpenScope scope;
			scope.Open(core_api::get_main_window());
			ClipboardHelper::SetString(trim(buffer).get_ptr());
			scope.Close();

			break;
		}
		case ID_DLG_CLEAR_FILTER:
		{
			uSetDlgItemText(m_dlg->m_hWnd, IDC_EDIT_FILTER, "");
			break;
		}
		case ID_DLG_CLEAR_REL_CACHE:
		{
			if (!empty_sel && artist_id.get_length() && release_id.get_length())
                discogs_interface->delete_artist_cache(artist_id, release_id);
			break;
		}
		case ID_DLG_FILTER_TOGGLE:
		{
			bool checked = IsDlgButtonChecked(m_dlg->m_hWnd, IDC_CHK_FIND_RELEASE_FILTER_VERS) == BST_CHECKED;

			uButton_SetCheck(m_dlg->m_hWnd, IDC_CHK_FIND_RELEASE_FILTER_VERS, !checked);
			::SendMessage(m_dlg->m_hWnd, WM_COMMAND, MAKEWPARAM(IDC_CHK_FIND_RELEASE_FILTER_VERS, BN_CLICKED), 0);

			break;
		}
		case ID_DLG_MAIN_ROLE_TOGGLE:
		{

			bool checked = IsDlgButtonChecked(m_dlg->m_hWnd, IDC_CHK_FIND_RELEASE_FILTER_ROLEMAIN) == BST_CHECKED;

			uButton_SetCheck(m_dlg->m_hWnd, IDC_CHK_FIND_RELEASE_FILTER_ROLEMAIN, !checked);
			::SendMessage(m_dlg->m_hWnd, WM_COMMAND, MAKEWPARAM(IDC_CHK_FIND_RELEASE_FILTER_ROLEMAIN, BN_CLICKED), 0);

			break;
		}
		}
	}
	catch (...) {}

}

//public serves find-rel dlg

const Artist_ptr CFindReleaseTree::Get_Artist() { return get_artist(); }

size_t CFindReleaseTree::Get_Artist_List_Position() {
	size_t res;
	res = m_find_release_artist ? m_find_release_artist->search_role_list_pos : pfc_infinite;
	return res;
}

size_t CFindReleaseTree::Get_Size() {
	size_t res;
	res = m_rt_cache.bulk_Size();
	return res;
}

void CFindReleaseTree::OnInitExpand(int lparam) {

	HTREEITEM w = TreeView_GetFirstVisible(m_hwndTreeView);
	while (w) {
		TVITEM tvitem = { 0 };
		tvitem.mask = TVIF_PARAM | TVIS_EXPANDED;
		tvitem.hItem = w;
		TreeView_GetItem(m_hwndTreeView, &tvitem);

		if (tvitem.lParam == lparam) {
			TreeView_Expand(m_hwndTreeView, tvitem.hItem, TVM_EXPAND);
			break;
		}

		w = TreeView_GetNextSibling(m_hwndTreeView, w);
	}
}
