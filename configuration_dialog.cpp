#include "stdafx.h"

#include "tag_mappings_dialog.h"
#include "discogs_interface.h"
#include "utils.h"
#include "tasks.h"
#include "utils_db.h"
#include "yesno_dialog.h"

#include "configuration_dialog.h"

static HWND g_hWndTabDialog[NUM_TABS] = {nullptr};
static HWND g_hWndCurrentTab = nullptr;
static t_uint32 g_current_tab;

bool my_threaded_process::run_modal(service_ptr_t<threaded_process_callback> p_callback, unsigned p_flags, HWND p_parent, const char* p_title, t_size p_title_len = ~0) {
	bool bres = false;
	g_run_modal(p_callback, p_flags, p_parent, p_title, p_title_len);
	return bres;
}
bool my_threaded_process::run_modeless(service_ptr_t<threaded_process_callback> p_callback, unsigned p_flags, HWND p_parent, const char* p_title, t_size p_title_len = ~0) {
	bool bres = false;
	g_run_modeless(p_callback, p_flags, p_parent, p_title, p_title_len);
	return bres;
}

//! Decrements reference count; deletes the object if reference count reaches zero. This is normally not called directly but managed by service_ptr_t<> template. \n
//! Implemented by service_impl_* classes.
//! @returns New reference count. For debug purposes only, in certain conditions return values may be unreliable.
int my_threaded_process::service_release() throw() {
	int ires = 0;
	return ires;
};
//! Increments reference count. This is normally not called directly but managed by service_ptr_t<> template. \n
//! Implemented by service_impl_* classes.
//! @returns New reference count. For debug purposes only, in certain conditions return values may be unreliable.
int my_threaded_process::service_add_ref() throw() {
	int ires = 0;
	return ires;
};
//! Queries whether the object supports specific interface and retrieves a pointer to that interface. This is normally not called directly but managed by service_query_t<> function template. \n
//! Checks the parameter against GUIDs of interfaces supported by this object, if the GUID is one of supported interfaces, p_out is set to service_base pointer that can be static_cast<>'ed to queried interface and the method returns true; otherwise the method returns false. \n
//! Implemented by service_impl_* classes. \n
//! Note that service_query() implementation semantics (but not usage semantics) changed in SDK for foobar2000 1.4; they used to be auto-implemented by each service interface (via FB2K_MAKE_SERVICE_INTERFACE macro); they're now implemented in service_impl_* instead. See SDK readme for more details. \n
bool my_threaded_process::service_query(service_ptr& p_out, const GUID& p_guid) {
	bool bres = 0;
	return bres;
};

void CConfigurationDialog::InitTabs() {
	tab_table.append_single(tab_entry("Searching", searching_dialog_proc, IDD_DIALOG_CONF_FIND_RELEASE));
	tab_table.append_single(tab_entry("Searching Adv.", searching_adv_dialog_proc, IDD_DIALOG_CONF_FIND_ADV_RELEASE));
	tab_table.append_single(tab_entry("Matching", matching_dialog_proc, IDD_DIALOG_CONF_MATCHING));
	tab_table.append_single(tab_entry("Tagging", tagging_dialog_proc, IDD_DIALOG_CONF_TAGGING));
	tab_table.append_single(tab_entry("Cache", caching_dialog_proc, IDD_DIALOG_CONF_CACHING));
	tab_table.append_single(tab_entry("Artwork", art_dialog_proc, IDD_DIALOG_CONF_ART));
	tab_table.append_single(tab_entry("UI Options", ui_dialog_proc, IDD_DIALOG_CONF_UI));
	tab_table.append_single(tab_entry("OAuth", oauth_dialog_proc, IDD_DIALOG_CONF_OAUTH));
}

CConfigurationDialog::~CConfigurationDialog() {

	if (g_discogs) {
		g_discogs->configuration_dialog = nullptr;

	}
}

//from libPPUI\CDialogResizeHelper.cpp
static BOOL GetChildWindowRect(HWND wnd, UINT id, RECT* child)
{
	RECT temp;
	HWND wndChild = GetDlgItem(wnd, id);
	if (wndChild == NULL) return FALSE;
	if (!GetWindowRect(wndChild, &temp)) return FALSE;
	if (!MapWindowPoints(0, wnd, (POINT*)&temp, 2)) return FALSE;
	*child = temp;
	return TRUE;
}

LRESULT CConfigurationDialog::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {

	if (!g_discogs) {
		//installation is not reliable
		return FALSE;
	}

	InitTabs();

	HWND hWndTab = uGetDlgItem(IDC_TAB_CFG);

	//darkmode
	m_dark.AddDialog(m_hWnd);
	m_dark.AddTabCtrl(hWndTab);

	// set up tabs and create (not visible) subdialogs
	uTCITEM item = {0};
	item.mask = TCIF_TEXT;
	for (size_t n = 0; n < NUM_TABS; n++) {
		PFC_ASSERT(tab_table[n].m_pszName != nullptr);

		item.pszText = tab_table[n].m_pszName;
		uTabCtrl_InsertItem(hWndTab, n, &item);

		g_hWndTabDialog[n] = tab_table[n].CreateTabDialog(m_hWnd, (LPARAM)this);

		//darkmode
		m_dark.AddDialog(g_hWndTabDialog[n]);
	}

	// get the size of the inner part of the tab control
	RECT rcTab;
	GetChildWindowRect(m_hWnd, IDC_TAB_CFG, &rcTab);
	uSendMessage(hWndTab, TCM_ADJUSTRECT, FALSE, (LPARAM)&rcTab);

	// tab Control
	RECT rcTabDialog;
	::GetClientRect(g_hWndTabDialog[0], &rcTabDialog);
	OffsetRect(&rcTabDialog, rcTab.left, rcTab.top);
	rcTabDialog.bottom = (rcTabDialog.bottom > rcTab.bottom)
		? rcTabDialog.bottom : rcTab.bottom;
	rcTabDialog.right = (rcTabDialog.right > rcTab.right)
		? rcTabDialog.right : rcTab.right;

	uSendMessage(hWndTab, TCM_ADJUSTRECT, TRUE, (LPARAM)&rcTabDialog);
	::SetWindowPos(hWndTab, nullptr,
		rcTabDialog.left, rcTabDialog.top,
		rcTabDialog.right - rcTabDialog.left, rcTabDialog.bottom - rcTabDialog.top,
		SWP_NOZORDER | SWP_NOACTIVATE);

	// position the subdialogs in the inner part of the tab control
	uSendMessage(hWndTab, TCM_ADJUSTRECT, FALSE, (LPARAM)&rcTabDialog);

	// fix left white stripe
	if (!m_dark.IsDark()) {
		InflateRect(&rcTabDialog, 2, 1);
		OffsetRect(&rcTabDialog, -1, 1);
	}

	for (size_t n = 0; n < tabsize(g_hWndTabDialog); n++) {
		if (g_hWndTabDialog[n] != nullptr) {
			::SetWindowPos(g_hWndTabDialog[n], nullptr,
				rcTabDialog.left, rcTabDialog.top,
				rcTabDialog.right - rcTabDialog.left, rcTabDialog.bottom - rcTabDialog.top,
				SWP_NOZORDER | SWP_NOACTIVATE);
		}
	}

	g_current_tab = conf.last_conf_tab;
	uSendMessage(hWndTab, TCM_SETCURSEL, g_current_tab, 0);

	g_hWndCurrentTab = g_hWndTabDialog[g_current_tab];
	if (g_hWndCurrentTab) {
		::ShowWindow(g_hWndCurrentTab, SW_SHOW);
	}

	return TRUE;
}

LRESULT CConfigurationDialog::OnChangingTab(WORD /*wNotifyCode*/, LPNMHDR /*lParam*/, BOOL& /*bHandled*/) {

	if (get_state() & preferences_state::changed) {

		CYesNoApiDialog yndlg;

		auto res = yndlg.query(m_hWnd, { "Configuration Changes","Apply Changes ?" }, true, false);

		switch (res) {
		case 1:/*yes*/
			pushcfg(false);
			OnChanged();
			break;
		case 2:
			setting_dlg = true;
			init_current_tab();
			conf_edit = conf;
			setting_dlg = false;
			OnChanged();
			break;
		/*default:
			return TRUE;*/
		}
	}
	return FALSE;
}

LRESULT CConfigurationDialog::OnChangeTab(WORD /*wNotifyCode*/, LPNMHDR /*lParam*/, BOOL& /*bHandled*/) {
	if (g_hWndCurrentTab != nullptr) {
		::ShowWindow(g_hWndCurrentTab, SW_HIDE);
	}
	g_hWndCurrentTab = nullptr;

	g_current_tab = (t_uint32)::SendDlgItemMessage(m_hWnd, IDC_TAB_CFG, TCM_GETCURSEL, 0, 0);

	if (g_current_tab < tabsize(g_hWndTabDialog)) {
		g_hWndCurrentTab = g_hWndTabDialog[g_current_tab];
		::ShowWindow(g_hWndCurrentTab, SW_SHOW);
	}
	return FALSE;
}

bool CConfigurationDialog::build_current_cfg(bool reset) {

	bool bres = false;

	if (reset || g_hWndCurrentTab == g_hWndTabDialog[CONF_FIND_RELEASE_TAB]) {
		save_searching_dialog(g_hWndTabDialog[CONF_FIND_RELEASE_TAB], !reset/*bind*/);
	}
	if (reset || g_hWndCurrentTab == g_hWndTabDialog[CONF_FIND_ADV_RELEASE_TAB]) {
		save_searching_adv_dialog(g_hWndTabDialog[CONF_FIND_ADV_RELEASE_TAB], !reset/*bind*/);
	}
	if (reset || g_hWndCurrentTab == g_hWndTabDialog[CONF_MATCHING_TAB]) {
		save_matching_dialog(g_hWndTabDialog[CONF_MATCHING_TAB], !reset);
	}
	if (reset || g_hWndCurrentTab == g_hWndTabDialog[CONF_TAGGING_TAB]) {
		save_tagging_dialog(g_hWndTabDialog[CONF_TAGGING_TAB], !reset/*, true*/);
	}
	if (reset || g_hWndCurrentTab == g_hWndTabDialog[CONF_CACHING_TAB]) {
		save_caching_dialog(g_hWndTabDialog[CONF_CACHING_TAB], !reset);
	}
	if (reset || g_hWndCurrentTab == g_hWndTabDialog[CONF_ART_TAB]) {
		save_art_dialog(g_hWndTabDialog[CONF_ART_TAB], !reset);
	}
	if (reset || g_hWndCurrentTab == g_hWndTabDialog[CONF_UI_TAB]) {
		save_ui_dialog(g_hWndTabDialog[CONF_UI_TAB], !reset);
	}
	if (reset || g_hWndCurrentTab == g_hWndTabDialog[CONF_OATH_TAB]) {
		save_oauth_dialog(g_hWndTabDialog[CONF_OATH_TAB], !reset);
	}

	bres = reset || HasChanged();
	return bres;
}

void CConfigurationDialog::pushcfg(bool reset) {

	if (build_current_cfg(reset)) {
		std::pair<pfc::string8, pfc::string8> poa;
		poa.first = reset ? conf.oauth_token : conf_edit.oauth_token;
		poa.second = reset ? conf.oauth_token_secret : conf_edit.oauth_token_secret;
		discogs_interface->fetcher->update_oauth(poa.first, poa.second);

		if (!reset) conf = conf_edit;

		bool brefresh_tagmap_multivalues = !conf.multivalue_fields.equals(CONF.multivalue_fields);

		conf_edit = foo_conf(conf);
		conf_edit.SetName("CfgEdit");

		bool fonts_changed = conf.custom_font != CONF.custom_font;
		if (fonts_changed) {
			g_hFont = nullptr;
			g_hFontTabs = nullptr;
		}

		CONF.save(CConf::cfgFilter::CONF, conf);
		CONF.load();

		if (brefresh_tagmap_multivalues) {
			update_loaded_tagmaps_multivalues();
		}
		if (fonts_changed) {
			g_discogs->notify(ui_element_notify_font_changed, 0, 0, 0);
		}
	}
}

void CConfigurationDialog::reset() {

	BOOL bDummy;
	OnDefaults(0, 0, NULL, bDummy);
}

