#pragma once

#include <array>

#include "libPPUI\HyperLinkCtrl.h"
#include "helpers\DarkMode.h"

#include "my_editwithbuttons.h"

#include "querydefmap.h"
#include "history_oplog.h"
#include "find_release_tree.h"
#include "find_artist_list.h"
#include "find_artist_list_ILO.h"
#include "discogger_search_request.h"

class expand_master_release_process_callback;
class get_artist_process_callback;
class search_artist_process_callback;

static std::once_flag is_wine_light_theme_flag;

class CFindReleaseDialog : public MyCDialogImpl<CFindReleaseDialog>,
	public CMessageFilter, public ILOD_artist_list,
	public CDialogResize<CFindReleaseDialog> {

public:

	CListControlOwnerData* ilo_get_uilist() override {
		return dynamic_cast<CListControlOwnerData*>(&m_alist);
	}

	// constructor / destructor

	CFindReleaseDialog(HWND p_parent, metadb_handle_list items, foo_conf cfg);

	~CFindReleaseDialog();

	virtual BOOL PreTranslateMessage(MSG* pMsg) override {
		return ::IsDialogMessage(m_hWnd, pMsg);
	}

#pragma warning( push )
#pragma warning( disable : 26454 )

	BEGIN_MSG_MAP(CFindReleaseDialog)

		MSG_WM_TIMER(OnTypeFilterTimer)

		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		// Release-tree WM_CONTEXTMENU is handled by chained m_dctree below.
		// No dialog-level handler should intercept its keyboard/right-click menu.
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)

		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
		COMMAND_ID_HANDLER(IDC_EDIT_FILTER, OnEditFilter)

		COMMAND_ID_HANDLER(IDC_BTN_SEARCH, OnButtonSearch)
		COMMAND_ID_HANDLER(IDC_BTN_CONFIGURE, OnButtonConfigure)
		COMMAND_ID_HANDLER(IDC_BTN_PROCESS_RELEASE, OnButtonNext)

		NOTIFY_HANDLER(IDC_ARTIST_LIST, NM_DBLCLK, OnArtistListDoubleClick)

		COMMAND_ID_HANDLER(IDC_CHK_ONLY_EXACT_MATCHES, OnCheckboxOnlyExactMatches)
		COMMAND_ID_HANDLER(IDC_CHK_RELEASE_SHOW_PROFILE, OnCheckboxBindProfilePanel)
		COMMAND_ID_HANDLER(IDC_CHK_FIND_RELEASE_FILTER_VERS, OnCheckboxFindReleaseFilterFlags)
		COMMAND_ID_HANDLER(IDC_CHK_FIND_RELEASE_FILTER_ROLEMAIN, OnCheckboxFindReleaseFilterFlags)

		CHAIN_MSG_MAP_MEMBER(m_dctree)
		CHAIN_MSG_MAP(CDialogResize<CFindReleaseDialog>)
	END_MSG_MAP()

