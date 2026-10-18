#include "stdafx.h"

#include "discogs.h"
#include "utils_db.h"
#include "querydefmap.h"

//to get track selection
#include "find_release_dialog.h"

#include "history_oplog.h"

bool history_oplog::init(bool enabled, size_t max_items) {

	if (m_enabled == enabled && m_max_items == max_items) return false;

	m_enabled = enabled;
	m_max_items = max_items;

	zap_vhistory();

	sqldb db;

	std::map<oplog_type, vppair*> out_history = {
		{oplog_type::release, &m_vres_release_history},
		{oplog_type::artist, &m_vres_artist_history},
		{oplog_type::query, &m_vres_search_query_history},
		{oplog_type::filter, &m_vres_filter_history}
	};

	size_t inc = db.recharge_history(kcmdHistoryWashup, max_items, out_history);

	return inc != pfc_infinite;
}

//serves config dialog
void history_oplog::zap_vhistory() {

	m_vres_release_history.clear();
	m_vres_artist_history.clear();
	m_vres_search_query_history.clear();
	m_vres_filter_history.clear();
}

bool history_oplog::add_history_row(oplog_type optype, rppair row) {

	if (!get_row_key(optype, row).get_length()) return false;

	vppair& vh = get_history(optype);
	vppair::iterator fit;

	fit = std::find_if(vh.begin(), vh.end(),
		[=](const rppair& w) {
			return get_row_key(optype, w).equals(get_row_key(optype, row));
		});

	if (!vh.size() || fit == vh.end()) {

		vh.insert(vh.begin(), row);
		return true;
	}
	else {
		std::iter_swap(vh.begin(), fit);
	}
	return false;
}

enum {
	MENU_RESTORE_CURRENT = 1,
	MENU_SUB_A = 100, MENU_SUB_B = 200,
	MENU_SUB_C = 300, MENU_SUB_D = 400
};

const char* const qry_tmpl[] = {
	"\tartist=   && title= " ,
	"\tartist=   && track= ",
	"\tartist=   && credit= ",
	"\tartist=   && format= ",
	"\tartist=   && year= ",
	"\tartist=   && title=   && format= ",
	"\ttitle=   && track= ",
	"\ttitle=   && credit= ",
	"\ttitle=   && format= ",
	"\ttitle=   && year= ",
	"\ttitle=   && catno= ",
	"\ttitle=   && track=   && format= ",
	"\tartist= Various && title= ",
	"\tartist= Various && title=   && format= ",
	"\tartist= Various && track=   && credit = "
};