LRESULT CConfigurationDialog::OnDefaults(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {

	CYesNoApiDialog yndlg;
	if (!yndlg.query(m_hWnd, { "Reset", "Reset Discogger settings?" })) {
		return FALSE;
	}

	foo_conf temp;
	conf = temp;
	conf_edit = temp;
	setting_dlg = true;
	init_searching_dialog(g_hWndTabDialog[CONF_FIND_RELEASE_TAB], false);
	init_searching_adv_dialog(g_hWndTabDialog[CONF_FIND_ADV_RELEASE_TAB], false);
	init_caching_dialog(g_hWndTabDialog[CONF_CACHING_TAB], false);
	init_matching_dialog(g_hWndTabDialog[CONF_MATCHING_TAB], false);
	init_tagging_dialog(g_hWndTabDialog[CONF_TAGGING_TAB], false);
	init_art_dialog(g_hWndTabDialog[CONF_ART_TAB], false);
	init_ui_dialog(g_hWndTabDialog[CONF_UI_TAB], false);
	init_oauth_dialog(g_hWndTabDialog[CONF_OATH_TAB], false);
	setting_dlg = false;
	pushcfg(true);

	conf = CONF;
	conf_edit = CONF;

	OnChanged();

	on_delete_history(m_hWnd, 0, true);

	return FALSE;
}

LRESULT CConfigurationDialog::OnCustomAnvChanged(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/) {
	conf.replace_ANVs = lParam;
	if (g_hWndTabDialog[CONF_TAGGING_TAB]) {
		uButton_SetCheck(g_hWndTabDialog[CONF_TAGGING_TAB],
			IDC_CHK_ENABLE_ANV, conf.replace_ANVs);
	}
	return FALSE;
}

#ifdef SIM_VA_MA_BETA
LRESULT CConfigurationDialog::OnCustomMsg_VA_as_MA_Reset(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/) {

	if (g_hWndTabDialog[CONF_ART_TAB]) {
		uButton_SetCheck(g_hWndTabDialog[CONF_ART_TAB],
			IDC_CHK_CFG_ART_SIM_VA_AS_MA, false);
	}
	return FALSE;
}
#endif

LRESULT CConfigurationDialog::OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled) {

	if (conf.last_conf_tab != g_current_tab) {
		CONF.save_active_config_tab(g_current_tab);
	}
	return FALSE;
}

void CConfigurationDialog::show_tab(unsigned int itab) {
	if (itab >= 0 && itab < NUM_TABS) {
		if (g_hWndCurrentTab != nullptr) {
			::ShowWindow(g_hWndCurrentTab, SW_HIDE);
		}

		g_current_tab = (t_uint32)::SendDlgItemMessage(m_hWnd, IDC_TAB_CFG, TCM_GETCURSEL, itab, 0);
		g_hWndCurrentTab = g_hWndTabDialog[itab];
		::ShowWindow(g_hWndCurrentTab, SW_SHOW);
	}
}

inline void set_window_text(HWND wnd, int IDC, const pfc::string8 &text) {
	pfc::stringcvt::string_wide_from_ansi wtext(text);
	::SetWindowText(::uGetDlgItem(wnd, IDC), (LPCTSTR)const_cast<wchar_t*>(wtext.get_ptr()));
}

enum {
	rowArtistTitle = 1,
	rowArtistTrack,
	rowTitleTrack,
	rowTitleCredit,
};

void InitComboQueryType(HWND wnd, UINT id, t_size sel) {
	std::map<t_size, std::string>rowstylemap;
	rowstylemap.emplace((t_size)rowArtistTitle, "Artist & Title");
	rowstylemap.emplace((t_size)rowArtistTrack, "Artist & Track");
	rowstylemap.emplace((t_size)rowTitleTrack, "Title & Track");
	rowstylemap.emplace((t_size)rowTitleCredit, "Title & Credit");
	HWND ctrlwnd = uGetDlgItem(wnd, id);

	LRESULT lr = uSendDlgItemMessage(wnd, id, CB_RESETCONTENT, 0, 0);
	size_t pos = 0;
	for (auto& [val, text] : rowstylemap) {
		lr = uSendDlgItemMessageText(wnd, id, CB_INSERTSTRING, /*pos*/-1, text.c_str());
		uSendDlgItemMessage(wnd, id, CB_SETITEMDATA, lr, val);
		if (val == sel) {
			lr = uSendDlgItemMessageText(wnd, id, CB_SETCURSEL, lr, 0);
		}
		++pos;
	}
}

enum {
	rowStyleGrid = 0,
	rowStyleFlat,
	rowStylePlaylist,
	rowStylePlaylistDelimited,
};

void InitComboRowStyle(HWND wnd, UINT id, t_size sel) {
	std::map<t_size, std::string>rowstylemap;
	rowstylemap.emplace((t_size)rowStyleGrid, "Grid");
	rowstylemap.emplace((t_size)rowStyleFlat, "Flat");
	rowstylemap.emplace((t_size)rowStylePlaylist, "Playlist");
	rowstylemap.emplace((t_size)rowStylePlaylistDelimited, "Playlist Grid");
	HWND ctrlwnd = uGetDlgItem(wnd, id);

	LRESULT lr = uSendDlgItemMessage(wnd, id, CB_RESETCONTENT, 0, 0);
	size_t pos = 0;
	for (auto& [val, text] : rowstylemap) {
		lr = uSendDlgItemMessageText(wnd, id, CB_INSERTSTRING, pos, text.c_str());
		uSendDlgItemMessage(wnd, id, CB_SETITEMDATA, lr, val);
		if (val == sel) {
			lr = uSendDlgItemMessageText(wnd, id, CB_SETCURSEL, lr, 0);
		}
		++pos;
	}
}

void CConfigurationDialog::init_searching_dialog(HWND wnd, bool subclass) {

	if (subclass) {
		HWND hwnd_hlp = ::GetDlgItem(wnd, IDC_SYNTAX_HELP_SEARCH);
		subclass_hyper_link_help_syntax(m_help_link_search, hwnd_hlp, m_dark.IsDark());

		HWND wndStaticHeader = ::GetDlgItem(wnd, IDC_GRP_STATIC_CFG_SEARCH_1);

		m_staticPrefHeader_Search1.SubclassWindow(wndStaticHeader);
		m_staticPrefHeader_Search1.PaintHeader();

		wndStaticHeader = ::GetDlgItem(wnd, IDC_GRP_STATIC_CFG_SEARCH_2);

		m_staticPrefHeader_Search2.SubclassWindow(wndStaticHeader);
		m_staticPrefHeader_Search2.PaintHeader();

		wndStaticHeader = ::GetDlgItem(wnd, IDC_GRP_STATIC_CFG_SEARCH_3);

		m_staticPrefHeader_Search3.SubclassWindow(wndStaticHeader);
		m_staticPrefHeader_Search3.PaintHeader();

		//dark mode
		m_dark.AddControls(wnd);
	}
	::ShowWindow(m_help_link_search, SW_SHOW);

	uButton_SetCheck(wnd, IDC_CHK_ENABLE_AUTO_SEARCH, conf.enable_autosearch);
	uButton_SetCheck(wnd, IDC_CHK_AUTO_REL_LOAD_ON_OPEN, conf.auto_rel_load_on_open);
	uButton_SetCheck(wnd, IDC_CHK_AUTO_REL_LOAD_ON_SELECT, conf.auto_rel_load_on_select);

	FlgMng fv_skip;
	conf.GetFlagVar(CFG_SKIP_MNG_FLAG, fv_skip);

	uButton_SetCheck(wnd, IDC_CHK_SKIP_RELEASE_DLG_IDED, fv_skip.GetFlag(SkipMng::RELEASE_DLG_IDED));
	bool skip_va_auto_load = fv_skip.GetFlag(SkipMng::RELEASE_DLG_VA_AUTO_LOAD);
	uButton_SetCheck(wnd, IDC_CHK_SKIP_RELEASE_DLG_VA_AUTO_LOAD, !skip_va_auto_load);

	uSetDlgItemText(wnd, IDC_EDIT_RELEASE_FORMATTING, conf.search_release_format_string);
	uSetDlgItemText(wnd, IDC_EDIT_MASTER_FORMATTING, conf.search_master_format_string);
	uSetDlgItemText(wnd, IDC_EDIT_MASTER_SUB_FORMATTING, conf.search_master_sub_format_string);

	FlgMng fv_init_query;
	conf.GetFlagVar(CFG_ON_INIT_QUERY_FLAGS, fv_init_query);

	bool enabled = false;
	enabled = fv_init_query.GetFlag(InitQueryMng::INIT_QUERY_FLAG_ENABLED);
	uButton_SetCheck(wnd, IDC_CHK_REL_INIT_QUERY_FLAG_ENABLE, enabled);

}

void CConfigurationDialog::init_searching_adv_dialog(HWND wnd, bool subclass) {

	if (subclass) {

		HWND hwnd_hlp = ::GetDlgItem(wnd, IDC_SYNTAX_HELP_SEARCH_ADV);
		subclass_hyper_link_help_syntax(m_help_link_search_adv, hwnd_hlp, m_dark.IsDark());

		HWND wndStaticHeader = ::GetDlgItem(wnd, IDC_GRP_STATIC_CFG_SEARCH_ADV_1);

		m_staticPrefHeader_SearchAdv1.SubclassWindow(wndStaticHeader);
		m_staticPrefHeader_SearchAdv1.PaintHeader();

		wndStaticHeader = ::GetDlgItem(wnd, IDC_GRP_STATIC_CFG_SEARCH_ADV_2);

		m_staticPrefHeader_SearchAdv2.SubclassWindow(wndStaticHeader);
		m_staticPrefHeader_SearchAdv2.PaintHeader();

		wndStaticHeader = ::GetDlgItem(wnd, IDC_GRP_STATIC_CFG_SEARCH_ADV_3);

		m_staticPrefHeader_SearchAdv3.SubclassWindow(wndStaticHeader);
		m_staticPrefHeader_SearchAdv3.PaintHeader();

		//dark mode
		m_dark.AddControls(wnd);
	}

	::ShowWindow(m_help_link_search_adv, SW_SHOW);

	FlgMng fv_init_query;
	conf.GetFlagVar(CFG_ON_INIT_QUERY_FLAGS, fv_init_query);

	bool enabled = fv_init_query.GetFlag(InitQueryMng::INIT_QUERY_PX_LNK_ENABLED);
	uButton_SetCheck(wnd, IDC_CHK_ON_INIT_QUERY_PX_LNK, enabled);

	enabled = fv_init_query.GetFlag(InitQueryMng::INIT_QUERY_ART_AS_CRED_IN_VA);
	uButton_SetCheck(wnd, IDC_CHK_INIT_QUERY_FLAG_ART_AS_CRED_IN_VA, enabled);

	enabled = fv_init_query.GetFlag(InitQueryMng::INIT_QUERY_CUST_TF_ENABLED);
	uButton_SetCheck(wnd, IDC_CHK_ON_INIT_QUERY_FLAG_CUSTOM_TF, enabled);

	uSetDlgItemText(wnd, IDC_EDIT_VARIOUS_PREFIXES, conf.various_prefixes);
	uSetDlgItemText(wnd, IDC_EDIT_MULT_ART_LINKS, conf.multiple_artists_links);
	uSetDlgItemText(wnd, IDC_EDIT_REL_INIT_QUERY_TF, conf.on_init_query_tf);

	pfc::string8 buf = PFC_string_formatter() << LOWORD(conf.query_max);
	uSetDlgItemText(wnd, IDC_EDIT_QUERY_MAX_LOADS, buf);

	buf = PFC_string_formatter() << HIWORD(conf.query_max);
	uSetDlgItemText(wnd, IDC_EDIT_QUERY_MAX_ITEMS, buf);

	InitComboQueryType(wnd, IDC_COMBO_REL_INIT_QUERY_TYPE, conf.on_init_query_def);

	HWND hcmb = ::uGetDlgItem(wnd, IDC_COMBO_REL_INIT_QUERY_TYPE);
	BOOL res = ::SendMessage(hcmb, CB_SETMINVISIBLE, 10, 0L);
}