#pragma warning( pop )

	BEGIN_DLGRESIZE_MAP(CFindReleaseDialog)

		DLGRESIZE_CONTROL(IDC_LABEL_RELEASE_ID, DLSZ_MOVE_X | DLSZ_MOVE_Y)
		DLGRESIZE_CONTROL(IDC_LABEL_RELEASE_MINI_ID, DLSZ_MOVE_Y)
		DLGRESIZE_CONTROL(IDC_RELEASE_URL_TEXT, DLSZ_MOVE_X | DLSZ_MOVE_Y)
		DLGRESIZE_CONTROL(IDC_RELEASE_MINI_URL_TEXT, DLSZ_MOVE_Y | DLSZ_SIZE_X)
		DLGRESIZE_CONTROL(IDC_STATIC_FIND_REL_STATS_EXT, DLSZ_MOVE_X | DLSZ_MOVE_Y)
		DLGRESIZE_CONTROL(IDC_EDIT_FILTER, DLSZ_MOVE_X)
		DLGRESIZE_CONTROL(IDC_ARTIST_LIST, DLSZ_SIZE_Y)
		DLGRESIZE_CONTROL(IDC_EDIT_SEARCH, DLSZ_SIZE_X)
		DLGRESIZE_CONTROL(IDC_STATIC_FIND_REL_SEARCH_STATS, DLSZ_SIZE_X)
		DLGRESIZE_CONTROL(IDC_BTN_SEARCH, DLSZ_MOVE_X)
		DLGRESIZE_CONTROL(IDC_CHK_SEARCH, DLSZ_MOVE_X)
		DLGRESIZE_CONTROL(IDC_RELEASE_TREE, DLSZ_SIZE_Y)
		DLGRESIZE_CONTROL(IDC_CHK_FIND_RELEASE_FILTER_ROLEMAIN, DLSZ_MOVE_Y)

		BEGIN_DLGRESIZE_GROUP()

		DLGRESIZE_CONTROL(IDC_ARTIST_LIST, DLSZ_SIZE_X)
		DLGRESIZE_CONTROL(IDC_RELEASE_TREE, DLSZ_SIZE_X)
		DLGRESIZE_CONTROL(IDC_LABEL_RELEASES, DLSZ_MOVE_X)
		DLGRESIZE_CONTROL(IDC_CHK_FIND_RELEASE_FILTER_ROLEMAIN, DLSZ_MOVE_X)

		END_DLGRESIZE_GROUP()

		DLGRESIZE_CONTROL(IDC_CHK_FIND_RELEASE_FILTER_VERS, DLSZ_MOVE_X)
		DLGRESIZE_CONTROL(IDC_LABEL_FILTER, DLSZ_MOVE_X)
		DLGRESIZE_CONTROL(IDC_CHK_ONLY_EXACT_MATCHES, DLSZ_MOVE_Y)
		DLGRESIZE_CONTROL(IDC_CHK_RELEASE_SHOW_PROFILE, DLSZ_MOVE_Y)
		DLGRESIZE_CONTROL(IDC_BTN_CONFIGURE, DLSZ_MOVE_Y)
		DLGRESIZE_CONTROL(IDC_STATIC_FIND_REL_STATS, DLSZ_MOVE_Y | DLSZ_SIZE_X)
		DLGRESIZE_CONTROL(IDC_BTN_PROCESS_RELEASE, DLSZ_MOVE_X)

	END_DLGRESIZE_MAP()

	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
		m_dctree.EnableDispInfo(false);
		destroy();
		return TRUE;
	}

	void DlgResize_UpdateLayout(int cxWidth, int cyHeight) {
		CDialogResize<CFindReleaseDialog>::DlgResize_UpdateLayout(cxWidth, cyHeight);
	}

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	LRESULT OnButtonNext(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnArtistListDoubleClick(int idCtrl, LPNMHDR hdr, BOOL& bHandled);

	LRESULT OnButtonSearch(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnButtonConfigure(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	LRESULT OnCheckboxBindProfilePanel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCheckboxOnlyExactMatches(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCheckboxFindReleaseFilterFlags(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	//..

	LRESULT OnEditFilter(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	void OnTypeFilterTimer(WPARAM id);
	void apply_filter(pfc::string8 strFilter, bool force_redraw, bool force_rebuild, threaded_process_status& p_status, abort_callback& p_abort);

	void SetSearchMode(SearchMode sm);
	void SetSearchModeText(const pfc::string8 msg, bool mode_prefix = true);
	void SetSearchModeFlag(SearchMode sm, bool enabled);
	size_t GetSearchMode();
    // Main-thread-only identity: stale modeless completions cannot replace
    // a newer artist, direct-ID or advanced-search request in this dialog.
    std::uint64_t current_search_request() const noexcept { return m_search_request_epoch.current(); }
    bool is_current_search_request(std::uint64_t ticket) const noexcept { return m_search_request_epoch.matches(ticket); }
    std::uint64_t current_filter_request() const noexcept { return m_filter_request_epoch.current(); }
    bool is_current_filter_request(std::uint64_t ticket) const noexcept { return m_filter_request_epoch.matches(ticket); }

	bool GetCustomQueryTF(pfc::string8& frm_custom_tf);

	bool GetSearchMultiArtistsString(pfc::string8& out);

	pfc::string8 GetSearchModeMsg();
	void UpdateSearchQueryHasRun(const pfc::string8 expression);
	void UpdateSearchArtistHasRun(const pfc::string8 artist);
	void GetCurrentExpression(pfc::string8& out);
	void GetCurrentSearchArtist(pfc::string8& out);
	void UpdateSearchModeDisplayMode(const char* artist);
	void InvalidateSearchDisplay(bool list_tree);

	//todo: 
	friend class expand_master_release_process_callback;
	friend class tree_apply_filter_process_callback;
	friend class get_artist_process_callback;
	friend class on_search_artist_done_process_callback;
	friend class on_get_artist_done_process_callback;
	friend class get_multi_artists_process_callback;
	friend class search_artist_process_callback;

	friend class CArtistList;
	friend class CFindReleaseTree;
	//..

	void show() override;
	void hide() override;
	void enable(bool enabled) override;

	void enable_alt(bool enabled);

	//serves track_matching dlg->go_back()
	void SetItems(metadb_handle_list track_matching_items) { m_items = track_matching_items; }

	//serves EnterKeySubclassProc
	bool ForwardVKReturn();

	UINT IDD = IDD_DIALOG_FIND_RELEASE;

	//flag_enum_findrelease_dialog
	enum flg_fr {
		FLG_PROFILE_DLG_SHOW        = 1 << 0,
		FLG_PROFILE_DLG_ATTACHED    = 1 << 1,
		FLG_SHOW_RELEASE_TREE_STATS = 1 << 2,
		FLG_RESERVED                = 1 << 3,
		FLG_RESERVED2               = 1 << 4,
		FLG_VA_AS_MA                = 1 << 5
	};

	history_oplog* get_oplogger() { return &m_oplogger; }

	size_t get_tree_artist_list_pos() { return m_dctree.Get_Artist_List_Position(); }

	void ShowArtistSearchResults(Artist_ptr artist);

	//serves tree profile refresh requests, ...
	void UpdateArtistProfile(Artist_ptr artist);
	pfc::string EscapeWin(pfc::string8 keyWord) {
		pfc::string8 out_keyWord(keyWord);
		out_keyWord.replace_string("&", "&&");
		return out_keyWord;
	}

	void print_root_stats(rppair root_stat, bool save = true, bool isartist = true, bool onlylink = false);

	bool add_history(oplog_type optype, std::string cmd, pfc::string8 ff, pfc::string8 fs, pfc::string8 sf, pfc::string8 ss);
	bool add_history(oplog_type optype, std::string cmd, rppair row);

	bool SetConfigFlag(int ID, int flag, bool flagvalue);
	std::mutex m_loading_selection_rw_mutex;
	size_t m_loading_selection_id = pfc_infinite;

private:

	//timer

	enum {
		KTypeFilterTimerID = 0xd21e0907,
		KTypeFilterTimeOut = 500
	};

	void KFilterTimerOn(CWindow parent) { m_tickCount = 0; SetTimer(KTypeFilterTimerID, KTypeFilterTimeOut); }
	void KTurnOffFilterTimer() throw() { KillTimer(KTypeFilterTimerID); }

	//controls

	std::function<bool()>stdf_conditioned_invalidation = [this]() {

		return m_is_wine_light_theme;
	};


	std::function<bool()>stdf_enteroverride_artist = [this]() {

		BOOL bdummy;
		HWND button = uGetDlgItem(IDC_BTN_SEARCH);

		if (::IsWindowEnabled(button)) OnButtonSearch(0L, 0L, NULL, bdummy);
		return false;
	};

	std::function<bool()>stdf_enteroverride_url = [this]() {

		BOOL bdummy;
		HWND button = uGetDlgItem(IDC_BTN_PROCESS_RELEASE);

		if (::IsWindowEnabled(button)) OnButtonNext(0L, 0L, NULL, bdummy);
		return false;
	};

	//callback history

	std::function<bool(HWND hwnd, wchar_t* editval)> m_stdf_call_history = [&](HWND hwnd, wchar_t* wstrt) {

		oplog_type optype =
			hwnd == this->m_edit_artist ? oplog_type::artist :
			hwnd == this->m_edit_release ? oplog_type::release :
			hwnd == this->m_edit_filter ? oplog_type::filter : oplog_type::filter;

		//show & capture menu cmd

		pfc::string8 menu_cmd;
		bool menu_hit = this->get_oplogger()->do_history_menu(optype, hwnd, menu_cmd);

		//process menu cmd

		if (menu_hit) {

			pfc::string8 in = pfc::stringcvt::string_utf8_from_os(wstrt).get_ptr();

			wchar_t wide_menu_cmd[MAX_PATH + 1];

			pfc::stringcvt::convert_utf8_to_wide(wide_menu_cmd, MAX_PATH, menu_cmd, menu_cmd.get_length());

			//transfer result text
			_tcscpy_s(wstrt, MAX_PATH, wide_menu_cmd);

			//input != output ?
			return ((bool)stricmp_utf8(in, menu_cmd));
		}

		return false;
	};

	//callback search query and history

	//todo
	pfc::string8 process_search_query_menu_hit(HWND hwnd, wchar_t* wstrt, pfc::string8 menu_cmd, size_t &ins_pos) {

		pfc::string8 param_text = trim(uGetWindowText(m_edit_artist));

		DWORD dwStart, dwEnd;
		dwStart = dwEnd = 0;
		SendMessage(m_edit_artist, EM_GETSEL, (WPARAM)&dwStart, (LPARAM)&dwEnd);

		bool bsel = dwStart != dwEnd;

		const pfc::string8 in = pfc::stringcvt::string_utf8_from_os(wstrt).get_ptr();

		pfc::string8 buffer(in);
		pfc::string8 tmpField;

		if (menu_cmd.get_length() < kv_query_fields_ordered.size()) {
			for (auto w : kv_query_fields_ordered) {
				if (!w.compare(menu_cmd)) {
					tmpField = w.c_str();
					break;
				}
			}
		}

		if (tmpField.get_length()) {

			if (!bsel) {
				bool has_focus = GetFocus() == m_edit_artist;
				if (has_focus && !dwStart) {
					//cursor at pos 0
					t_size fld_size = tmpField.get_length();
					if (param_text.get_length()) {
						tmpField << " ";
					}
					buffer = tmpField;
					buffer << in;
					ins_pos = fld_size /*+ 1*/;
				}
				else if (!has_focus || dwStart == param_text.get_length()) {
					//cursor at last pos
					buffer << " & " << tmpField;
					ins_pos = buffer.get_length() + 3;
				}
				else {
					t_size fld_size = tmpField.get_length();
					tmpField << in.subString(dwEnd);
					buffer = in.subString(0, dwStart);
					buffer << " & " << tmpField;
					ins_pos = dwStart + fld_size + 3;
				}
			}
			else {
				pfc::string8 join = "";
				if (dwStart) join = " & ";
				buffer = in.subString(0, dwStart);
					buffer << join << tmpField << " " << in.subString(dwEnd, in.get_length());
				ins_pos = dwStart + tmpField.get_length() + join.get_length();
			}
		}
		else {
			if (menu_cmd.startsWith("Web")) {

				pfc::string8 url;

				if (menu_cmd.equals("Web Discogs advanced search")) {

					url << DISCOGS_PUBLIC_SEARCH_URL << "advanced";
				}
				else if (menu_cmd.equals("Web Discogs current search")) {

					QueryDefMap qdm_search_query = {};

					bool bquery = search_query::TextToMap(buffer, qdm_search_query);

					std::pair<std::string, std::string> exp_pair;
					search_query::MapToText(qdm_search_query, exp_pair, true);

					pfc::string8 query_scaped = exp_pair.second.c_str();

					url << DISCOGS_PUBLIC_SEARCH_URL << "?" << query_scaped;
				}

				if (url.get_length()) {
					try {
						display_url(url);
					}
					catch (...) {};
				}
			}
			else {
				//history or templates
				buffer = menu_cmd;
				ins_pos = buffer.get_length();
			}
		}

		return buffer;
	}

	std::function<bool(HWND hwnd, wchar_t* editval, size_t& ins_pos)> m_stdf_call_history_query = [&](HWND hwnd/*, HMENU hmenu*/, wchar_t* wstrt, size_t& ins_pos) {

		if (hwnd != this->m_edit_artist) {
			return false;
		}

		oplog_type optype = oplog_type::query;

		pfc::string8 menu_cmd;

		pfc::string8 buffer;
		GetCurrentExpression(buffer);

		if (buffer.get_length()) {

			QueryDefMap qdm_search_query = {};

			bool bquery = search_query::TextToMap(buffer, qdm_search_query);

			bquery &= search_query::CountAvailableFields(qdm_search_query) > 0; // ::IsMinimal(qdm_search_query);

			if (bquery) {
				//..
				menu_cmd = buffer;
				//..
			}
		}

		pfc::string8 tmp_album_artist;
		pfc::string8 tmp_artist;
		pfc::string8 tmp_title;
		pfc::string8 tmp_track;


		if ((m_frm_album_artist.get_length() || m_frm_artist.get_length())) {
			tmp_album_artist = m_frm_album_artist;
			tmp_artist = m_frm_artist;
			tmp_title = m_frm_album;
			tmp_track = m_frm_track_title;

		}
		else {

			//artist
			std::vector<pfc::string8> vsplit_title;
			split(m_frm_album, "-", 0, vsplit_title);
			tmp_artist << (m_frm_album_artist.get_length() ?
				m_frm_album_artist : m_frm_artist.get_length() ? m_frm_artist : vsplit_title.size() > 1 ? vsplit_title[0] : pfc::string8());

			//title
			tmp_title << (vsplit_title.size() > 1 ? vsplit_title[1] : m_frm_album);

			//track
			vsplit_title.clear();
			split(m_frm_track_title, "-", 0, vsplit_title);

			if (vsplit_title.size()) {
				if (vsplit_title.size() > 1 && tmp_artist.equals(vsplit_title[0])) {
					tmp_track = trim(vsplit_title[1]);
				}
				else {
					tmp_track = trim(vsplit_title[0]);
				}
			}
			else {
				tmp_track = m_frm_track_title;
			}
		}

		pfc::string8 frm_sani_artist(tmp_artist);
		bool bdone = search_query::sanitize_various_artists_with_csv(conf.various_prefixes.c_str(), frm_sani_artist);

		pfc::string8 param_text = trim(uGetWindowText(m_edit_artist));

		//todo
		param_text = param_text;
		param_text << "�" << tmp_album_artist;
		param_text << "�" << tmp_artist;
		param_text << "�" << tmp_title;
		param_text << "�" << tmp_track;
		param_text << "�" << frm_sani_artist;

		//show & capture menu cmd

		bool menu_hit = this->get_oplogger()->do_history_menu(optype, hwnd/*, hmenu*/, menu_cmd, param_text);

		//process menu cmd

		if (menu_hit) {

			buffer = process_search_query_menu_hit(hwnd, wstrt, menu_cmd, ins_pos);

			if (!buffer.get_length()) {
				//..
				return false;
				//..
			}

			wchar_t wide_cmd[MAX_PATH + 1];

			pfc::stringcvt::convert_utf8_to_wide(wide_cmd, MAX_PATH, /*menu_cmd*/buffer, /*menu_cmd*/buffer.get_length());

			pfc::string8 in = pfc::stringcvt::string_utf8_from_os(wstrt).get_ptr();

			//transfer result text
			_tcscpy_s(wstrt, MAX_PATH, wide_cmd);

			//input != output ?
			return ((bool)stricmp_utf8(in, buffer));
		}

		return false;
	};

	std::function<bool(HWND hwnd, wchar_t* editval, size_t ndx)> m_stdf_call_history_query_shortcut = [&](HWND hwnd, wchar_t* wstrt, size_t ndx) {
		pfc::string8 frm_custom_tf;
		GetCustomQueryTF(frm_custom_tf);

		pfc::string8 buffer;
		bool done = search_query::GetSuggestion(m_frm_album_artist, m_frm_artist, m_frm_album, m_frm_track_title, m_frm_sani_artist, frm_custom_tf, ndx, buffer);
		if (done) {
			wchar_t wide_cmd[MAX_PATH + 1];

			pfc::stringcvt::convert_utf8_to_wide(wide_cmd, MAX_PATH, buffer, buffer.get_length());

			pfc::string8 in = pfc::stringcvt::string_utf8_from_os(wstrt).get_ptr();

			//transfer result text
			_tcscpy_s(wstrt, MAX_PATH, wide_cmd);
			return ((bool)stricmp_utf8(in, buffer));
		}
		return false;
	};

	void set_history_key_override() {

		cewb_release_filter.SetHistoryHandler(m_stdf_call_history);
		ceqwb_artist_search.SetQueryHistoryHandlerShortCut(m_stdf_call_history_query_shortcut);
		ceqwb_artist_search.SetQueryHistoryHandler(m_stdf_call_history_query);
		ceqwb_artist_search.SetHistoryHandler(m_stdf_call_history);
		cewb_release_url.SetHistoryHandler(m_stdf_call_history);
	}

	void set_enter_key_override(bool enter_ovr) {

		if (enter_ovr) {
			ceqwb_artist_search.SetEnterOverride(stdf_enteroverride_artist);
			cewb_release_url.SetEnterOverride(stdf_enteroverride_url);
		}
		else {
			ceqwb_artist_search.SetEnterOverride(nullptr);
			cewb_release_url.SetEnterOverride(nullptr);
		}
	}

	void set_conditioned_invalidation()
	{
		ceqwb_artist_search.SetConditionedInvalidation(stdf_conditioned_invalidation);
		cewb_release_filter.SetConditionedInvalidation(stdf_conditioned_invalidation);
		cewb_release_url.SetConditionedInvalidation(stdf_conditioned_invalidation);

	}
	void set_role_label(bool filtered) {

		pfc::string8 note = (PFC_string_formatter() << "&main role " << (filtered ? "&& filtered versions" : ""));
		uSetDlgItemText(m_hWnd, IDC_CHK_FIND_RELEASE_FILTER_ROLEMAIN, note);
	}

	void init_cfged_dialog_controls();

	// dlg config

	bool build_current_cfg();
	void pushcfg();

	// artist get/search service callbacks

	void on_get_artist_done(cupdRelSrc updsrc, Artist_ptr& artist);
	void on_search_artist_done(const pfc::array_t<Artist_ptr>& p_artist_exact_matches, const pfc::array_t<Artist_ptr>& p_artist_other_matches, bool append, std::pair<size_t, size_t>& out_va_cap);
    bool on_search_artist_done_if_current(std::uint64_t ticket,
        const pfc::array_t<Artist_ptr>& exact, const pfc::array_t<Artist_ptr>& other,
        bool append, std::pair<size_t, size_t>& cap);
    bool on_search_artist_done_if_current(std::uint64_t ticket,
        const pfc::array_t<Artist_ptr>& exact, const pfc::array_t<Artist_ptr>& other,
        bool append) {
        std::pair<size_t, size_t> cap{0, 0};
        return on_search_artist_done_if_current(ticket, exact, other, append, cap);
    }
#if defined(DISCOGGER_RUNNER_MEDIA_QA)
public:
    // TEST-ONLY: available solely in the isolated runner component.
    std::pair<size_t, size_t> runner_qa_populated_search_rows();
    std::array<size_t, 3> runner_qa_live_search_probe(const char* expected_release_id);
private:
#endif
    void on_search_artist_done(const pfc::array_t<Artist_ptr>& p_artist_exact_matches, const pfc::array_t<Artist_ptr>& p_artist_other_matches, bool append) {
		std::pair<size_t, size_t> unused_va_cap{0, 0};
		on_search_artist_done(p_artist_exact_matches, p_artist_other_matches, append, unused_va_cap);
	}

	// route artist search
	void route_artist_search(pfc::string8 artistname, bool dlgbutton, bool idded);

	// release service callbacks

	void call_expand_master_service(MasterRelease_ptr& master_release, int pos); //serves tree
	void on_expand_master_release_done(const MasterRelease_ptr& master_release, int pos, threaded_process_status& p_status, abort_callback& p_abort, const MasterRelease_ptr& staged_master = nullptr);

	// spawns get_artist_process_callback

	void convey_artist_list_selection(cupdRelSrc in_cupdsrc);

	// spawns process_release_callback

	void trigger_release(const pfc::string8& release_id);

	//..

	bool id_from_url(HWND hwndCtrl, pfc::string8& out);

	//filter box delay/timer
	bool is_filter_box_event_enabled() { return m_filter_box_events_enabled; }
	void block_filter_box_events(bool state) { m_filter_box_events_enabled = !state; }

	//filter box album autofill...
	bool is_filter_autofill_enabled() { return m_filter_box_autofill_enabled; }
	void block_autofill_release_filter(bool blocked) { m_filter_box_autofill_enabled = !blocked; }

private:

	metadb_handle_list m_items;

	foo_conf conf;
	history_oplog m_oplogger;
	id_tracer m_tracer;

	rppair m_row_stats;

	CFindReleaseTree m_dctree;
	CArtistList m_alist;

	CMyEditQueryWithButtons ceqwb_artist_search;
	CMyEditWithButtons cewb_release_filter;
	CMyEditWithButtons cewb_release_url;

	inline static bool m_is_wine_light_theme;

	CHyperLink m_artist_link;

	fb2k::CDarkModeHooks m_dark;

	uint32_t m_tickCount = 0;
	bool m_filter_box_events_enabled = true;
	bool m_filter_box_autofill_enabled = true;

	bool m_updating_releases;

	service_ptr_t<titleformat_object> m_album_name_script;
	service_ptr_t<titleformat_object> m_artist_name_script;
	service_ptr_t<titleformat_object> m_album_artist_script;
	service_ptr_t<titleformat_object> m_track_title_script;
	service_ptr_t<titleformat_object> m_query_custom_script;

	formatting_script m_query_custom_tf;
	
	pfc::string8 m_search_artist;
	pfc::string8 m_search_expression;

    discogger_async::search_request_epoch m_search_request_epoch;
    discogger_async::search_request_epoch m_filter_request_epoch;
	size_t m_query_mode = SearchMode::DEFAULT_SEARCH;
	QueryDefMap m_qdm_search_query = {};

	HWND m_artist_list = nullptr;
	HWND m_release_tree = nullptr;
	HWND m_edit_release = nullptr;
	HWND m_edit_artist = nullptr;
	HWND m_edit_filter = nullptr;
	HWND m_static_search_msg = nullptr;
	HWND m_chkbox_rolemain = nullptr;
	
	pfc::string8 m_frm_album;
	pfc::string8 m_frm_artist;
	pfc::string8 m_frm_album_artist;
	pfc::string8 m_frm_track_title;
	pfc::string8 m_frm_sani_artist; //when used as credit

	friend class CArtistList;
	friend class ILOD_artist_list;

public:

		decltype(conf)const& config() const { return conf; }

};

// Subclass proc to hook WM_GETDLGCODE VKRETURN

static LRESULT CALLBACK EnterKeySubclassProc(
	HWND hwnd, UINT uiMsg, WPARAM wParam, LPARAM lParam,
	UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	switch (uiMsg) {
	case WM_NCDESTROY:
		RemoveWindowSubclass(hwnd, EnterKeySubclassProc,
			uIdSubclass);
		break;

	case WM_GETDLGCODE:
	{
		LPMSG lpMsg = (LPMSG)lParam;

		if (lpMsg == NULL) {

			return 0;
		}

		switch (lpMsg->message) {

		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			switch (lpMsg->wParam) {

			case VK_RETURN: {

				//forward to dialog member FowardVKReturn...

				bool bwant_return =
					g_discogs->find_release_dialog->ForwardVKReturn();

				if (bwant_return) {
					return DefSubclassProc(hwnd, uiMsg, wParam, lParam)
						& ~VK_RETURN;
				}
				break;
			}
			case VK_ESCAPE:

				return DefSubclassProc(hwnd, uiMsg, wParam, lParam)
					| DLGC_WANTALLKEYS;

			default:
				;
			}

		default:
			;
		}
	}
	}
	return DefSubclassProc(hwnd, uiMsg, wParam, lParam);
}