bool gen_query_submenus(HMENU hParentMenu, const std::string expr, const std::string input_text) {

	pfc::string8 in_text;
	pfc::string8 frm_album_artist;
	pfc::string8 frm_artist;
	pfc::string8 frm_album;
	pfc::string8 frm_track_title;
	pfc::string8 frm_sani_artist;

	std::vector<pfc::string8> vsplit_title;
	split(input_text.c_str(), "�", 0, vsplit_title);

	if (vsplit_title.size() >= 5) { //todo: split last token empty: returns n-1!
		in_text = vsplit_title[0];
		frm_album_artist = vsplit_title[1];
		frm_artist = vsplit_title[2];
		frm_album = vsplit_title[3];
		frm_track_title = vsplit_title[4];
		//todo
		if (vsplit_title.size() > 5) {
			frm_sani_artist = vsplit_title[5];
		}
	}

	auto cf = kv_query_fields_ordered.size() - 1; //type=

	//remove trail pop_back

	bool bquery = expr.size();
	QueryDefMap qdm_search_query = {};

	for (const std::string fld : kv_query_fields_ordered) {
		qdm_search_query[fld] = {};
	}

	if (expr.size()) {
		bquery = search_query::TextToMap(expr.c_str(), qdm_search_query);
	}

	size_t csubmenus = 3;
	wchar_t submenu_add[] = L"Add search field";
	wchar_t submenu_templates[] = L"Search templates";
	wchar_t submenu_suggestions[] = L"Suggestions";
	LPWSTR submenus_data[3] = {
		submenu_add, submenu_templates, submenu_suggestions,
	};
	UINT submenus_ids[3] = {
		MENU_SUB_A, MENU_SUB_B,
		MENU_SUB_C
	};
	pfc::array_t<HMENU> submenus; submenus.resize(csubmenus);
	pfc::array_t<MENUITEMINFO> submenu_infos; submenu_infos.resize(csubmenus);
	for (size_t n = 0; n < csubmenus; n++) {
		submenus[n] = CreatePopupMenu();
		submenu_infos[n] = { 0 };
		submenu_infos[n].cbSize = sizeof(MENUITEMINFO);
		submenu_infos[n].fMask = MIIM_SUBMENU | MIIM_STRING | MIIM_ID;
		submenu_infos[n].hSubMenu = submenus[n];
		submenu_infos[n].dwTypeData = submenus_data[n];
		submenu_infos[n].wID = submenus_ids[n];
	}

	//A BASIC

	size_t item = 0;
	for (size_t n = 1; n < cf; n++) {

		std::string str = kv_query_fields_ordered[n];
		//remove trailing =
		str.pop_back();

		bool benable = false;
		QueryDefMap qdm_edit_search_query = {};
		for (const std::string fld : kv_query_fields_ordered) {
			//query definition map
			qdm_edit_search_query[fld] = {};
		}
		if (input_text.size()) {
			benable = search_query::TextToMap(input_text.c_str(), qdm_edit_search_query);
			if (benable) { benable = qdm_edit_search_query.at(kv_query_fields_ordered[n]).first.size(); }
		}
		uAppendMenu(submenus[item], MF_STRING | (!benable ? 0 : MF_DISABLED | MF_GRAYED),
			submenus_ids[item] + n, str.c_str());
	}
	InsertMenuItem(hParentMenu, submenus_ids[item], true, &submenu_infos[item]);

	AppendMenu(hParentMenu, MF_STRING, MF_SEPARATOR, 0);

	//B TEMPLATES

	++item;
	auto szqry_tmpl = sizeof(qry_tmpl)/sizeof(qry_tmpl[0]);
	for (size_t n = 0; n < szqry_tmpl; n++) {

		std::string str = kv_query_fields_ordered[n];
		str.pop_back();

		bool benable = true;
		uAppendMenu(submenus[item], MF_STRING | (benable ? 0 : MF_DISABLED | MF_GRAYED),
			submenus_ids[item] + n, qry_tmpl[n]);
	}
	InsertMenuItem(hParentMenu, submenus_ids[item], true, &submenu_infos[item]);

	//C SUGGESTIONS

	++item;

	bool benable = false;

	pfc::string8 lbl(in_text);
	size_t ndx = 1;

	// 1 artist & title
	// 2 artist & track
	// 3 title & track
	// 4 title & credit
	// 5 q & title

	pfc::string8 frm_custom_tf;
	bool bres = g_discogs->find_release_dialog->GetCustomQueryTF(frm_custom_tf);

	//todo: shortcuts with another parent wnd

	//Ctrl 1 to 5
	for (size_t ndx = 1; ndx <= search_query::k_nested_suggestions; ndx++) {
		bool done = search_query::GetSuggestion(frm_album_artist, frm_artist, frm_album, frm_track_title, frm_sani_artist, frm_custom_tf, ndx, lbl);
		if (done) {
			benable = stricmp_utf8(in_text, lbl);
			lbl = EscapeWin(lbl); lbl << "\tCtrl+" << ndx;
			uAppendMenu(submenus[item], MF_STRING | (benable ? 0 : MF_DISABLED | MF_GRAYED), submenus_ids[item] + GetMenuItemCount(submenus[item]), lbl);
		}
	}

	lbl = frm_album_artist.get_length() ? frm_album_artist : frm_artist.get_length()? frm_artist : "";
	if (lbl.get_length()) {
		if (GetMenuItemCount(submenus[item])) {
			AppendMenu(submenus[item], MF_STRING, MF_SEPARATOR, 0);
		}
		benable = stricmp_utf8(in_text, lbl);
		lbl = EscapeWin(lbl); lbl << "\tCtrl+9";
		uAppendMenu(submenus[item], MF_STRING | (benable ? 0 : MF_DISABLED | MF_GRAYED), submenus_ids[item] + GetMenuItemCount(submenus[item]), lbl);
	}

	lbl = frm_custom_tf;
	if (lbl.get_length()) {
		if (GetMenuItemCount(submenus[item])) {
			AppendMenu(submenus[item], MF_STRING, MF_SEPARATOR, 0);
		}
		benable = stricmp_utf8(in_text, lbl);
		lbl = EscapeWin(lbl); lbl << "\tCtrl+0";
		uAppendMenu(submenus[item], MF_STRING | (benable ? 0 : MF_DISABLED | MF_GRAYED), submenus_ids[item] + GetMenuItemCount(submenus[item]), lbl);
	}

	InsertMenuItem(hParentMenu, submenus_ids[item], true, &submenu_infos[item]);

	return true;
}