void CConfigurationDialog::init_matching_dialog(HWND wnd, bool subclass) {

	if (subclass) {

		// help

		HWND hwnd_hlp = ::GetDlgItem(wnd, IDC_SYNTAX_HELP_MATCHING);
		subclass_hyper_link_help_syntax(m_help_link_matching, hwnd_hlp, m_dark.IsDark());

		// headers

		HWND wndStaticHeader = ::GetDlgItem(wnd, IDC_GRP_STATIC_TRK_MATCHING);
		::ShowWindow(m_help_link_matching, SW_SHOW);

		m_staticPrefHeader_Trk_Matching.SubclassWindow(wndStaticHeader);
		m_staticPrefHeader_Trk_Matching.PaintHeader();

		wndStaticHeader = ::GetDlgItem(wnd, IDC_GRP_STATIC_AUTO_TRK_MATCHING);

		m_staticPrefHeader_Trk_Auto_Matching.SubclassWindow(wndStaticHeader);
		m_staticPrefHeader_Trk_Auto_Matching.PaintHeader();

		wndStaticHeader = ::GetDlgItem(wnd, IDC_GRP_STATIC_STATBAR_INFO_TRK_MATCHING);

		m_staticPrefHeader_Trk_SB_Matching.SubclassWindow(wndStaticHeader);
		m_staticPrefHeader_Trk_SB_Matching.PaintHeader();

			//dark mode
		m_dark.AddControls(wnd);
	}

	uButton_SetCheck(wnd, IDC_CHK_MATCH_USING_DURATIONS, conf.match_tracks_using_duration);
	uButton_SetCheck(wnd, IDC_CHK_MATCH_USING_NUMBERS, conf.match_tracks_using_number);
	uButton_SetCheck(wnd, IDC_CHK_MATCH_ASSUME_SORTED, conf.assume_tracks_sorted);
	uButton_SetCheck(wnd, IDC_CHK_SKIP_RELEASE_DLG, conf.skip_mng_flag & SkipMng::RELEASE_DLG_MATCHED);
	uButton_SetCheck(wnd, IDC_CHK_SKIP_BRAINZ_MIBS_FETCH, conf.skip_mng_flag & SkipMng::BRAINZ_ID_FETCH);
	uSetDlgItemText(wnd, IDC_EDIT_DISCOGS_FORMATTING, conf.release_discogs_format_string);
	uSetDlgItemText(wnd, IDC_EDIT_FILE_FORMATTING, conf.release_file_format_string);
	uSetDlgItemText(wnd, IDC_EDIT_STATUS_BAR_FORMATTING, conf.release_status_bar_info_format_string);

}

void CConfigurationDialog::init_tagging_dialog(HWND wnd, bool subclass) {

	if (subclass) {

		HWND wndStaticHeader = ::GetDlgItem(wnd, IDC_GRP_STATIC_CFG_WRITE_TAGS);

		m_staticPrefHeader_Write_Tags.SubclassWindow(wndStaticHeader);
		m_staticPrefHeader_Write_Tags.PaintHeader();

		wndStaticHeader = ::GetDlgItem(wnd, IDC_GRP_STATIC_CFG_ARTIST_NAMES);

		m_staticPrefHeader_Artist_Names.SubclassWindow(wndStaticHeader);
		m_staticPrefHeader_Artist_Names.PaintHeader();

		wndStaticHeader = ::GetDlgItem(wnd, IDC_GRP_STATIC_CFG_MULTIVALUES);

		m_staticPrefHeader_Multi_Values.SubclassWindow(wndStaticHeader);
		m_staticPrefHeader_Multi_Values.PaintHeader();

		//dark mode
		m_dark.AddControls(wnd);
	}

	uButton_SetCheck(wnd, IDC_CHK_ENABLE_ANV, conf.replace_ANVs);
	uButton_SetCheck(wnd, IDC_CHK_MOVE_THE_AT_BEGINNING, conf.move_the_at_beginning);
	uButton_SetCheck(wnd, IDC_CHK_DISCARD_NUMERIC_SUFFIXES, conf.discard_numeric_suffix);

	uButton_SetCheck(wnd, IDC_CHK_SKIP_PREVIEW_DIALOG, conf.skip_mng_flag & SkipMng::PREVIEW_DLG);
	uButton_SetCheck(wnd, IDC_CHK_REMOVE_OTHER_TAGS, conf.remove_other_tags);
	uSetDlgItemText(wnd, IDC_EDIT_REMOVE_EXCLUDING_TAGS, conf.raw_remove_exclude_tags);

	uSetDlgItemText(wnd, IDC_EDIT_CFG_MULTIVALUE_FIELDS, conf.multivalue_fields);
	uButton_SetCheck(wnd, IDC_CHK_CFG_TAGSAVE_AUTO_MULTIV_FLAG, conf.tag_save_flags & TAGSAVE_AUTO_MULTIV_FLAG);

	uButton_SetCheck(wnd, IDC_CHK_CFG_TAGSAVE_LOG_FLAG, conf.tag_save_flags & TAGSAVE_LOG_FLAG);
	uButton_SetCheck(wnd, IDC_CHK_CFG_TAGSAVE_PRV_WRITECLOSE_FLAG, conf.tag_save_flags & TAGSAVE_PREVIEW_STICKY_FLAG);

	HWND hwnd_tag_credits = ::uGetDlgItem(wnd, IDC_BTN_EDIT_CAT_CREDIT);
	::ShowWindow(hwnd_tag_credits, SW_HIDE);

}

void CConfigurationDialog::init_memory_cache_buttons(HWND wnd) {

	pfc::string8 text;
	text << "Releases (" << discogs_interface->release_cache_size() << ")";
	set_window_text(wnd, IDC_BTN_CLEAR_CACHE_RELEASES, text);
	text = "";
	text << "Master Releases (" << discogs_interface->master_release_cache_size() << ")";
	set_window_text(wnd, IDC_BTN_CLEAR_CACHE_MASTERS, text);
	text = "";
	text << "Artists (" << discogs_interface->artist_cache_size() << ")";
	set_window_text(wnd, IDC_BTN_CLEAR_CACHE_ARTISTS, text);
	text = "";
	text << "Collection (" << discogs_interface->collection_cache_size() << ")";
	set_window_text(wnd, IDC_BTN_CLEAR_CACHE_COLLECTION, text);

	::EnableWindow(::uGetDlgItem(wnd, IDC_BTN_CLEAR_CACHE_RELEASES), discogs_interface->release_cache_size() ? TRUE : FALSE);
	::EnableWindow(::uGetDlgItem(wnd, IDC_BTN_CLEAR_CACHE_MASTERS), discogs_interface->master_release_cache_size() ? TRUE : FALSE);
	::EnableWindow(::uGetDlgItem(wnd, IDC_BTN_CLEAR_CACHE_ARTISTS), discogs_interface->artist_cache_size() ? TRUE : FALSE);
	::EnableWindow(::uGetDlgItem(wnd, IDC_BTN_CLEAR_CACHE_COLLECTION), discogs_interface->collection_cache_size() ? TRUE : FALSE);
}

void CConfigurationDialog::init_caching_dialog(HWND wnd, bool subclass) {

	init_memory_cache_buttons(wnd);

	if (subclass) {

		HWND wndStaticHeader = ::GetDlgItem(wnd, IDC_GRP_STATIC_CFG_CACHE_PARSE);

		m_staticPrefHeader_Cache_Parse.SubclassWindow(wndStaticHeader);
		m_staticPrefHeader_Cache_Parse.PaintHeader();

		wndStaticHeader = ::GetDlgItem(wnd, IDC_GRP_STATIC_CFG_CACHE_MEM);

		m_staticPrefHeader_Cache_Mem.SubclassWindow(wndStaticHeader);
		m_staticPrefHeader_Cache_Mem.PaintHeader();

		wndStaticHeader = ::GetDlgItem(wnd, IDC_GRP_STATIC_CFG_CACHE_DISK);

		m_staticPrefHeader_Cache_Disk.SubclassWindow(wndStaticHeader);
		m_staticPrefHeader_Cache_Disk.PaintHeader();

		//dark mode
		m_dark.AddControls(wnd);

	}

#ifdef CACHE_EXPIRATION
#else
	HWND hwndcustFont = ::uGetDlgItem(wnd, IDC_EDIT_CFG_CACHE_EXP_DAYS);
	::EnableWindow(hwndcustFont, FALSE);
	::ShowWindow(hwndcustFont, SW_HIDE);
	hwndcustFont = ::uGetDlgItem(wnd, IDC_CHK_CFG_CACHE_EXP_ENABLED);
	::EnableWindow(hwndcustFont, FALSE);
	::ShowWindow(hwndcustFont, SW_HIDE);
	hwndcustFont = ::uGetDlgItem(wnd, IDC_STATIC_CFG_CACHE_EXP_DAYS);
	::ShowWindow(hwndcustFont, SW_HIDE);
#endif

	//hidden_postfix regular & skip videp

	uButton_SetCheck(wnd, IDC_CHK_HIDDEN_MERGE_TITLES, conf.parse_hidden_merge_titles);
	uButton_SetCheck(wnd, IDC_CHK_HIDDEN_AS_REGULAR, conf.parse_hidden_as_regular);
	uButton_SetCheck(wnd, IDC_CHK_SKIP_VIDEO_TRACKS, conf.skip_video_tracks);

	//offline cache

	FlgMng fv_offline_cache;
	conf.GetFlagVar(CFG_CACHE_OFFLINE_CACHE_FLAG, fv_offline_cache);
	bool bread = fv_offline_cache.GetFlag(ol::CacheFlags::OC_READ);
	bool bwrite = fv_offline_cache.GetFlag(ol::CacheFlags::OC_WRITE);
	bool bmerge_subtracks = fv_offline_cache.GetFlag(ol::CacheFlags::MERGE_SUBTRACKS);
	bool bmerge_tracks_dotted_to_header = fv_offline_cache.GetFlag(ol::CacheFlags::MERGE_TRACKS_DOTTED_TO_HEADER);

	uButton_SetCheck(wnd, IDC_CHK_CFG_CACHE_READ_DSK_CACHE, bread);
	uButton_SetCheck(wnd, IDC_CHK_CFG_CACHE_WRITE_DSK_CACHE, bwrite);
	uButton_SetCheck(wnd, IDC_CHK_SUBTRACKS_MERGE_TITLES_CACHE, bmerge_subtracks);
	uButton_SetCheck(wnd, IDC_CHK_TRACKS_MERGE_DOTTED_TO_HEADER_CACHE, bmerge_tracks_dotted_to_header);

	//memory cache

	original_parsing_merge_titles = conf.parse_hidden_merge_titles;
	original_parsing_merge_subtracks = conf.cache_offline_cache_flag & ol::CacheFlags::MERGE_SUBTRACKS;
	original_parsing_tracks_dotted_to_header = conf.cache_offline_cache_flag & ol::CacheFlags::MERGE_TRACKS_DOTTED_TO_HEADER;

	original_parsing = conf.parse_hidden_as_regular;
	original_skip_video = conf.skip_video_tracks;

	pfc::string8 num;
	num << conf.cache_max_objects;
	uSetDlgItemText(wnd, IDC_EDIT_CACHED_OBJECTS, num);

	num = "";
	LPARAM lp = LPARAM(conf.disk_cache_exp);
	num << conf.expiration_days();
	uSetDlgItemText(wnd, IDC_EDIT_CFG_CACHE_EXP_DAYS, num);
	uButton_SetCheck(wnd, IDC_CHK_CFG_CACHE_EXP_ENABLED, conf.expiration_enabled());

}

void CConfigurationDialog::init_art_dialog(HWND wnd, bool subclass) {

	if (subclass) {

		// help

		HWND hwnd_hlp = ::GetDlgItem(wnd, IDC_SYNTAX_HELP_ARTWORK);
		subclass_hyper_link_help_syntax(m_help_link_artwork, hwnd_hlp, m_dark.IsDark());

		HWND wndStaticHeader = ::GetDlgItem(wnd, IDC_GRP_STATIC_CFG_ART_ALBUM);

		m_staticPrefHeader_Art_Album.SubclassWindow(wndStaticHeader);
		m_staticPrefHeader_Art_Album.PaintHeader();

		wndStaticHeader = ::GetDlgItem(wnd, IDC_GRP_STATIC_CFG_ART_ARTIST);

		m_staticPrefHeader_Art_Artist.SubclassWindow(wndStaticHeader);
		m_staticPrefHeader_Art_Artist.PaintHeader();

			//dark mode
		m_dark.AddControls(wnd);
	}

	uButton_SetCheck(wnd, IDC_CHK_SAVE_ALBUM_ART, conf.save_album_art);
	uButton_SetCheck(wnd, IDC_CHK_ALBUM_ART_FETCH_ALL, conf.album_art_fetch_all);
	uButton_SetCheck(wnd, IDC_CHK_ALBUM_ART_EMBED, conf.embed_album_art);
	uSetDlgItemText(wnd, IDC_EDIT_ALBUM_ART_DIR, conf.album_art_directory_string);
	uSetDlgItemText(wnd, IDC_EDIT_ALBUM_ART_PREFIX, conf.album_art_filename_string);
	uButton_SetCheck(wnd, IDC_CHK_ALBUM_ART_OVR, conf.album_art_overwrite);

	uButton_SetCheck(wnd, IDC_CHK_SAVE_ARTIST_ART, conf.save_artist_art);
	uButton_SetCheck(wnd, IDC_CHK_ARTIST_ART_FETCH_ALL, conf.artist_art_fetch_all);
	//todo: remove not impl
	uSetDlgItemText(wnd, IDC_EDIT_ARTIST_ART_IDS, conf.artist_art_id_format_string);
	uButton_SetCheck(wnd, IDC_CHK_ARTIST_ART_EMBED, conf.embed_artist_art);
	uSetDlgItemText(wnd, IDC_EDIT_ARTIST_ART_DIR, conf.artist_art_directory_string);
	uSetDlgItemText(wnd, IDC_EDIT_ARTIST_ART_PREFIX, conf.artist_art_filename_string);
	uButton_SetCheck(wnd, IDC_CHK_ARTIST_ART_OVERWRITE, conf.artist_art_overwrite);

	HWND wndSimVA = ::uGetDlgItem(wnd, IDC_CHK_CFG_ART_SIM_VA_AS_MA);
#ifdef SIM_VA_MA_BETA
	::ShowWindow(wndSimVA, SW_SHOW);
	bool sim_va_as_ma = conf.find_release_dlg_flags & CFindReleaseDialog::FLG_VA_AS_MA;
	uButton_SetCheck(wnd, IDC_CHK_CFG_ART_SIM_VA_AS_MA, sim_va_as_ma);
#else
	::ShowWindow(wndSimVA,SW_HIDE);
#endif

}

void CConfigurationDialog::init_ui_dialog(HWND wnd, bool subclass) {

	if (subclass) {

		HWND wndStaticHeader = ::GetDlgItem(wnd, IDC_GRP_STATIC_HISTORY);

		m_staticPrefHeader_History.SubclassWindow(wndStaticHeader);
		m_staticPrefHeader_History.PaintHeader();

		wndStaticHeader = ::GetDlgItem(wnd, IDC_GRP_STATIC_CFG_OTHER);

		m_staticPrefHeader_Other.SubclassWindow(wndStaticHeader);
		m_staticPrefHeader_Other.PaintHeader();

		//dark mode
		m_dark.AddControls(wnd);
	}

	//history
	uButton_SetCheck(wnd, IDC_CHK_CFG_UI_HISTORY_ENABLED, HIWORD(conf.history_enabled_max));
	size_t imax = LOWORD(conf.history_enabled_max);
	uSetDlgItemText(wnd, IDC_EDIT_UI_HISTORY_MAX_ITEMS, std::to_string(imax).c_str());
	//enter override
	uButton_SetCheck(wnd, IDC_CHK_RELEASE_ENTER_KEY_OVR, conf.release_enter_key_override);
	//release stats
	FlgMng fv_stats;
	conf.GetFlagVar(CFG_FIND_RELEASE_DIALOG_FLAG, fv_stats);
	bool bstats = fv_stats.GetFlag(CFindReleaseDialog::FLG_SHOW_RELEASE_TREE_STATS);
	uButton_SetCheck(wnd, IDC_CHK_FIND_RELEASE_STATS, bstats);
	//list style
	bool bv2 = core_version_info_v2::get()->test_version(2, 0, 0, 0);
	if (!bv2) {
		CONF.custom_font = 0;
		conf.custom_font = CONF.custom_font;
	}

	const auto cf = HIWORD(conf.custom_font);
	uButton_SetCheck(wnd, IDC_CHK_CFG_CUSTOM_FONT_DEFAULT, false);
	uButton_SetCheck(wnd, IDC_CHK_CFG_CUSTOM_FONT_EXPANDED, false);
	uButton_SetCheck(wnd, IDC_CHK_CFG_CUSTOM_FONT_FB2K, false);

	if (cf & (1 << 0)) {
		uButton_SetCheck(wnd, IDC_CHK_CFG_CUSTOM_FONT_EXPANDED, true);
	}
	else if (cf & (1 << 1)) {
		uButton_SetCheck(wnd, IDC_CHK_CFG_CUSTOM_FONT_FB2K, true);
	}
	else {
		uButton_SetCheck(wnd, IDC_CHK_CFG_CUSTOM_FONT_DEFAULT, true);
	}

	InitComboRowStyle(wnd, IDC_CMB_CONFIG_LIST_STYLE, conf.list_style);
	HWND hcmb = ::uGetDlgItem(wnd, IDC_CMB_CONFIG_LIST_STYLE);

	BOOL res = ::SendMessage(hcmb, CB_SETMINVISIBLE, 10, 0L);

	if (!bv2) {
		HWND hwndcustFont = ::uGetDlgItem(wnd, IDC_CHK_CFG_CUSTOM_FONT_FB2K);
		::EnableWindow(hwndcustFont, FALSE);
		::ShowWindow(hwndcustFont, SW_HIDE);
	}
}

void CConfigurationDialog::init_oauth_dialog(HWND wnd, bool subclass) {

	if (subclass) {

		//dark mode
		m_dark.AddControls(wnd);

		HWND wndStaticHeader = ::GetDlgItem(wnd, IDC_GRP_STATIC_CFG_OATH_STEP_1);

		m_staticPrefHeader_Oath_S1;
		m_staticPrefHeader_Oath_S1.SubclassWindow(wndStaticHeader);
		m_staticPrefHeader_Oath_S1.PaintHeader();

		wndStaticHeader = ::GetDlgItem(wnd, IDC_GRP_STATIC_CFG_OATH_STEP_2);

		m_staticPrefHeader_Oath_S2.SubclassWindow(wndStaticHeader);
		m_staticPrefHeader_Oath_S2.PaintHeader();

		wndStaticHeader = ::GetDlgItem(wnd, IDC_GRP_STATIC_CFG_OATH_STEP_3);

		m_staticPrefHeader_Oath_S3.SubclassWindow(wndStaticHeader);
		m_staticPrefHeader_Oath_S3.PaintHeader();
	}

	m_hwndTokenEdit = ::GetDlgItem(wnd, IDC_EDIT_OAUTH_TOKEN);
	m_hwndSecretEdit = ::GetDlgItem(wnd, IDC_EDIT_OAUTH_SECRET);
	m_hwndOAuthMsg = ::GetDlgItem(wnd, IDC_STATIC_CFG_OAUTH_MSG);

	uSetWindowText(m_hwndTokenEdit, conf.oauth_token);
	uSetWindowText(m_hwndSecretEdit, conf.oauth_token_secret);

	uSetWindowText(m_hwndOAuthMsg, "Click to test if OAuth is working.");

}

void CConfigurationDialog::show_oauth_msg(pfc::string8 msg, bool iserror) {

	if (g_current_tab != CONF_OATH_TAB) {
		HWND hWndTab = uGetDlgItem(IDC_TAB_CFG);
		uSendMessage(hWndTab, TCM_SETCURSEL, CONF_OATH_TAB, 0);
		show_tab(CONF_OATH_TAB);
	}

	uSetDlgItemText(g_hWndCurrentTab, IDC_STATIC_CFG_OAUTH_MSG, msg);
}

void CConfigurationDialog::save_searching_dialog(HWND wnd, bool dlgbind) {

	foo_conf* conf_ptr = dlgbind ? &conf_edit : &conf;

	conf_ptr->enable_autosearch = uButton_GetCheck(wnd, IDC_CHK_ENABLE_AUTO_SEARCH);
	conf_ptr->auto_rel_load_on_open = uButton_GetCheck(wnd, IDC_CHK_AUTO_REL_LOAD_ON_OPEN);
	conf_ptr->auto_rel_load_on_select = uButton_GetCheck(wnd, IDC_CHK_AUTO_REL_LOAD_ON_SELECT);

	FlgMng fv_skip;
	conf_ptr->GetFlagVar(CFG_SKIP_MNG_FLAG, fv_skip);
	fv_skip.SetFlag(wnd, IDC_CHK_SKIP_RELEASE_DLG_IDED, SkipMng::RELEASE_DLG_IDED);
	//3th param auto-load label to skip load
	fv_skip.SetFlag(wnd, IDC_CHK_SKIP_RELEASE_DLG_VA_AUTO_LOAD, SkipMng::RELEASE_DLG_VA_AUTO_LOAD, true);

	pfc::string8 text;
	uGetDlgItemText(wnd, IDC_EDIT_RELEASE_FORMATTING, text);
	conf_ptr->search_release_format_string = text;
	uGetDlgItemText(wnd, IDC_EDIT_MASTER_FORMATTING, text);
	conf_ptr->search_master_format_string = text;
	uGetDlgItemText(wnd, IDC_EDIT_MASTER_SUB_FORMATTING, text);
	conf_ptr->search_master_sub_format_string = text;

	FlgMng fv_init_query;
	conf_ptr->GetFlagVar(CFG_ON_INIT_QUERY_FLAGS, fv_init_query);

	fv_init_query.SetFlag(wnd, IDC_CHK_REL_INIT_QUERY_FLAG_ENABLE, InitQueryMng::INIT_QUERY_FLAG_ENABLED);
}

bool CConfigurationDialog::cfg_searching_has_changed() {

	bool bres = false;
	bool bcmp = false;

	bres |= conf.enable_autosearch != conf_edit.enable_autosearch;
	bres |= conf.auto_rel_load_on_open != conf_edit.auto_rel_load_on_open;
	bres |= conf.auto_rel_load_on_select != conf_edit.auto_rel_load_on_select;

	bres |= conf.skip_mng_flag != conf_edit.skip_mng_flag;

	bres |= conf.release_enter_key_override != conf_edit.release_enter_key_override;

	bcmp = pfc::strcmp_partial(conf.search_release_format_string, conf_edit.search_release_format_string);
	bres |= bcmp;
	bcmp = pfc::strcmp_partial(conf.search_master_format_string, conf_edit.search_master_format_string);
	bres |= bcmp;
	bcmp = pfc::strcmp_partial(conf.search_master_sub_format_string, conf_edit.search_master_sub_format_string);
	bres |= bcmp;

	bres |= conf.on_init_query_flags != conf_edit.on_init_query_flags;
	bres |= conf.on_init_query_def != conf_edit.on_init_query_def;

	return bres;
}

void CConfigurationDialog::save_searching_adv_dialog(HWND wnd, bool dlgbind) {

	foo_conf* conf_ptr = dlgbind ? &conf_edit : &conf;

	FlgMng fv_init_query;
	conf_ptr->GetFlagVar(CFG_ON_INIT_QUERY_FLAGS, fv_init_query);

	fv_init_query.SetFlag(wnd, IDC_CHK_ON_INIT_QUERY_PX_LNK, InitQueryMng::INIT_QUERY_PX_LNK_ENABLED);
	fv_init_query.SetFlag(wnd, IDC_CHK_INIT_QUERY_FLAG_ART_AS_CRED_IN_VA, InitQueryMng::INIT_QUERY_ART_AS_CRED_IN_VA);
	fv_init_query.SetFlag(wnd, IDC_CHK_ON_INIT_QUERY_FLAG_CUSTOM_TF, InitQueryMng::INIT_QUERY_CUST_TF_ENABLED);

	pfc::string8 text;

	uGetDlgItemText(wnd, IDC_EDIT_VARIOUS_PREFIXES, text);
	conf_ptr->various_prefixes = text;
	uGetDlgItemText(wnd, IDC_EDIT_MULT_ART_LINKS, text);
	conf_ptr->multiple_artists_links = text;
	fv_init_query.SetFlag(wnd, IDC_CHK_ON_INIT_QUERY_FLAG_CUSTOM_TF, InitQueryMng::INIT_QUERY_CUST_TF_ENABLED);

	uGetDlgItemText(wnd, IDC_EDIT_REL_INIT_QUERY_TF, text);
	conf_ptr->on_init_query_tf = text;

	int s = ::uSendDlgItemMessage(wnd, IDC_COMBO_REL_INIT_QUERY_TYPE, CB_GETCURSEL, 0, 0);
	conf_ptr->on_init_query_def = ::uSendDlgItemMessage(wnd, IDC_COMBO_REL_INIT_QUERY_TYPE, CB_GETITEMDATA, s, 0);

	size_t max_loads = LOWORD(conf_ptr->query_max);
	size_t max_items = HIWORD(conf_ptr->query_max);

	bool ok_val = false;
	uGetDlgItemText(wnd, IDC_EDIT_QUERY_MAX_LOADS, text);
	if (is_number(text.c_str())) {
		if (ok_val = (atoi(text) >= 5 && atoi(text) <= 20)) {
			max_loads = atoi(text);
			ok_val = true;
		}
	}

	uGetDlgItemText(wnd, IDC_EDIT_QUERY_MAX_ITEMS, text);
	if (is_number(text.c_str())) {
		if (ok_val = (atoi(text) >= 100 && atoi(text) <= 1000)) {
			max_items = atoi(text);
		}
	}

	if (ok_val) {
		conf_ptr->query_max = MAKELPARAM(max_loads, max_items);
	}
}