bool history_oplog::do_history_menu(oplog_type optype, HWND hwndCtrl/*, HMENU hMenu*/, pfc::string8& out, pfc::string8 params) {

	vppair& vophistory = get_history(optype);

	if (!vophistory.size() && optype != oplog_type::query) {
		//..
		return false;
		//..
	}

	CRect clientRect;
	::GetWindowRect(hwndCtrl, &clientRect);
	POINT pt = {};
	pt.x = clientRect.right;
	pt.y = clientRect.bottom;

	enum { SUBMENU_FIELDS = 1, SUBMENU_TMPL_QUERIES = 2, SUBMENU_TMPL_SUGG = 3, MENU_FIRST_ITEM = 4, MENU_WEB = 1000 };

	HMENU hMenu = CreatePopupMenu();

	//build-in SEARCH FIELDS and Templates submenu

	if (optype == oplog_type::query) {

		QueryDefMap qdm_search_query = {};
		bool bquery = search_query::TextToMap(out, qdm_search_query);

		gen_query_submenus(hMenu, out.c_str(), params.c_str());

		AppendMenu(hMenu, MF_STRING, MF_SEPARATOR, 0);
	}

	if (CONF.history_enabled()) {

		//gen query history menu items

		for (auto walk_it = vophistory.begin(); walk_it != vophistory.end(); ++walk_it) {

			const pfc::string8 label = get_menu_label(optype, walk_it._Ptr);
			const pfc::stringcvt::string_os_from_utf8 os_str(label);
			size_t item_id = MENU_FIRST_ITEM + std::distance(vophistory.begin(), walk_it);
			AppendMenu(hMenu, MF_STRING, item_id, os_str);
		}
	}

	if (optype == oplog_type::query) {

		//separator and web advanced search URLs

		if (CONF.history_enabled() && vophistory.size()) {
			AppendMenu(hMenu, MF_STRING, MF_SEPARATOR, 0);
		}

		bool bquery = out.get_length();

		AppendMenu(hMenu, MF_STRING, MENU_WEB, L"\tWeb Discogs advanced search");
		AppendMenu(hMenu, MF_STRING | (bquery ? 0 : MF_DISABLED | MF_GRAYED), MENU_WEB + 1, L"\tWeb Discogs current search");
	}

	int cmd = TrackPopupMenu(hMenu, TPM_RIGHTALIGN | TPM_TOPALIGN | TPM_RETURNCMD, pt.x, pt.y, 0, hwndCtrl, NULL);

	TCHAR buffer[MAX_PATH];
	if (optype == oplog_type::query) {
		::GetMenuString(hMenu, cmd, buffer, MAX_PATH, 0);
	}

	DestroyMenu(hMenu);

	//return value

	if (cmd) {

		if (cmd == MENU_RESTORE_CURRENT) {

			return out.get_length();
		}
		else {
			if (cmd >= MENU_SUB_A && cmd < MENU_SUB_B) {
				//fields
				out = kv_query_fields_ordered[cmd - MENU_SUB_A].c_str();
				return out.get_length();;
			}
			else if (cmd >= MENU_SUB_B && cmd < MENU_SUB_C) {
				//templates
				out = qry_tmpl[cmd - MENU_SUB_B];
				out.replace_string("&&", "&");
				out.remove_chars(0, 1);

				return out.get_length();;
			}
			else if (cmd >= MENU_SUB_C && cmd < MENU_SUB_D) {
				//suggestions
				const pfc::stringcvt::string_utf8_from_wide cvt(buffer);
				out = cvt;
				auto shortcut_pos = out.find_first('\t');
				if (shortcut_pos != SIZE_MAX) {
					out = out.subString(0, shortcut_pos);
				}
				out.replace_string("&&", "&");
				return out.get_length();;
			}
			else if (cmd == MENU_WEB) {
				out = "Web Discogs advanced search";
				return out.get_length();;
			}
			else if (cmd == MENU_WEB + 1) {
				out = "Web Discogs current search";
				return out.get_length();;
			}
			else {
				if (cmd - MENU_FIRST_ITEM < vophistory.size()) {
					rppair row_h = vophistory.at(cmd - MENU_FIRST_ITEM);
					out = get_row_val(optype, row_h);
				}
			}
		}
		return out.get_length();
	}
	return false;
}