bool CConfigurationDialog::cfg_searching_adv_has_changed() {

	bool bres = false;
	bool bcmp = false;

	bcmp = !conf.various_prefixes.equals(conf_edit.various_prefixes);
	bres |= bcmp;
	bcmp = !conf.on_init_query_tf.equals(conf_edit.on_init_query_tf);
	bres |= bcmp;
	bcmp = !conf.multiple_artists_links.equals(conf_edit.multiple_artists_links);

	bres |= bcmp;

	bres |= conf.on_init_query_def != conf_edit.on_init_query_def;
	bres |= conf.query_max != conf_edit.query_max;
	bres |= conf.on_init_query_flags != conf_edit.on_init_query_flags;
	return bres;
}

void CConfigurationDialog::save_matching_dialog(HWND wnd, bool dlgbind) {

	foo_conf* conf_ptr = dlgbind ? &conf_edit : &conf;

	conf_ptr->match_tracks_using_duration = uButton_GetCheck(wnd, IDC_CHK_MATCH_USING_DURATIONS);
	conf_ptr->match_tracks_using_number = uButton_GetCheck(wnd, IDC_CHK_MATCH_USING_NUMBERS);
	conf_ptr->assume_tracks_sorted = uButton_GetCheck(wnd, IDC_CHK_MATCH_ASSUME_SORTED);

	pfc::string8 text;
	uGetDlgItemText(wnd, IDC_EDIT_DISCOGS_FORMATTING, text);
	conf_ptr->release_discogs_format_string = text;

	uGetDlgItemText(wnd, IDC_EDIT_FILE_FORMATTING, text);
	conf_ptr->release_file_format_string = text;

	uGetDlgItemText(wnd, IDC_EDIT_STATUS_BAR_FORMATTING, text);
	conf_ptr->release_status_bar_info_format_string = text;

	if (uButton_GetCheck(wnd, IDC_CHK_SKIP_RELEASE_DLG))
		conf_ptr->skip_mng_flag |= SkipMng::RELEASE_DLG_MATCHED;
	else
		conf_ptr->skip_mng_flag &= ~(SkipMng::RELEASE_DLG_MATCHED);

	if (uButton_GetCheck(wnd, IDC_CHK_SKIP_BRAINZ_MIBS_FETCH))
		conf_ptr->skip_mng_flag |= SkipMng::BRAINZ_ID_FETCH;
	else
		conf_ptr->skip_mng_flag &= ~(SkipMng::BRAINZ_ID_FETCH);
}

bool CConfigurationDialog::cfg_matching_has_changed() {

	bool bres = false;
	bool bcmp = false;

	bres |= conf.match_tracks_using_duration != conf_edit.match_tracks_using_duration;
	bres |= conf.match_tracks_using_number != conf_edit.match_tracks_using_number;
	bres |= conf.assume_tracks_sorted != conf_edit.assume_tracks_sorted;
	bres |= conf.skip_mng_flag != conf_edit.skip_mng_flag;

	bcmp = pfc::strcmp_partial(conf.release_discogs_format_string, conf_edit.release_discogs_format_string);
	bres |= bcmp;
	bcmp = pfc::strcmp_partial(conf.release_file_format_string, conf_edit.release_file_format_string);
	bres |= bcmp;
	bcmp = pfc::strcmp_partial(conf.release_status_bar_info_format_string, conf_edit.release_status_bar_info_format_string);
	bres |= bcmp;
	return bres;
}

void CConfigurationDialog::save_tagging_dialog(HWND wnd, bool dlgbind) {

	foo_conf* conf_ptr = dlgbind ? &conf_edit : &conf;

	conf_ptr->replace_ANVs = uButton_GetCheck(wnd, IDC_CHK_ENABLE_ANV);
	conf_ptr->move_the_at_beginning = uButton_GetCheck(wnd, IDC_CHK_MOVE_THE_AT_BEGINNING);
	conf_ptr->discard_numeric_suffix = uButton_GetCheck(wnd, IDC_CHK_DISCARD_NUMERIC_SUFFIXES);

	if (uButton_GetCheck(wnd, IDC_CHK_SKIP_PREVIEW_DIALOG))
		conf_ptr->skip_mng_flag |= SkipMng::PREVIEW_DLG;
	else
		conf_ptr->skip_mng_flag &= ~(SkipMng::PREVIEW_DLG);

	conf_ptr->remove_other_tags = uButton_GetCheck(wnd, IDC_CHK_REMOVE_OTHER_TAGS);
	pfc::string8 text;
	uGetDlgItemText(wnd, IDC_EDIT_REMOVE_EXCLUDING_TAGS, text);
	conf_ptr->set_remove_exclude_tags(text);
	uGetDlgItemText(wnd, IDC_EDIT_CFG_MULTIVALUE_FIELDS, text);
	conf_ptr->multivalue_fields = text;

	//auto-append multivalue
	bool bcheck = uButton_GetCheck(wnd, IDC_CHK_CFG_TAGSAVE_AUTO_MULTIV_FLAG);
	if (bcheck)
		conf_ptr->tag_save_flags |= TAGSAVE_AUTO_MULTIV_FLAG;
	else
		conf_ptr->tag_save_flags &= ~(TAGSAVE_AUTO_MULTIV_FLAG);
	//log tag writes
	bcheck = uButton_GetCheck(wnd, IDC_CHK_CFG_TAGSAVE_LOG_FLAG);
	if (bcheck)
		conf_ptr->tag_save_flags |= TAGSAVE_LOG_FLAG;
	else
		conf_ptr->tag_save_flags &= ~(TAGSAVE_LOG_FLAG);
	//keep preview open after tag writes
	bcheck = uButton_GetCheck(wnd, IDC_CHK_CFG_TAGSAVE_PRV_WRITECLOSE_FLAG);
	if (bcheck)
		conf_ptr->tag_save_flags |= TAGSAVE_PREVIEW_STICKY_FLAG;
	else {
		conf_ptr->tag_save_flags &= ~(TAGSAVE_PREVIEW_STICKY_FLAG);
	}
}

bool CConfigurationDialog::cfg_tagging_has_changed() {

	bool bres = false;
	bool bcmp = false;
	bres |= conf.replace_ANVs != conf_edit.replace_ANVs;
	bres |= conf.move_the_at_beginning != conf_edit.move_the_at_beginning;
	bres |= conf.discard_numeric_suffix != conf_edit.discard_numeric_suffix;
	bres |= conf.skip_mng_flag != conf_edit.skip_mng_flag;
	bres |= conf.remove_other_tags != conf_edit.remove_other_tags;
	bcmp = pfc::strcmp_partial(conf.raw_remove_exclude_tags, conf_edit.raw_remove_exclude_tags);
	bres |= bcmp;
	bcmp = pfc::strcmp_partial(conf.multivalue_fields, conf_edit.multivalue_fields);
	bres |= bcmp;
	bres |= conf.tag_save_flags != conf_edit.tag_save_flags;
	return bres;
}

void CConfigurationDialog::save_caching_dialog(HWND wnd, bool dlgbind) {

	foo_conf* conf_ptr = dlgbind ? &conf_edit : &conf;

	conf_ptr->parse_hidden_merge_titles = uButton_GetCheck(wnd, IDC_CHK_HIDDEN_MERGE_TITLES);
	conf_ptr->parse_hidden_as_regular = uButton_GetCheck(wnd, IDC_CHK_HIDDEN_AS_REGULAR);
	conf_ptr->skip_video_tracks = uButton_GetCheck(wnd, IDC_CHK_SKIP_VIDEO_TRACKS);

	if (uButton_GetCheck(wnd, IDC_CHK_CFG_CACHE_READ_DSK_CACHE))
		conf_ptr->cache_offline_cache_flag |= ol::CacheFlags::OC_READ;
	else
		conf_ptr->cache_offline_cache_flag &= ~(ol::CacheFlags::OC_READ);

	if (uButton_GetCheck(wnd, IDC_CHK_CFG_CACHE_WRITE_DSK_CACHE))
		conf_ptr->cache_offline_cache_flag |= ol::CacheFlags::OC_WRITE;
	else
		conf_ptr->cache_offline_cache_flag &= ~(ol::CacheFlags::OC_WRITE);

	if (uButton_GetCheck(wnd, IDC_CHK_SUBTRACKS_MERGE_TITLES_CACHE))
		conf_ptr->cache_offline_cache_flag |= ol::CacheFlags::MERGE_SUBTRACKS;
	else
		conf_ptr->cache_offline_cache_flag &= ~(ol::CacheFlags::MERGE_SUBTRACKS);

	if (uButton_GetCheck(wnd, IDC_CHK_TRACKS_MERGE_DOTTED_TO_HEADER_CACHE))
		conf_ptr->cache_offline_cache_flag |= ol::CacheFlags::MERGE_TRACKS_DOTTED_TO_HEADER;
	else
		conf_ptr->cache_offline_cache_flag &= ~(ol::CacheFlags::MERGE_TRACKS_DOTTED_TO_HEADER);

	auto expiration_enabled = uButton_GetCheck(wnd, IDC_CHK_CFG_CACHE_EXP_ENABLED);
	conf_ptr->set_expiration_enabled(expiration_enabled);

	if (original_parsing_merge_titles != conf_ptr->parse_hidden_merge_titles ||
		original_parsing_merge_subtracks != (bool)(conf_ptr->cache_offline_cache_flag & ol::CacheFlags::MERGE_SUBTRACKS) ||
		original_parsing_tracks_dotted_to_header != (bool)(conf_ptr->cache_offline_cache_flag & ol::CacheFlags::MERGE_TRACKS_DOTTED_TO_HEADER) ||
		original_parsing != conf_ptr->parse_hidden_as_regular ||
		original_skip_video != conf_ptr->skip_video_tracks) {
		discogs_interface->reset_release_cache();
		HWND wndCacheTab = g_hWndTabDialog[CONF_CACHING_TAB];
		if (wndCacheTab) {
			init_memory_cache_buttons(wndCacheTab);
		}
	}
	pfc::string8 buffer;

	uGetDlgItemText(wnd, IDC_EDIT_CFG_CACHE_EXP_DAYS, buffer);

	if (is_number(buffer.c_str())) {
		conf_ptr->set_expiration_days(abs(atoi(buffer)));
	}

	uGetDlgItemText(wnd, IDC_EDIT_CACHED_OBJECTS, buffer);

	if (is_number(buffer.c_str())) {
		conf_ptr->cache_max_objects = abs(atol(buffer));
		discogs_interface->set_cache_size(conf_ptr->cache_max_objects);
	}
}

bool CConfigurationDialog::cfg_caching_has_changed() {

	bool bres = false;

	bres |= conf.parse_hidden_merge_titles != conf_edit.parse_hidden_merge_titles;
	bres |= conf.parse_hidden_as_regular != conf_edit.parse_hidden_as_regular;
	bres |= conf.skip_video_tracks != conf_edit.skip_video_tracks;
	bres |= conf.cache_max_objects != conf_edit.cache_max_objects;
	bres |= conf.cache_offline_cache_flag != conf_edit.cache_offline_cache_flag;
	bres |= conf.disk_cache_exp != conf_edit.disk_cache_exp;
	return bres;
}

void CConfigurationDialog::save_art_dialog(HWND wnd, bool dlgbind) {

	pfc::string8 temp;
	foo_conf* conf_ptr = dlgbind? &conf_edit : &conf;

	conf_ptr->save_album_art = uButton_GetCheck(wnd, IDC_CHK_SAVE_ALBUM_ART);
	conf_ptr->album_art_fetch_all = uButton_GetCheck(wnd, IDC_CHK_ALBUM_ART_FETCH_ALL);
	conf_ptr->embed_album_art = uButton_GetCheck(wnd, IDC_CHK_ALBUM_ART_EMBED);
	uGetDlgItemText(wnd, IDC_EDIT_ALBUM_ART_DIR, temp);
	conf_ptr->album_art_directory_string = temp;
	uGetDlgItemText(wnd, IDC_EDIT_ALBUM_ART_PREFIX, temp);
	conf_ptr->album_art_filename_string = temp;
	conf_ptr->album_art_overwrite = uButton_GetCheck(wnd, IDC_CHK_ALBUM_ART_OVR);

	conf_ptr->save_artist_art = uButton_GetCheck(wnd, IDC_CHK_SAVE_ARTIST_ART);
	conf_ptr->artist_art_fetch_all = uButton_GetCheck(wnd, IDC_CHK_ARTIST_ART_FETCH_ALL);
	//todo: remove not impl
	uGetDlgItemText(wnd, IDC_EDIT_ARTIST_ART_IDS, temp);
	conf_ptr->artist_art_id_format_string = temp;
	conf_ptr->embed_artist_art = uButton_GetCheck(wnd, IDC_CHK_ARTIST_ART_EMBED);
	uGetDlgItemText(wnd, IDC_EDIT_ARTIST_ART_DIR, temp);
	conf_ptr->artist_art_directory_string = temp;
	uGetDlgItemText(wnd, IDC_EDIT_ARTIST_ART_PREFIX, temp);
	conf_ptr->artist_art_filename_string = temp;
	conf_ptr->artist_art_overwrite = uButton_GetCheck(wnd, IDC_CHK_ARTIST_ART_OVERWRITE);

	int art_app_flag = LOWORD(conf_ptr->album_art_skip_default_cust);
	//todo: remove IDC_CHK_CFG_ART_FILE_MATCH
	bool bfilematch = uButton_GetCheck(wnd, IDC_CHK_CFG_ART_FILE_MATCH);
	if (bfilematch) {
		art_app_flag |= ARTSAVE_FILEMATCH_APP_FLAG;
	}
	else {
		art_app_flag &= ~(ARTSAVE_FILEMATCH_APP_FLAG);
	}
	conf_ptr->album_art_skip_default_cust = MAKELPARAM(art_app_flag, HIWORD(conf_ptr->album_art_skip_default_cust));

#ifdef SIM_VA_MA_BETA
	bool bsim_va_as_ma = uButton_GetCheck(wnd, IDC_CHK_CFG_ART_SIM_VA_AS_MA);
	if (bsim_va_as_ma) {
		conf_ptr->find_release_dlg_flags |= CFindReleaseDialog::FLG_VA_AS_MA;
	}
	else {
		conf_ptr->find_release_dlg_flags &= ~CFindReleaseDialog::FLG_VA_AS_MA;
		g_clear_va_ma_releases();
	}
#else
	//conf_ptr->find_release_dlg_flags &= ~CFindReleaseDialog::FLG_VA_AS_MA;
#endif
}

bool CConfigurationDialog::cfg_art_has_changed() {

	bool bres = false;
	bool bcmp = false;

	bres |= conf.save_album_art != conf_edit.save_album_art;
	bres |= conf.album_art_fetch_all != conf_edit.album_art_fetch_all;
	bres |= conf.embed_album_art != conf_edit.embed_album_art;

	bcmp = pfc::strcmp_partial(conf.album_art_directory_string, conf_edit.album_art_directory_string);
	bres |= bcmp;
	bcmp = pfc::strcmp_partial(conf.album_art_filename_string, conf_edit.album_art_filename_string);
	bres |= bcmp;

	bres |= conf.album_art_overwrite != conf_edit.album_art_overwrite;
	bres |= conf.save_artist_art != conf_edit.save_artist_art;
	bres |= conf.artist_art_fetch_all != conf_edit.artist_art_fetch_all;
	//todo: remove not impl
	bcmp = pfc::strcmp_partial(conf.artist_art_id_format_string, conf_edit.artist_art_id_format_string);
	bres |= bcmp;

	bres |= conf.embed_artist_art != conf_edit.embed_artist_art;

	bcmp = pfc::strcmp_partial(conf.artist_art_directory_string, conf_edit.artist_art_directory_string);
	bres |= bcmp;

	bcmp = stricmp_utf8(conf.artist_art_filename_string, conf_edit.artist_art_filename_string);
	bres |= bcmp;

	bres |= conf.artist_art_overwrite != conf_edit.artist_art_overwrite;

	bres |= conf.album_art_skip_default_cust != conf_edit.album_art_skip_default_cust;
#ifdef SIM_VA_MA_BETA
	bool sim_va_as_ma = conf.find_release_dlg_flags & CFindReleaseDialog::FLG_VA_AS_MA;
	bool sim_va_as_ma_edit = conf_edit.find_release_dlg_flags & CFindReleaseDialog::FLG_VA_AS_MA;

	bres |= (sim_va_as_ma != sim_va_as_ma_edit);
#endif
	return bres;
}

void CConfigurationDialog::save_ui_dialog(HWND wnd, bool dlgbind) {

	foo_conf* conf_ptr = dlgbind ? &conf_edit : &conf;

	bool history_enabled = uButton_GetCheck(wnd, IDC_CHK_CFG_UI_HISTORY_ENABLED);
	size_t max_items = atoi(uGetDlgItemText(wnd, IDC_EDIT_UI_HISTORY_MAX_ITEMS));

	conf_ptr->history_enabled_max = MAKELPARAM(max_items, history_enabled ? 1 : 0);
	conf_ptr->release_enter_key_override = uButton_GetCheck(wnd, IDC_CHK_RELEASE_ENTER_KEY_OVR);

	FlgMng fv_stats;
	conf_ptr->GetFlagVar(CFG_FIND_RELEASE_DIALOG_FLAG, fv_stats);
	bool val = fv_stats.SetFlag(wnd, IDC_CHK_FIND_RELEASE_STATS, CFindReleaseDialog::FLG_SHOW_RELEASE_TREE_STATS);

	size_t custom_font = uButton_GetCheck(wnd, IDC_CHK_CFG_CUSTOM_FONT_DEFAULT) ? 0 : 0;
	bool chk = uButton_GetCheck(wnd, IDC_CHK_CFG_CUSTOM_FONT_EXPANDED);
	custom_font = chk ? (custom_font | (1 << 0)) : custom_font;
	chk = uButton_GetCheck(wnd, IDC_CHK_CFG_CUSTOM_FONT_FB2K);
	custom_font = chk ? (custom_font | (1 << 1)) : custom_font;

	conf_ptr->custom_font = MAKELPARAM(0, custom_font);

	//todo: move this to on apply
	if (g_discogs->find_release_dialog) {
		g_discogs->find_release_dialog->SetConfigFlag(fv_stats.id(), CFindReleaseDialog::FLG_SHOW_RELEASE_TREE_STATS, val);
	}
	conf_ptr->list_style = ::uSendDlgItemMessage(wnd, IDC_CMB_CONFIG_LIST_STYLE, CB_GETCURSEL, 0, 0);
}

bool CConfigurationDialog::cfg_ui_has_changed() {

	bool bres = false;
	bres |= conf.history_enabled_max != conf_edit.history_enabled_max;
	bres |= conf.release_enter_key_override != conf_edit.release_enter_key_override;
	bres |= conf.list_style != conf_edit.list_style;
	bres |= conf.custom_font != conf_edit.custom_font;

	bres |= (conf.find_release_dlg_flags & CFindReleaseDialog::FLG_SHOW_RELEASE_TREE_STATS) != (conf_edit.find_release_dlg_flags & CFindReleaseDialog::FLG_SHOW_RELEASE_TREE_STATS);

	return bres;
}

void CConfigurationDialog::save_oauth_dialog(HWND wnd, bool dlgbind) {

	foo_conf* conf_ptr = dlgbind ? &conf_edit : &conf;

	pfc::string8 text;
	uGetDlgItemText(wnd, IDC_EDIT_OAUTH_TOKEN, text);
	conf_ptr->oauth_token = text.c_str();

	uGetDlgItemText(wnd, IDC_EDIT_OAUTH_SECRET, text);
	conf_ptr->oauth_token_secret = text.c_str();
}

bool CConfigurationDialog::cfg_oauth_has_changed() {

	bool bres = false;
	bool bcmp = false;

	bcmp = pfc::strcmp_partial(conf.oauth_token, conf_edit.oauth_token);
	bres |= bcmp;
	bcmp = pfc::strcmp_partial(conf.oauth_token_secret, conf_edit.oauth_token_secret);
	bres |= bcmp;

	return bres;
}

void CConfigurationDialog::init_current_tab() {

	if (g_hWndCurrentTab == g_hWndTabDialog[CONF_FIND_RELEASE_TAB]) {
		init_searching_dialog(g_hWndTabDialog[CONF_FIND_RELEASE_TAB], false);
	}
	else if (g_hWndCurrentTab == g_hWndTabDialog[CONF_FIND_ADV_RELEASE_TAB]) {
		init_searching_adv_dialog(g_hWndTabDialog[CONF_FIND_ADV_RELEASE_TAB], false);
	}
	else if (g_hWndCurrentTab == g_hWndTabDialog[CONF_MATCHING_TAB]) {
		init_matching_dialog(g_hWndTabDialog[CONF_MATCHING_TAB], false);
	}
	else if (g_hWndCurrentTab == g_hWndTabDialog[CONF_TAGGING_TAB]) {
		init_tagging_dialog(g_hWndTabDialog[CONF_TAGGING_TAB], false);
	}
	else if (g_hWndCurrentTab == g_hWndTabDialog[CONF_CACHING_TAB]) {
		init_caching_dialog(g_hWndTabDialog[CONF_CACHING_TAB], false);
	}
	else if (g_hWndCurrentTab == g_hWndTabDialog[CONF_ART_TAB]) {
		init_art_dialog(g_hWndTabDialog[CONF_ART_TAB], false);
	}
	else if (g_hWndCurrentTab == g_hWndTabDialog[CONF_UI_TAB]) {
		init_ui_dialog(g_hWndTabDialog[CONF_UI_TAB], false);
	}
	else if (g_hWndCurrentTab == g_hWndTabDialog[CONF_OATH_TAB]) {
		init_oauth_dialog(g_hWndTabDialog[CONF_OATH_TAB], false);
	}
}

INT_PTR WINAPI CConfigurationDialog::on_tagging_dialog_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp) {

	switch (msg) {
	case WM_INITDIALOG:
		setting_dlg = true;
		init_tagging_dialog(wnd, true);
		setting_dlg = false;
		return TRUE;
	case WM_COMMAND:
		if (LOWORD(wp) == IDC_BTN_TAG_MAPPINGS) {
			if (!g_discogs->tag_mappings_dialog) {
				g_discogs->tag_mappings_dialog = fb2k::newDialog<CTagMappingDialog>(core_api::get_main_window());
			}
			else {
				::SetFocus(g_discogs->tag_mappings_dialog->m_hWnd);
			}
		}
		else if (LOWORD(wp) == IDC_BTN_EDIT_CAT_CREDIT) {
		}
		else  {
			if ((HIWORD(wp) == BN_CLICKED) || (HIWORD(wp) == EN_UPDATE)) {
				if (!setting_dlg) {
					save_tagging_dialog(wnd, true);
					OnChanged();
				}
			}
		}
		return FALSE;
	}
	return FALSE;
}

INT_PTR WINAPI CConfigurationDialog::tagging_dialog_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {

	CConfigurationDialog * p_this = nullptr;

	if (msg == WM_INITDIALOG) {
		p_this = (CConfigurationDialog*)(lParam);
		::SetWindowLongPtr(hWnd, GWLP_USERDATA, (LPARAM)p_this);
	}
	else {
		//pointer to class
		p_this = reinterpret_cast<CConfigurationDialog*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));
	}
	return p_this ? p_this->on_tagging_dialog_message(hWnd, msg, wParam, lParam) : FALSE;
}

INT_PTR WINAPI CConfigurationDialog::caching_dialog_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {

	CConfigurationDialog * p_this;
	if (msg == WM_INITDIALOG) {
		p_this = (CConfigurationDialog*)(lParam);
		::SetWindowLongPtr(hWnd, GWLP_USERDATA, (LPARAM)p_this);
	}
	else {

		p_this = reinterpret_cast<CConfigurationDialog*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));
	}
	return p_this ? p_this->on_caching_dialog_message(hWnd, msg, wParam, lParam) : FALSE;
}

INT_PTR WINAPI CConfigurationDialog::on_caching_dialog_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp) {

	switch (msg) {
		case WM_INITDIALOG:
			setting_dlg = true;
			init_caching_dialog(wnd, true);
			setting_dlg = false;
			return TRUE;
		case WM_COMMAND:
			bool brefresh_mem_cache_buttons = false;
			if (LOWORD(wp) == IDC_BTN_CLEAR_CACHE_RELEASES) {
				discogs_interface->reset_release_cache();
				brefresh_mem_cache_buttons = true;
			}
			else if (LOWORD(wp) == IDC_BTN_CLEAR_CACHE_MASTERS) {
				discogs_interface->reset_master_release_cache();
				brefresh_mem_cache_buttons = true;
			}
			else if (LOWORD(wp) == IDC_BTN_CLEAR_CACHE_ARTISTS) {
				discogs_interface->reset_artist_cache();
				brefresh_mem_cache_buttons = true;
			}
			else if (LOWORD(wp) == IDC_BTN_CLEAR_CACHE_COLLECTION) {
				discogs_interface->reset_collection_cache();
				brefresh_mem_cache_buttons = true;
			}
			else {
				if ((HIWORD(wp) == BN_CLICKED) || (HIWORD(wp) == EN_UPDATE)) {
					if (!setting_dlg) {
						save_caching_dialog(wnd, true);
						OnChanged();
					}
				}
			}

			if (brefresh_mem_cache_buttons) {
				init_memory_cache_buttons(wnd);
			}
			return FALSE;
		}
	return FALSE;
}

INT_PTR WINAPI CConfigurationDialog::searching_dialog_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {

	CConfigurationDialog * p_this;

	if (msg == WM_INITDIALOG) {
		p_this = (CConfigurationDialog*)(lParam);
		::SetWindowLongPtr(hWnd, GWLP_USERDATA, (LPARAM)p_this);
	}
	else {
		// retrieve pointer to class
		p_this = reinterpret_cast<CConfigurationDialog*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));
	}
	return p_this ? p_this->on_searching_dialog_message(hWnd, msg, wParam, lParam) : FALSE;
}

INT_PTR WINAPI CConfigurationDialog::on_searching_dialog_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp) {

	switch (msg) {
		case WM_INITDIALOG:
			setting_dlg = true;
			init_searching_dialog(wnd, true);
			setting_dlg = false;
			return TRUE;
		case WM_COMMAND: {

			int idFrom = LOWORD(wp);
			int cmd = HIWORD(wp);

			switch (idFrom) {
			case IDC_COMBO_REL_INIT_QUERY_TYPE:
				if (!setting_dlg) {
					int cmd = HIWORD(wp);
					if (cmd == CBN_SELCHANGE) {
						int s = ::uSendDlgItemMessage(wnd, IDC_COMBO_REL_INIT_QUERY_TYPE, CB_GETCURSEL, 0, 0);
						if (s != CB_ERR) {
							int data = ::uSendDlgItemMessage(wnd, IDC_COMBO_REL_INIT_QUERY_TYPE, CB_GETITEMDATA, s, 0);
							conf_edit.on_init_query_def = data;
						}
						OnChanged();
						break;
					}
				}
				break;
			case (IDC_BTN_CONF_LOAD_FORMATTING):
				on_load_search_formatting(wnd);
				break;
			default:
				if ((HIWORD(wp) == BN_CLICKED) || (HIWORD(wp) == EN_UPDATE)) {
					if (!setting_dlg) {
						save_searching_dialog(wnd, true);
						OnChanged();
					}
				}
			}
			break;
		}
	}
	return FALSE;
}

INT_PTR WINAPI CConfigurationDialog::searching_adv_dialog_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {

	CConfigurationDialog* p_this;
	if (msg == WM_INITDIALOG) {
		p_this = (CConfigurationDialog*)(lParam);
		::SetWindowLongPtr(hWnd, GWLP_USERDATA, (LPARAM)p_this);
	}
	else {
		p_this = reinterpret_cast<CConfigurationDialog*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));
	}
	return p_this ? p_this->on_searching_adv_dialog_message(hWnd, msg, wParam, lParam) : FALSE;
}

INT_PTR WINAPI CConfigurationDialog::on_searching_adv_dialog_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp) {

	switch (msg) {
	case WM_INITDIALOG:
		setting_dlg = true;
		init_searching_adv_dialog(wnd, true);
		setting_dlg = false;
		return TRUE;
	case WM_COMMAND: {

		int idFrom = LOWORD(wp);
		int cmd = HIWORD(wp);

		switch (idFrom) {
		case (IDC_BTN_CONF_LOAD_ADV_FORMATTING):
			on_load_search_adv_formatting(wnd);
			break;
		case IDC_CMB_CONFIG_LIST_STYLE:
			if (!setting_dlg) {
				if (cmd == CBN_SELCHANGE) {
					int s = ::uSendDlgItemMessage(wnd, IDC_CMB_CONFIG_LIST_STYLE, CB_GETCURSEL, 0, 0);
					int data = ::uSendDlgItemMessage(wnd, IDC_CMB_CONFIG_LIST_STYLE, CB_GETITEMDATA, s, 0);
					if (s != CB_ERR) {
						conf_edit.list_style = s;
					}
					OnChanged();
					break;
				}
			}
			break;
		default:
			//MAX_ITEMS...
			if (cmd == BN_CLICKED || cmd == EN_UPDATE) {
				if (!setting_dlg) {
					save_searching_adv_dialog(wnd, true);
					OnChanged();
				}
			}
			break;
		}
		break;
	}
	}
	return FALSE;
}

INT_PTR WINAPI CConfigurationDialog::matching_dialog_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {

	CConfigurationDialog * p_this;
	if (msg == WM_INITDIALOG) {
		p_this = (CConfigurationDialog*)(lParam);
		::SetWindowLongPtr(hWnd, GWLP_USERDATA, (LPARAM)p_this);
	}
	else {
		p_this = reinterpret_cast<CConfigurationDialog*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));
	}
	return p_this ? p_this->on_matching_dialog_message(hWnd, msg, wParam, lParam) : FALSE;
}

INT_PTR WINAPI CConfigurationDialog::on_matching_dialog_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp) {

	switch (msg) {
		case WM_INITDIALOG:
			setting_dlg = true;
			init_matching_dialog(wnd, true);
			setting_dlg = false;
			return TRUE;
		case WM_COMMAND: {

			int idFrom = LOWORD(wp);
			int cmd = HIWORD(wp);

			switch (idFrom) {
			case (IDC_BTN_CONF_LOAD_MATCH_FORMATTING):
				on_load_match_formatting(wnd);
				break;
			default:
				if ((HIWORD(wp) == BN_CLICKED) || (HIWORD(wp) == EN_UPDATE)) {
					if (!setting_dlg) {
						save_matching_dialog(wnd, true);
						OnChanged();
					}
				}
				break;
			}
			break;
		}
	}
	return FALSE;
}

INT_PTR WINAPI CConfigurationDialog::art_dialog_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {

	CConfigurationDialog * p_this;

	if (msg == WM_INITDIALOG) {
		p_this = (CConfigurationDialog*)(lParam); //pointer to class
		::SetWindowLongPtr(hWnd, GWLP_USERDATA, (LPARAM)p_this); //store it for future use
	}
	else {
		p_this = reinterpret_cast<CConfigurationDialog*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));
	}
	return p_this ? p_this->on_art_dialog_message(hWnd, msg, wParam, lParam) : FALSE;
}

INT_PTR WINAPI CConfigurationDialog::on_art_dialog_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp) {

	switch (msg) {
		case WM_INITDIALOG:
			setting_dlg = true;
			init_art_dialog(wnd, true);
			setting_dlg = false;
			return TRUE;
		case WM_COMMAND: {
			if ((HIWORD(wp) == BN_CLICKED)|| (HIWORD(wp) == EN_UPDATE)) {
				if (!setting_dlg) {
				  save_art_dialog(wnd, true);
				  OnChanged();
				}
			}
			break;
		}
	}
	return FALSE;
}

INT_PTR WINAPI CConfigurationDialog::ui_dialog_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {

	CConfigurationDialog* p_this;
	if (msg == WM_INITDIALOG) {
		p_this = (CConfigurationDialog*)(lParam);
		::SetWindowLongPtr(hWnd, GWLP_USERDATA, (LPARAM)p_this);
	}
	else {
		p_this = reinterpret_cast<CConfigurationDialog*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));
	}
	return p_this ? p_this->on_ui_dialog_message(hWnd, msg, wParam, lParam) : FALSE;
}

INT_PTR WINAPI CConfigurationDialog::on_ui_dialog_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp) {

	switch (msg) {
		case WM_INITDIALOG:
			setting_dlg = true;
			init_ui_dialog(wnd, true);
			setting_dlg = false;
			return TRUE;
		case WM_COMMAND: {

			int idFrom = LOWORD(wp);
			int cmd = HIWORD(wp);

			switch (idFrom) {
				case IDC_BTN_UI_CLEAR_HISTORY:
					try {
						on_delete_history(wnd, 0, true);
					}
					catch (foo_discogs_exception e) {
						pfc::string8 error;
						error << ("(skipped) ") << e.what() << "\n";
						completion_notify::ptr reply;
						popup_message_v3::query_t query = { "Information",
							PFC_string_formatter() << "Error(s)", popup_message::icon_error,
							popup_message_v3::buttonOK,
							popup_message_v3::iconQuestion,
							reply
						};
						popup_message_v3::get()->show_query_modal(query);
					}
					break;
				case IDC_CMB_CONFIG_LIST_STYLE:
					if (!setting_dlg) {
						if (cmd == CBN_SELCHANGE) {
							int s = ::uSendDlgItemMessage(wnd, IDC_CMB_CONFIG_LIST_STYLE, CB_GETCURSEL, 0, 0);
							int data = ::uSendDlgItemMessage(wnd, IDC_CMB_CONFIG_LIST_STYLE, CB_GETITEMDATA, s, 0);
							if (s != CB_ERR) {
								conf_edit.list_style = s;
							}
							OnChanged();
							break;
						}
					}
					break;
				default:
					//IDC_UI_HISTORY_MAX_ITEMS...
					if (cmd == BN_CLICKED || cmd == EN_UPDATE) {
						if (!setting_dlg) {
							save_ui_dialog(wnd, true);
							OnChanged();
						}
					}
					break;
			}
			break;
		}
	}
	return FALSE;
}

INT_PTR WINAPI CConfigurationDialog::oauth_dialog_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {

	CConfigurationDialog * p_this;

	if (msg == WM_INITDIALOG) {
		p_this = (CConfigurationDialog*)(lParam);
		::SetWindowLongPtr(hWnd, GWLP_USERDATA, (LPARAM)p_this);
	}
	else {
		p_this = reinterpret_cast<CConfigurationDialog*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));
	}
	return p_this ? p_this->on_oauth_dialog_message(hWnd, msg, wParam, lParam) : FALSE;
}

INT_PTR WINAPI CConfigurationDialog::on_oauth_dialog_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp) {

	pfc::string8 text;
	switch (msg) {
		case WM_INITDIALOG:
			setting_dlg = true;
			init_oauth_dialog(wnd, true);
			setting_dlg = false;
			return TRUE;

		case WM_COMMAND:
			switch (wp) {
				case IDC_BTN_OAUTH_TEST:
					on_test_oauth(g_hWndTabDialog[CONF_OATH_TAB]);
					break;

				case IDC_BTN_OAUTH_AUTHORIZE:
					on_authorize_oauth(g_hWndTabDialog[CONF_OATH_TAB]);
					break;

				case IDC_BTN_OAUTH_GENERATE:
					on_generate_oauth(g_hWndTabDialog[CONF_OATH_TAB]);
					break;
				default:
					if ((HIWORD(wp) == BN_CLICKED) || (HIWORD(wp) == EN_UPDATE)) {
						if (!setting_dlg) {
							save_oauth_dialog(wnd, true);
							OnChanged();
						}
					}
					break;
			}
			break;
	}
	return FALSE;
}

void CConfigurationDialog::on_load_search_formatting(HWND wnd) {

	CRect rcButton;
	HWND hwndCtrl = ::GetDlgItem(wnd, IDC_BTN_CONF_LOAD_FORMATTING);
	::GetWindowRect(hwndCtrl, rcButton);

	POINT pt = {};
	pt.x = rcButton.left;
	pt.y = rcButton.bottom;

	enum { CMD_1 = 1, CMD_2, CMD_3, CMD_4, CMD_5, CMD_6 };
	HMENU hSplitMenu = CreatePopupMenu();

	AppendMenu(hSplitMenu, MF_STRING, CMD_1, L"Default &Release formatting");
	AppendMenu(hSplitMenu, MF_STRING, CMD_2, L"Default &Master formatting");
	AppendMenu(hSplitMenu, MF_STRING, CMD_3, L"Default Master &Sub-release formatting");
	AppendMenu(hSplitMenu, MF_SEPARATOR, 0, 0);
	AppendMenu(hSplitMenu, MF_STRING, CMD_4, L"Default R&elease with artist role");
	AppendMenu(hSplitMenu, MF_STRING, CMD_5, L"Default M&aster with artist role");

	int cmd = TrackPopupMenu(hSplitMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD, pt.x, pt.y, 0, m_hWnd, NULL);
	DestroyMenu(hSplitMenu);

	if (!cmd) return;

	CConf tmpcfg;
	UINT uid = 0;
	pfc::string8 frm_string = "";

	switch (cmd)
	{
	case CMD_1:
		uid = IDC_EDIT_RELEASE_FORMATTING;
		frm_string << tmpcfg.search_release_format_string;
		break;
	case CMD_2:
		uid = IDC_EDIT_MASTER_FORMATTING;
		frm_string << tmpcfg.search_master_format_string;
		break;
	case CMD_3:
		uid = IDC_EDIT_MASTER_SUB_FORMATTING;
		frm_string << tmpcfg.search_master_sub_format_string;
		break;

	case CMD_4:
		uid = IDC_EDIT_RELEASE_FORMATTING;
		frm_string << "[\'[\'%RELEASE_SEARCH_ROLES%\']\' ]" << tmpcfg.search_release_format_string;
		break;
	case CMD_5: {
		uid = IDC_EDIT_MASTER_FORMATTING;
		pfc::string8 buff(tmpcfg.search_master_format_string);
		size_t dolpos = buff.find_first('$');
		frm_string << "\'[T]\' [\'[\'%MASTER_RELEASE_SEARCH_ROLES%\']\' ]" << buff.subString(dolpos);
		break;
	}

	defualt:
		//quit
		return;
	}

	uSetDlgItemText(wnd, uid, frm_string);
	save_searching_dialog(g_hWndTabDialog[CONF_FIND_RELEASE_TAB], true);
	OnChanged();

}

void CConfigurationDialog::on_load_search_adv_formatting(HWND wnd) {

	CRect rcButton;
	HWND hwndCtrl = ::GetDlgItem(wnd, IDC_BTN_CONF_LOAD_ADV_FORMATTING);
	::GetWindowRect(hwndCtrl, rcButton);

	POINT pt = {};
	pt.x = rcButton.left;
	pt.y = rcButton.bottom;

	const LONG ui_ids[3] = { IDC_EDIT_VARIOUS_PREFIXES, IDC_EDIT_MULT_ART_LINKS, IDC_EDIT_REL_INIT_QUERY_TF };
	const pfc::array_t<pfc::string8> rep_strs; 

	CConf tmpcfg;
	size_t i = 0;

	pfc::string8 cmd_1_dlgstr = uGetDlgItemText(wnd, ui_ids[i++]);
	pfc::string8 cmd_2_dlgstr = uGetDlgItemText(wnd, ui_ids[i++]);
	pfc::string8 cmd_3_dlgstr = uGetDlgItemText(wnd, ui_ids[i++]);

	pfc::string8 cmd_1_repstr = tmpcfg.various_prefixes;
	pfc::string8 cmd_2_repstr = tmpcfg.multiple_artists_links;
	pfc::string8 cmd_3_repstr = tmpcfg.on_init_query_tf;

	enum { CMD_1 = 1, CMD_2, CMD_3/*, CMD_4, CMD_5, CMD_6*/ };
	HMENU hSplitMenu = CreatePopupMenu();

	AppendMenu(hSplitMenu, MF_STRING | (cmd_1_dlgstr.equals(cmd_1_repstr) ? MF_DISABLED | MF_GRAYED : 0), CMD_1, L"&Default Various artists prefixes");
	AppendMenu(hSplitMenu, MF_STRING | (cmd_2_dlgstr.equals(cmd_2_repstr) ? MF_DISABLED | MF_GRAYED : 0), CMD_2, L"D&efault Multiple artists joins");
	AppendMenu(hSplitMenu, MF_STRING | (cmd_3_dlgstr.equals(cmd_3_repstr) ? MF_DISABLED | MF_GRAYED : 0), CMD_3, L"De&fault Custom query format");

	int cmd = TrackPopupMenu(hSplitMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD, pt.x, pt.y, 0, m_hWnd, NULL);
	DestroyMenu(hSplitMenu);

	if (!cmd) return;

	UINT uid = 0;
	pfc::string8 frm_string = "";

	switch (cmd)
	{
	case CMD_1:
		uid = ui_ids[0];
		frm_string << cmd_1_repstr;
		break;
	case CMD_2:
		uid = ui_ids[1];
		frm_string << cmd_2_repstr;
		break;
	case CMD_3:
		uid = ui_ids[2];
		frm_string << cmd_3_repstr;
		break;
	defualt:
		//quit
		return;
	}

	uSetDlgItemText(wnd, uid, frm_string);
	save_searching_adv_dialog(g_hWndTabDialog[CONF_FIND_ADV_RELEASE_TAB], true);
	OnChanged();

}

void CConfigurationDialog::on_load_match_formatting(HWND wnd) {

	CRect rcButton;
	HWND hwndCtrl = ::GetDlgItem(wnd, IDC_BTN_CONF_LOAD_MATCH_FORMATTING);
	::GetWindowRect(hwndCtrl, rcButton);

	POINT pt = {};
	pt.x = rcButton.left;
	pt.y = rcButton.bottom;

	const LONG ui_ids[3] = { IDC_EDIT_DISCOGS_FORMATTING, IDC_EDIT_FILE_FORMATTING, IDC_EDIT_STATUS_BAR_FORMATTING };
	const pfc::array_t<pfc::string8> rep_strs;

	CConf tmpcfg;
	size_t i = 0;

	pfc::string8 cmd_1_dlgstr = uGetDlgItemText(wnd, ui_ids[i++]);
	pfc::string8 cmd_2_dlgstr = uGetDlgItemText(wnd, ui_ids[i++]);
	pfc::string8 cmd_3_dlgstr = uGetDlgItemText(wnd, ui_ids[i++]);
	uSetDlgItemText(wnd, IDC_EDIT_DISCOGS_FORMATTING, conf.release_discogs_format_string);
	uSetDlgItemText(wnd, IDC_EDIT_FILE_FORMATTING, conf.release_file_format_string);
	uSetDlgItemText(wnd, IDC_EDIT_STATUS_BAR_FORMATTING, conf.release_status_bar_info_format_string);
	pfc::string8 cmd_1_repstr = tmpcfg.release_discogs_format_string;
	pfc::string8 cmd_2_repstr = tmpcfg.release_file_format_string;
	pfc::string8 cmd_3_repstr = tmpcfg.release_status_bar_info_format_string;

	enum { CMD_1 = 1, CMD_2, CMD_3/*, CMD_4, CMD_5, CMD_6*/ };
	HMENU hSplitMenu = CreatePopupMenu();

	AppendMenu(hSplitMenu, MF_STRING | (cmd_1_dlgstr.equals(cmd_1_repstr) ? MF_DISABLED | MF_GRAYED : 0), CMD_1, L"&Default Discogs track formatting string");
	AppendMenu(hSplitMenu, MF_STRING | (cmd_2_dlgstr.equals(cmd_2_repstr) ? MF_DISABLED | MF_GRAYED : 0), CMD_2, L"D&efault File formatting string");
	AppendMenu(hSplitMenu, MF_STRING | (cmd_3_dlgstr.equals(cmd_3_repstr) ? MF_DISABLED | MF_GRAYED : 0), CMD_3, L"De&fault Status bar release info");

	int cmd = TrackPopupMenu(hSplitMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD, pt.x, pt.y, 0, m_hWnd, NULL);
	DestroyMenu(hSplitMenu);

	if (!cmd) return;

	UINT uid = 0;
	pfc::string8 frm_string = "";

	switch (cmd)
	{
	case CMD_1:
		uid = ui_ids[0];
		frm_string << cmd_1_repstr;
		break;
	case CMD_2:
		uid = ui_ids[1];
		frm_string << cmd_2_repstr;
		break;
	case CMD_3:
		uid = ui_ids[2];
		frm_string << cmd_3_repstr;
		break;
	defualt:
		//quit
		return;
	}

	uSetDlgItemText(wnd, uid, frm_string);
	save_matching_dialog(g_hWndTabDialog[CONF_MATCHING_TAB], true);
	OnChanged();

}

void CConfigurationDialog::on_delete_history(HWND wnd, size_t max, bool zap) {

	if (g_discogs->find_release_dialog) {
		g_discogs->find_release_dialog->get_oplogger()->zap_vhistory();
	}

	// todo: service
	sqldb db;
	size_t inc = db.recharge_history(kcmdHistoryDeleteAll, ~0, {});
	db.close();
}

void CConfigurationDialog::on_test_oauth(HWND wnd) {

	pfc::string8 text;
	uGetDlgItemText(wnd, IDC_EDIT_OAUTH_TOKEN, text);
	pfc::string8 token = pfc::string8(text);

	uGetDlgItemText(wnd, IDC_EDIT_OAUTH_SECRET, text);
	pfc::string8 token_secret = pfc::string8(text);

	service_ptr_t<test_oauth_process_callback> task = new service_impl_t<test_oauth_process_callback>(token, token_secret);

	m_tp.get()->run_modeless(task,
		threaded_process::flag_show_item |
		threaded_process::flag_show_abort,
		m_hWnd,
		"Testing Discogs OAuth support..."
	);
}

void CConfigurationDialog::on_authorize_oauth(HWND wnd) {
	service_ptr_t<authorize_oauth_process_callback> task = new service_impl_t<authorize_oauth_process_callback>();
	task->start(m_hWnd);
}

void CConfigurationDialog::on_generate_oauth(HWND wnd) {
	pfc::string8 code;
	uGetDlgItemText(wnd, IDC_EDIT_OAUTH_PIN, code);

	if (code.get_length() == 0) {
		show_oauth_msg("Enter a valid PIN code.", true);
		return;
	}

	service_ptr_t<generate_oauth_process_callback> task = new service_impl_t<generate_oauth_process_callback>(code);
	task->start(m_hWnd);
}

t_uint32 CConfigurationDialog::get_state() {

	t_uint32 state = preferences_state::resettable;
	if (HasChanged()) state |= preferences_state::changed;
	return state | preferences_state::dark_mode_supported;
}

void CConfigurationDialog::apply() {

	pushcfg(false);
}

bool CConfigurationDialog::HasChanged() {

	bool bchanged = false;

	if (g_hWndCurrentTab == g_hWndTabDialog[CONF_FIND_RELEASE_TAB]) {
		bchanged = cfg_searching_has_changed();
	}
	else if (g_hWndCurrentTab == g_hWndTabDialog[CONF_FIND_ADV_RELEASE_TAB]) {
		bchanged = cfg_searching_adv_has_changed();
	}
	else if (g_hWndCurrentTab == g_hWndTabDialog[CONF_MATCHING_TAB]) {
		bchanged = cfg_matching_has_changed();
	}
	else if (g_hWndCurrentTab == g_hWndTabDialog[CONF_TAGGING_TAB]) {
		bchanged = cfg_tagging_has_changed();
	}
	else if (g_hWndCurrentTab == g_hWndTabDialog[CONF_CACHING_TAB]) {
		bchanged = cfg_caching_has_changed();
	}
	else if (g_hWndCurrentTab == g_hWndTabDialog[CONF_ART_TAB]) {
		bchanged = cfg_art_has_changed();
	}
	else if (g_hWndCurrentTab == g_hWndTabDialog[CONF_UI_TAB]) {
		bchanged = cfg_ui_has_changed();
	}
	else if (g_hWndCurrentTab == g_hWndTabDialog[CONF_OATH_TAB]) {
		bchanged = cfg_oauth_has_changed();
	}
	return bchanged;
}

void CConfigurationDialog::OnChanged() {
	m_callback->on_state_changed();
}

static preferences_page_factory_t<preferences_page_myimpl> g_preferences_page_myimpl_factory;
