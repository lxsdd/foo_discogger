#include "stdafx.h"
#include <cstdlib>
#include <cstring>
#include "configuration_dialog.h"
#include "tasks.h"

#include "utils.h"
#include "utils_db.h"
#include "ol_cache.h"

#include "tag_mappings_dialog.h"
#include "find_release_dialog.h"
#if defined(DISCOGGER_RUNNER_MEDIA_QA)
#include "tests/runner-live-search-telemetry.h"
#endif
#include "discogger_release_id.h"
#include "discogger_artist_activation.h"
#include "discogger_artist_refresh.h"
#include "discogs_interface.h"
#include "discogger_search_visibility.h"
#include <unordered_set>
#include <iterator>

namespace ol = Offline;

namespace {
// Advanced Discogs searches can match releases under several artist identities.
// The search tree is a preview, not an artist selection or a tagging decision.
// Keep the original artist list for explicit selection while displaying every
// already fetched result (once per Discogs ID) without another API request.
Artist_ptr combined_advanced_search_preview(const pfc::array_t<Artist_ptr>& artists) {
    Artist_ptr preview = std::make_shared<Artist>("0"); // existing search preview sentinel
    preview->name = "Search results across artists";
    preview->loaded_preview = true;
    // A search response is partial; do not advertise an authoritative artist catalog.
    preview->loaded_releases = false;
    std::unordered_set<std::string> seen_masters, seen_releases;
    auto append_master = [&](const MasterRelease_ptr& master) {
        if (master && seen_masters.emplace(master->id.get_ptr()).second) {
            preview->master_releases.append_single(master);
            preview->search_order_master.append_single(true);
        }
    };
    auto append_release = [&](const Release_ptr& release) {
        if (release && seen_releases.emplace(release->id.get_ptr()).second) {
            preview->releases.append_single(release);
            preview->search_order_master.append_single(false);
        }
    };

    for (size_t i = 0; i < artists.get_count(); ++i) {
        const Artist_ptr& artist = artists[i];
        if (!artist) continue;
        size_t master_idx = 0, release_idx = 0;
        for (size_t order = 0; order < artist->search_order_master.get_count(); ++order) {
            if (artist->search_order_master[order]) {
                if (master_idx < artist->master_releases.get_count())
                    append_master(artist->master_releases[master_idx]);
                ++master_idx;
            } else {
                if (release_idx < artist->releases.get_count())
                    append_release(artist->releases[release_idx]);
                ++release_idx;
            }
        }
        // Defensive: an incomplete upstream order array must not hide results.
        while (master_idx < artist->master_releases.get_count())
            append_master(artist->master_releases[master_idx++]);
        while (release_idx < artist->releases.get_count())
            append_release(artist->releases[release_idx++]);
    }
    return preview;
}
} // namespace


// find release
// {F924481B-17A0-423F-B1A6-07293DC46810}
static const GUID guid_cfg_dialog_position_find_release_dlg = { 0xf924481b, 0x17a0, 0x423f, { 0xb1, 0xa6, 0x7, 0x29, 0x3d, 0xc4, 0x68, 0x10 } };
static cfgDialogPosition cfg_dialog_position_find_release_dlg(guid_cfg_dialog_position_find_release_dlg);
// find release mini
// {0EE3A123-730D-41C6-BAFF-47B4ACB783F5}
static const GUID guid_cfg_dialog_position_find_release_mini_dlg = { 0xee3a123, 0x730d, 0x41c6, { 0xba, 0xff, 0x47, 0xb4, 0xac, 0xb7, 0x83, 0xf5 } };
static cfgDialogPosition cfg_dialog_position_find_release_mini_dlg(guid_cfg_dialog_position_find_release_mini_dlg);

void load_global_icons() {

	auto dpiX = QueryScreenDPIEx(core_api::get_main_window()).cx;

	bool bdark = fb2k::isDarkMode() || check_os_wine_dark_no_theme();
	g_hIcon_quian = LoadDpiIconResource(!bdark ? Icon::Quian : Icon::Quian_Dark, dpiX);
	g_hIcon_rec = LoadDpiBitmapResource(Icon::Record, bdark);

}

// constructor

CFindReleaseDialog::CFindReleaseDialog(HWND p_parent, metadb_handle_list items, foo_conf cfg) : m_items(items), conf(cfg),
		ceqwb_artist_search(), cewb_release_filter(), cewb_release_url(),
		m_dctree(this, &m_tracer), m_alist(this, &m_tracer),
		m_query_mode(SearchMode::DEFAULT_SEARCH) {

	if (!CONF.awt_get_alt_mode()) {
		IDD = IDD_DIALOG_FIND_RELEASE;
	}
	else {
		IDD = IDD_DIALOG_FIND_RELEASE_MINI;
	}

	load_global_icons();

	if (cfg.on_init_query_tf.get_length()) {
		static_api_ptr_t<titleformat_compiler>()->compile_safe_ex(m_query_custom_script, cfg.on_init_query_tf);
		m_query_custom_tf = cfg.on_init_query_tf;
	}

	//HWND

	m_artist_list = nullptr;
	m_release_tree = nullptr;
	m_edit_artist = nullptr;
	m_edit_release = nullptr;
	m_edit_filter = nullptr;

	//..

	m_updating_releases = false;
	m_tickCount = 0;

	static_api_ptr_t<titleformat_compiler>()->compile_force(m_album_name_script, "[%album%]");
	static_api_ptr_t<titleformat_compiler>()->compile_force(m_artist_name_script, "[%artist%]");
	static_api_ptr_t<titleformat_compiler>()->compile_force(m_album_artist_script, "[%album artist%]");
	static_api_ptr_t<titleformat_compiler>()->compile_force(m_track_title_script, "[%title%]");

	std::call_once(is_wine_light_theme_flag, []() {
		if (IsWine()) {
			TCHAR theme_name[MAX_PATH];
			auto hres = GetCurrentThemeName(theme_name, MAX_PATH, nullptr, 0, nullptr, 0);
			hres = GetThemeDocumentationProperty(theme_name,
				SZ_THDOCPROP_DISPLAYNAME, theme_name, MAX_PATH);
			auto cvt = pfc::stringcvt::string_utf8_from_os(theme_name, MAX_PATH);
			pfc::string8 str_cvt(cvt);
			m_is_wine_light_theme = str_cvt.equals("Light");
		}
		else {
			m_is_wine_light_theme = false;
		}

		});

}

CFindReleaseDialog::~CFindReleaseDialog() {
	if (g_discogs) {
		awt_update_mod_flag(/*fromFlag*/false);

#ifdef SIM_VA_MA_BETA
		//disable 
		if (conf.find_release_dlg_flags & FLG_VA_AS_MA) {
			conf.find_release_dlg_flags &= ~FLG_VA_AS_MA;
			CONF.find_release_dlg_flags &= ~FLG_VA_AS_MA;
			CONF.save(CConf::cfgFilter::CONF, conf, CFG_FIND_RELEASE_DIALOG_FLAG);
			g_clear_va_ma_releases();
		}
#endif
		g_discogs->find_release_dialog = nullptr;
	}
}

void CFindReleaseDialog::show() /*override*/ {

	if (g_discogs->find_release_artist_dialog) {
		::ShowWindow(g_discogs->find_release_artist_dialog->m_hWnd, SW_SHOW);
	}

	MyCDialogImpl::show();

}

void CFindReleaseDialog::hide() /*override*/ {

	if (g_discogs->find_release_artist_dialog) {
		::ShowWindow(g_discogs->find_release_artist_dialog->m_hWnd, SW_HIDE);
	}

	MyCDialogImpl::hide();

}

void CFindReleaseDialog::enable(bool is_enabled) /*override*/ {
	//..
}

void CFindReleaseDialog::enable_alt(bool is_enabled) {

	HWND h = GetDlgItem(IDC_RELEASE_MINI_URL_TEXT);
	::ShowWindow(h, is_enabled ? SW_SHOW : SW_HIDE);
	h = GetDlgItem(IDC_RELEASE_URL_TEXT);
	::ShowWindow(h, !is_enabled ? SW_SHOW : SW_HIDE);
	h = GetDlgItem(IDC_LABEL_RELEASE_MINI_ID);
	::ShowWindow(h, is_enabled ? SW_SHOW : SW_HIDE);
	h = GetDlgItem(IDC_LABEL_RELEASE_ID);
	::ShowWindow(h, !is_enabled ? SW_SHOW : SW_HIDE);

	HWND h1 = GetDlgItem(IDC_RELEASE_MINI_URL_TEXT);
	HWND h2 = GetDlgItem(IDC_BTN_PROCESS_RELEASE);
	HWND h3 = GetDlgItem(IDC_LABEL_RELEASE_ID);
	HWND h4 = GetDlgItem(IDC_BTN_CONFIGURE);
	HWND h5 = GetDlgItem(IDC_STATIC_FIND_REL_STATS);

	for (HWND walk = ::GetWindow(m_hWnd, GW_CHILD); walk != NULL; ) {
		HWND next = ::GetWindow(walk, GW_HWNDNEXT);
		if (is_enabled && next != h1 && next != h2 && next != h3 && next != h4 && next != h5) {
			::uEnableWindow(next, !is_enabled);
		}
		else if (!is_enabled) {
			//..
			return;
		}
		walk = next;
	}
}

// settings

void CFindReleaseDialog::pushcfg() {

	if (build_current_cfg()) {
		CONF.save(CConf::cfgFilter::FIND, conf);
		CONF.load();
	}
}

inline bool CFindReleaseDialog::build_current_cfg() {

	bool bres = false;
	bool bcheck = IsDlgButtonChecked(IDC_CHK_ONLY_EXACT_MATCHES);

	if (CONF.display_exact_matches != bcheck) {
		conf.display_exact_matches = bcheck;
		bres |= true;
	}

	conf.find_release_dlg_flags &= ~FilterFlag::RoleMainAT;
	if ((CONF.find_release_filter_flag & FilterFlag::Versions) != (conf.find_release_filter_flag & FilterFlag::Versions)) {
		bres |= true;
	}
	if ((CONF.find_release_filter_flag & FilterFlag::RoleMain) != (conf.find_release_filter_flag & FilterFlag::RoleMain)) {
		bres |= true;
	}

	//bind artist profile panel
	size_t attach_flagged = (IsDlgButtonChecked(IDC_CHK_RELEASE_SHOW_PROFILE) == BST_CHECKED) ?
		FLG_PROFILE_DLG_SHOW : 0;

	size_t open_flagged = g_discogs->find_release_artist_dialog ? FLG_PROFILE_DLG_ATTACHED : 0;

	conf.find_release_dlg_flags &= ~(FLG_PROFILE_DLG_SHOW | FLG_PROFILE_DLG_ATTACHED);
	conf.find_release_dlg_flags |= (attach_flagged | open_flagged);

	int mask = ~(~0 << 3);
	if ((CONF.find_release_dlg_flags & mask) != (conf.find_release_dlg_flags & mask)) {
		bres |= true;
	}

	return bres;
}

void CFindReleaseDialog::init_cfged_dialog_controls() {

	// TEXT BOX RETURN OVERRIDE

	set_enter_key_override(conf.release_enter_key_override);

	//INIT HISTORY, HISTORY OVERRIDE

	size_t max_items = LOWORD(conf.history_enabled_max);

	if (conf.history_enabled()) {
		m_oplogger.init(conf.history_enabled(), max_items);
	}

	//init artist profile panel
	if (conf.find_release_dlg_flags & flg_fr::FLG_PROFILE_DLG_SHOW) {

		uButton_SetCheck(m_hWnd, IDC_CHK_RELEASE_SHOW_PROFILE, true);

		if (conf.find_release_dlg_flags & flg_fr::FLG_PROFILE_DLG_ATTACHED) {

			if (!g_discogs->find_release_artist_dialog) {

				// new profile dlg
				g_discogs->find_release_artist_dialog = 
					fb2k::newDialog<CFindReleaseArtistDialog>(core_api::get_main_window(), SW_HIDE);
			}
		}
	}

	// tree stats

	if (conf.find_release_dlg_flags & flg_fr::FLG_SHOW_RELEASE_TREE_STATS) {
		print_root_stats(m_row_stats, false, true, CONF.awt_get_alt_mode());
	}
	else {
		pfc::string8 artist_name = m_alist.Get_Artist() ? m_alist.Get_Artist()->name : pfc::string8();
		pfc::string8 artist_id = m_alist.Get_Artist() ? m_alist.Get_Artist()->id : pfc::string8();
		rppair row_stats{ std::pair("",""), std::pair(artist_name, artist_id) };

		print_root_stats(row_stats, /*overwrite*/ !m_row_stats.second.first.get_length());
	}
}

// message map

LRESULT CFindReleaseDialog::OnButtonNext(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {

	pfc::string8 release_id;
	release_id = trim(uGetWindowText(m_edit_release).c_str());

	if (is_number(release_id.c_str()) || id_from_url(m_edit_release, release_id)) {

		trigger_release(release_id);

	}

	return TRUE;
}

// Open the selected artist's releases on a genuine list-row double-click,
// using the same path as Enter. The notification is sent to this dialog,
// not to the list's own WTL message map.
LRESULT CFindReleaseDialog::OnArtistListDoubleClick(
    int /*idCtrl*/, LPNMHDR hdr, BOOL& bHandled) {
    bHandled = FALSE;
    if (hdr == nullptr || hdr->hwndFrom != m_artist_list ||
        hdr->code != NM_DBLCLK)
        return 0;

    const auto* item = reinterpret_cast<const NMITEMACTIVATE*>(hdr);
    if (!discogger_ui::should_load_artist_on_double_click(
            item->iItem, m_alist.GetFirstSelected(),
            m_alist.Get_Artists().get_count()))
        return 0;

    bHandled = TRUE;
    m_alist.Default_Action();
    return 0;
}

//from Enter key hook

bool CFindReleaseDialog::ForwardVKReturn()
{
	HWND hwnd = GetFocus();

	if (hwnd == m_artist_list) {

		m_alist.Default_Action();
		return true;

	}
	else if (hwnd == m_release_tree) {

		m_dctree.vkreturn_test_master_expand_release();

		//want return
		return true;
	}
	return false;
}

bool OAuthCheck(const foo_conf& conf) {
#if defined(DISCOGGER_RUNNER_MEDIA_QA)
	// The protected Windows QA runner injects all four values in memory. The
	// disposable GUI may search without persisting tokens to Discogger config.
	const char* runner = std::getenv("GITHUB_ACTIONS");
	const char* key = std::getenv("DISCOGS_OAUTH_CONSUMER_KEY");
	const char* secret = std::getenv("DISCOGS_OAUTH_CONSUMER_SECRET");
	const char* access = std::getenv("DISCOGS_OAUTH_ACCESS_TOKEN");
	const char* access_secret = std::getenv("DISCOGS_OAUTH_TOKEN_SECRET");
	if (runner && std::strcmp(runner, "true") == 0 &&
		key && *key && secret && *secret && access && *access &&
		access_secret && *access_secret) return true;
#endif

	if (!g_discogs->gave_oauth_warning && (!conf.oauth_token.get_length() || !conf.oauth_token_secret.get_length())) {

		g_discogs->gave_oauth_warning = true;

		if (!g_discogs->configuration_dialog) {
			static_api_ptr_t<ui_control>()->show_preferences(guid_pref_page);
		}

		if (CConfigurationDialog* dlg = g_discogs->configuration_dialog) {
			dlg->show_oauth_msg("Please configure OAuth.", true);
			::SetFocus(dlg->m_hWnd);
		}
		return false;
	}
	return true;
}

// on init dialog

#define OVRCEDIT
#define VKHOOK


void CFindReleaseDialog::SetSearchMode(SearchMode sm) { m_query_mode = sm; }
size_t CFindReleaseDialog::GetSearchMode() { return m_query_mode; }

void CFindReleaseDialog::SetSearchModeFlag(SearchMode sm, bool enabled) {

	if (enabled) {
		m_query_mode |= sm;
	}
	else {
		m_query_mode &= ~sm;
	}

	if (m_query_mode) {
		conf.find_release_filter_flag |= FilterFlag::RoleMainAT;
	}
	else {
		conf.find_release_filter_flag &= ~FilterFlag::RoleMainAT;
	}
}

void CFindReleaseDialog::SetSearchModeText(const pfc::string8 msg, bool mode_prefix) {

	pfc::string8 buffer;

	if (mode_prefix) {
		if (msg.length()) {
			buffer << GetSearchModeMsg();
			buffer << ": ";
			buffer << msg;
		}
	}
	else {
		buffer = msg;
	}

    const HWND status_window = m_static_search_msg;
    fb2k::inMainThread([status_window, buffer] {
        if (::IsWindow(status_window) &&
            ::GetDlgCtrlID(status_window) == IDC_STATIC_FIND_REL_SEARCH_STATS)
            uSetWindowText(status_window, buffer);
    });
}

pfc::string8 CFindReleaseDialog::GetSearchModeMsg() {

	pfc::string8 msg;
	if (m_query_mode == SearchMode::DEFAULT_SEARCH) {
		msg = "";
	}
	else {
		if (m_query_mode & SearchMode::AT) {
			msg = "Query results";
			if (m_query_mode & SearchMode::VA) {
				msg = "Query results (VA)";
			}
		}
		else if (m_query_mode & SearchMode::VA) {
			//todo
			msg = "VA album artist";
			if (!m_tracer.has_master()  && !m_tracer.has_release()) {
				msg << ", try searching for 'artist=various && title= the title'";
			}
		}
	}

	return msg;
}

bool CFindReleaseDialog::GetCustomQueryTF(pfc::string8& frm_custom_tf) {

	pfc::string8 custom_tf;
	if (conf.on_init_query_tf.get_length()) {

		//script formatter

		file_info_impl info;
		m_items[0]->get_info(info);
		fake_threaded_process_status fake_status;
		titleformat_hook_impl_multiformat hook(fake_status, nullptr, nullptr, nullptr, nullptr);
		hook.set_va_csv(conf.various_prefixes);

		//

		try {
			m_query_custom_script->run_hook(m_items[0]->get_location(), &info, &hook, custom_tf, nullptr);
		}
		catch (...) {
			pfc::string8 msg("Error running custom search query title-format: ");
			msg << conf.on_init_query_tf;
			log_msg(msg);
			return false;
		}

		QueryDefMap qdm_temp;
		if (search_query::TextToMap(custom_tf, qdm_temp)) {
			pfc::string8 chk_artist_va(qdm_temp.at("artist=").first.c_str());
			bool bsani = search_query::sanitize_various_artists_with_csv(conf.various_prefixes.c_str(), chk_artist_va);
			if (bsani) {
				qdm_temp.at("artist=").first = chk_artist_va;
				qdm_temp.at("artist=").second = chk_artist_va;
				std::pair<std::string, std::string> exp_pair = {};
				search_query::MapToText(qdm_temp, exp_pair, false);
				custom_tf = exp_pair.first.c_str();
			}
		}
		frm_custom_tf = custom_tf;
		return true;

	}
	return false;
}

void CFindReleaseDialog::UpdateSearchModeDisplayMode(const char* artist) {

	bool b_AT = (bool)(m_query_mode & SearchMode::AT);

	::EnableWindow(m_chkbox_rolemain, !b_AT);
	if (b_AT && m_search_expression.get_length()) {
		uSetWindowText(m_edit_artist, m_search_expression);
	}
	else if ((m_query_mode & SearchMode::VA) && m_search_expression.get_length()) {
		uSetWindowText(m_edit_artist, m_search_expression);
	}
	else {
		if (m_search_expression.get_length()) {
			uSetWindowText(m_edit_artist, m_search_expression);
		}
		else {
			if (artist) {
				uSetWindowText(m_edit_artist, artist);
			}
			else {
				//..
			}
		}
	}
		uSetWindowText(m_static_search_msg, GetSearchModeMsg());
}

void CFindReleaseDialog::InvalidateSearchDisplay(bool list_tree) {

	bool b_AT = (bool)(m_query_mode & SearchMode::AT);
	::EnableWindow(m_chkbox_rolemain, !b_AT);

	::InvalidateRect(m_edit_artist, NULL, TRUE);
	::InvalidateRect(m_static_search_msg, NULL, TRUE);
	::InvalidateRect(m_chkbox_rolemain, NULL, TRUE);
	if (list_tree) {
		::InvalidateRect(m_artist_list, NULL, TRUE);
		::InvalidateRect(m_release_tree, {0}, TRUE);
	}
}

void CFindReleaseDialog::UpdateSearchQueryHasRun(const pfc::string8 expression) {

	m_search_expression = expression;

	m_qdm_search_query = {};
	search_query::TextToMap(expression, m_qdm_search_query);
}

void CFindReleaseDialog::UpdateSearchArtistHasRun(const pfc::string8 artist) {

	m_search_artist = artist;

}

void CFindReleaseDialog::GetCurrentExpression(pfc::string8& out) {
	out = m_search_expression;
}

void CFindReleaseDialog::GetCurrentSearchArtist(pfc::string8& out) {
	out = m_search_artist;
}


//

//     O N    I N I T   

//


LRESULT CFindReleaseDialog::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
#if defined(DISCOGGER_RUNNER_MEDIA_QA)
	discogger_live_search_qa::reset(); // New native dialog, not each search invocation.
#endif

	SetIcon(g_discogs->icon);

	enable_alt(CONF.awt_get_alt_mode());

	if (CONF.awt_get_alt_mode()) {
		m_edit_release = GetDlgItem(IDC_RELEASE_MINI_URL_TEXT);
	}
	else {
		m_edit_release = GetDlgItem(IDC_RELEASE_URL_TEXT);
	}

	m_edit_artist = GetDlgItem(IDC_EDIT_SEARCH);
	m_edit_filter = GetDlgItem(IDC_EDIT_FILTER);
	m_release_tree = GetDlgItem(IDC_RELEASE_TREE);

	m_static_search_msg = GetDlgItem(IDC_STATIC_FIND_REL_SEARCH_STATS);

	m_chkbox_rolemain = GetDlgItem(IDC_CHK_FIND_RELEASE_FILTER_ROLEMAIN);

	ceqwb_artist_search.SubclassWindow(m_edit_artist);
	cewb_release_filter.SubclassWindow(m_edit_filter);
	cewb_release_url.SubclassWindow(m_edit_release);

	set_history_key_override();

	//fix wine light theme
	set_conditioned_invalidation();

	cewb_release_filter.SetEnterEscHandlers();
	ceqwb_artist_search.SetEnterEscHandlers();
	cewb_release_url.SetEnterEscHandlers();

	COLORREF link_color = get_hyper_link_color(m_dark.IsDark());

	//first override ext. style
	m_artist_link.SetHyperLinkExtendedStyle(0, HLINK_UNDERLINED);
	m_artist_link.SetHyperLinkExtendedStyle(HLINK_NOTUNDERLINED, HLINK_NOTUNDERLINED);
	m_artist_link.SetHyperLinkExtendedStyle(HLINK_SINGLELINE, HLINK_SINGLELINE);
	m_artist_link.SetHyperLinkExtendedStyle(HLINK_NOTOOLTIP, HLINK_NOTOOLTIP);
	//then subclass
	m_artist_link.SubclassWindow(GetDlgItem(IDC_STATIC_FIND_REL_STATS));
	//then set colors
	m_artist_link.m_clrLink = link_color;
	m_artist_link.m_clrVisited = link_color;

	// init tracer

	metadb_handle_ptr item = m_items[0];
	m_tracer.init_tracker_tags(m_items);

	// track album, artist, album artist and track title

	item->format_title(nullptr, m_frm_artist, m_artist_name_script, nullptr);
	item->format_title(nullptr, m_frm_album_artist, m_album_artist_script, nullptr);
	item->format_title(nullptr, m_frm_album, m_album_name_script, nullptr);
	item->format_title(nullptr, m_frm_track_title, m_track_title_script, nullptr);

	pfc::string8 bk_frm_album = m_frm_album;
	pfc::string8 bk_frm_track_title = m_frm_track_title;

	// todo: clean up and move all query related calculations to a function

	// CUSTOM QUERY TITLE FORMAT

	pfc::string8 frm_custom_tf;
	GetCustomQueryTF(frm_custom_tf);

	bool skip_all = false;

	// On init query enabled
	bool cfg_on_init_query_enabled = conf.on_init_query_flags & InitQueryMng::INIT_QUERY_FLAG_ENABLED;
	bool cfg_on_init_query_artist_as_credit = conf.on_init_query_flags & InitQueryMng::INIT_QUERY_ART_AS_CRED_IN_VA;
	bool cfg_on_init_query_custom_tf_enabled = conf.on_init_query_flags & InitQueryMng::INIT_QUERY_CUST_TF_ENABLED;

	// VA autoload (requires artist ids)
	bool bva_auto_load = !(conf.skip_mng_flag & SkipMng::RELEASE_DLG_VA_AUTO_LOAD);
	bva_auto_load &= m_tracer.is_multi_artist(); // bva_in_frm_title;

	//init tree filter options - versions and main role checkboxes

	if (conf.find_release_filter_flag & FilterFlag::Versions) {
		uButton_SetCheck(m_hWnd, IDC_CHK_FIND_RELEASE_FILTER_VERS, true);
	}
	if (conf.find_release_filter_flag & FilterFlag::RoleMain) {
		uButton_SetCheck(m_hWnd, IDC_CHK_FIND_RELEASE_FILTER_ROLEMAIN, true);
	}

	// album artist or artist name

	const char* artist = (m_frm_album_artist.get_length() ? m_frm_album_artist.c_str() :
		m_frm_artist.get_length() ? m_frm_artist.c_str() : "");

	pfc::string8 strArtistFromTitle;
	pfc::string8 strTitleFromTitle;

	pfc::string8 pfx = has_csv_prefix(m_frm_album_artist, conf.various_prefixes, true, false);
	if (pfx.get_length()) {
		m_frm_album_artist.replace(pfx, "various");
	}
	else {
		pfx = has_csv_prefix(m_frm_album_artist, conf.various_prefixes, false, true);
		if (pfx.get_length()) m_frm_album_artist.replace(pfx, "various");
	}

	bool done = search_query::sanitize_various_artists_with_csv(conf.various_prefixes.c_str(), m_frm_album_artist);
	pfc::string8 frm_sani_artist(m_frm_artist);
	done = search_query::sanitize_various_artists_with_csv(conf.various_prefixes.c_str(), m_frm_artist);

	pfc::string8 out;
	pfc::string8 tmp_edit_artist_route = m_frm_album_artist;
	pfc::string8 tmp_edit_artist = m_frm_album_artist;
	pfc::string8 tmp_edit_artist_no_query = m_frm_album_artist;

	std::pair<std::string, std::string> exp_pair ={};
	std::pair<std::string, std::string> exp_pair_all = {};

	bool b_is_various = m_frm_album_artist.equals("various");

	if (cfg_on_init_query_enabled && !bva_auto_load) {

		bool bres = false;

		if (cfg_on_init_query_custom_tf_enabled) {
			out = frm_custom_tf;
			bres = true;
		}
		else {
			//todo:
			size_t def_ndx = conf.on_init_query_def;
			bres = search_query::GetSuggestion(m_frm_album_artist, m_frm_artist, m_frm_album, m_frm_track_title, frm_sani_artist, "", def_ndx, out);
		}

		m_qdm_search_query.clear();
		search_query::TextToMap(out, m_qdm_search_query);

		if (bres) {

			if (b_is_various && !cfg_on_init_query_custom_tf_enabled) {
				if (m_qdm_search_query.at("artist=").first.size()) {

					QueryDefMap qdm_temp;
					search_query::TextToMap(out, qdm_temp);

					// swap artist to credit if artist is not a va polizone?

					pfc::string8 pfx_va_polyzone;
					pfx_va_polyzone = has_csv_prefix(m_frm_artist, conf.various_prefixes, true, false);
					if (!pfx_va_polyzone.get_length())
						pfx_va_polyzone = has_csv_prefix(m_frm_artist, conf.various_prefixes, true, true);

					if (!pfx_va_polyzone.get_length()) {
						qdm_temp.at("credit=").first = m_frm_artist;
						qdm_temp.at("credit=").second = urlEscape(m_frm_artist);
					}

					//diff 
					bres = search_query::MapToText(qdm_temp, exp_pair, false);
					m_qdm_search_query.clear();
					search_query::TextToMap(exp_pair.first.c_str(), m_qdm_search_query);
				}
				else {
					if (!m_qdm_search_query.at("title=").first.size()) {
						m_qdm_search_query.at("artist=").first = "various";
						m_qdm_search_query.at("artist=").second = "various";
					}
				}
			}
		}

		if (bres) {
			if (search_query::IsMinimal(m_qdm_search_query)) {
				search_query::MapToText(m_qdm_search_query, exp_pair_all, true);
				tmp_edit_artist_route = exp_pair_all.second.c_str();
				search_query::MapToText(m_qdm_search_query, exp_pair, false);
				tmp_edit_artist = exp_pair.first.c_str();
			}
		}

		SetSearchModeFlag(SearchMode::AT, search_query::IsMinimal(m_qdm_search_query)/*IsRunnable(tmp_edit_artist)*/);

	}
	else {
		//...
	}

	SetSearchModeFlag(SearchMode::VA_AUTO_LOAD, bva_auto_load);
	SetSearchModeFlag(SearchMode::VA, b_is_various);

	//init artist search/release ID textboxes

	pfc::string8 rel_id;

	if (CONF.awt_get_alt_mode()) {

		file_info_impl finfo;
		metadb_handle_ptr item = m_items[0];
		item->get_info(finfo);

		pfc::string8 id;
		bool bwid = g_discogs->file_info_get_tag(item, finfo, "WWW", id);

		if (m_tracer.has_release()) {
			if (bwid) {
				rel_id = id;
			}
			else {
				id << m_tracer.release_id;
			}
		}
		else
		{
			if (bwid) {
				rel_id = id;
				if (id_from_url(m_edit_release, id)) {
					m_tracer.release_id = std::atoi(id);
					m_tracer.release_tag = true;
					uSetWindowText(m_edit_release, rel_id);
				}
			}
		}

		if (bwid) {
			rppair rpp(std::pair("", ""), std::pair(rel_id, rel_id));
			print_root_stats(rpp, false, false, true);
		}
	}
	else {
		if (m_tracer.has_release()) {
			rel_id << m_tracer.release_id;
		}
	}
	// set dlg title
	uSetWindowText( m_hWnd, bk_frm_album.get_length() ? bk_frm_album.get_ptr() : "Find releases");

	if (cfg_on_init_query_enabled && m_query_mode & SearchMode::AT) {
		search_query::MapToText(m_qdm_search_query, exp_pair, false);
		uSetWindowText(m_edit_artist, exp_pair.first.c_str()); 
	}
	else {
		uSetWindowText(m_edit_artist, tmp_edit_artist_no_query);
	}

	// dlg resize and dimensions

	DlgResize_Init(mygripp.enabled, true);

	if (CONF.awt_get_alt_mode()) {

		cfg_dialog_position_find_release_mini_dlg.AddWindow(m_hWnd);
	}
	else {
		cfg_dialog_position_find_release_dlg.AddWindow(m_hWnd);
	}

	HWND wndReplace = ::GetDlgItem(m_hWnd, IDC_ARTIST_LIST);
	m_alist.CreateInDialog(*this, IDC_ARTIST_LIST, wndReplace);
	m_alist.InitializeHeaderCtrl(HDS_HIDDEN);

	m_artist_list = m_alist.m_hWnd;

	if (CONF.awt_get_alt_mode()) {
		::ShowWindow(m_alist, SW_HIDE);
	}
	else {
		m_alist.Inititalize();
		m_dctree.Inititalize(m_release_tree, m_edit_filter, m_edit_release, m_items, m_frm_album /*ALBUM*/);
	}

	SetWindowSubclass(m_artist_list, EnterKeySubclassProc, 0, 0);
	SetWindowSubclass(m_release_tree, EnterKeySubclassProc, 0, 0);

	// dark mode

	init_cfged_dialog_controls();

	m_dark.AddDialog(m_hWnd);
	m_dark.AddControls(m_hWnd);

	CustomFont(m_hWnd, HIWORD(conf.custom_font));
	Invalidate(true);

	{
		bool brelease_ided = m_tracer.release_tag && m_tracer.release_id != pfc_infinite;
		bool bskip_ided = conf.skip_mng_flag & SkipMng::RELEASE_DLG_IDED;

		bool cfg_always_load_artist_ided_preview = /*force*/true && !CONF.awt_get_alt_mode();

		if (CONF.awt_get_alt_mode()) {
			//do not route artist
		}
		else {

			bool broute = false;
			bool broute_idded = false;

			if (!(bskip_ided && brelease_ided)) {

				//route
				broute = true;
				broute_idded = m_tracer.artist_tag;
			}
			else 
			{
				if (cfg_always_load_artist_ided_preview && m_tracer.artist_tag) {

					//route
					broute = true;
					broute_idded = true;
				}
			}

			bool va_no_id = m_query_mode & SearchMode::VA && !broute_idded && !search_query::isVAQueryMinimal(m_qdm_search_query);
			if (va_no_id) {
				SetSearchModeText("Use query syntax 'artist= various && title= ...' or enter an individual artist/appearance.", false);
			}

			//do not route 'VA without ids
			broute &= !va_no_id;

			if (broute) {
				//todo_ rev tmp
				pfc::string8 artist_meta;
				if (tmp_edit_artist_route.get_length()) {
					artist_meta = tmp_edit_artist_route;
				}
				else {
					artist_meta = trim(uGetWindowText(m_edit_artist));
				}

				if (artist_meta.get_length()) {
					route_artist_search(artist_meta, false, broute_idded);
				}
			}
		}
	}

	//..

	//album filter textbox

	block_filter_box_events(true);

	uSetWindowText(m_edit_filter, strTitleFromTitle.get_length() ? strTitleFromTitle : m_frm_album);

	block_filter_box_events(false);

	if (OAuthCheck(conf)) {

		bool brelease_ided = m_tracer.release_tag && m_tracer.release_id != pfc_infinite;
		bool bskip_ided = conf.skip_mng_flag & SkipMng::RELEASE_DLG_IDED;

		//autofill release id
		if (!CONF.awt_get_alt_mode()) {
			pfc::string8 rel_id;
			if (brelease_ided) {
				rel_id << m_tracer.release_id;
			}
			uSetWindowText(m_edit_release, rel_id);
		}

		if (!CONF.awt_get_alt_mode() && bskip_ided && brelease_ided) {

			//.. skip find release dialog
			trigger_release(std::to_string(m_tracer.release_id).c_str());

			//..

			return FALSE;
		}
		else {

			show();

			//default buttons

			UINT default_button = 0;
			if (m_tracer.has_amr() || m_tracer.release_tag) {
				default_button = IDC_BTN_PROCESS_RELEASE;
			}
			else if (!artist) {
				default_button = IDC_EDIT_SEARCH;
			}
			else if (artist) {
				default_button = IDC_BTN_SEARCH;
			}

			if (default_button) {
				uSendMessage(m_hWnd, WM_NEXTDLGCTL, (WPARAM)(HWND)GetDlgItem(default_button), TRUE);
			}
			else {
				uSendMessage(m_hWnd, WM_NEXTDLGCTL, (WPARAM)(HWND)cewb_release_filter, TRUE);
			}
		}
	}
	else {
		// OAuthCheck failed
		show();

	}

	//prevent default control focus
	return FALSE;
}

LRESULT CFindReleaseDialog::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {

	pushcfg();
	if (g_discogs) {
		//destroy artist panel instance
		if (g_discogs->find_release_artist_dialog
			&& (conf.find_release_dlg_flags & (FLG_PROFILE_DLG_SHOW | FLG_PROFILE_DLG_ATTACHED)) == (FLG_PROFILE_DLG_SHOW | FLG_PROFILE_DLG_ATTACHED)) {
			g_discogs->find_release_artist_dialog->DestroyWindow();
		}
	}

	if (!CONF.awt_get_alt_mode()) {
		cfg_dialog_position_find_release_dlg.RemoveWindow(m_hWnd);
	}
	else {
		cfg_dialog_position_find_release_mini_dlg.RemoveWindow(m_hWnd);
	}

	KillTimer(KTypeFilterTimerID);

	return FALSE;
}

LRESULT CFindReleaseDialog::OnButtonSearch(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
#if defined(DISCOGGER_RUNNER_MEDIA_QA)
	discogger_live_search_qa::button.fetch_add(1);
	const HWND live_edit = ::GetDlgItem(m_hWnd, IDC_EDIT_SEARCH);
	discogger_live_search_qa::input_member_valid.store(::IsWindow(m_edit_artist) ? 1u : 0u);
	discogger_live_search_qa::input_hwnd_match.store(live_edit == m_edit_artist ? 1u : 0u);
	discogger_live_search_qa::input_member_length.store(static_cast<unsigned>(::GetWindowTextLengthW(m_edit_artist)));
	discogger_live_search_qa::input_live_length.store(static_cast<unsigned>(::GetWindowTextLengthW(live_edit)));
#endif

	for (const std::string fld : kv_query_fields_ordered) {

		m_qdm_search_query[fld] = {};

	}

	//todo: move search query related 
	pfc::string8 trot_text = trim(uGetWindowText(m_edit_artist));
#if defined(DISCOGGER_RUNNER_MEDIA_QA)
	discogger_live_search_qa::input_nonempty.store(trot_text.get_length() ? 1u : 0u);
	if (trot_text.get_length()) discogger_live_search_qa::input_nonempty_calls.fetch_add(1);
	else discogger_live_search_qa::input_empty_calls.fetch_add(1);
#endif

	bool bva_in_artist = false;
	bva_in_artist |= pfc::stringLite::g_equalsCaseInsensitive(trot_text, "various");
	bva_in_artist |= pfc::stringLite::g_equalsCaseInsensitive(trot_text, "various artists");

	if (!trot_text.get_length()) {
#if defined(DISCOGGER_RUNNER_MEDIA_QA)
		discogger_live_search_qa::early_exit.store(1);
#endif
		//..
		return FALSE;
		//..
	}

	bool bid = false;

	if (bool bquery_from_url = search_query::UrlToMap(trot_text, m_qdm_search_query)) {
		std::pair<std::string, std::string> exp_pair;
		search_query::MapToText(m_qdm_search_query, exp_pair, false);
		trot_text = exp_pair.first.c_str();
		uSetWindowText(m_edit_artist, trot_text);
	}

	m_qdm_search_query = {};
	bool bquery = search_query::TextToMap(trot_text, m_qdm_search_query);
	bquery &= search_query::IsMinimal(m_qdm_search_query);
#if defined(DISCOGGER_RUNNER_MEDIA_QA)
	discogger_live_search_qa::parsed_fields.store(static_cast<unsigned>(search_query::CountAvailableFields(m_qdm_search_query)));
	discogger_live_search_qa::query_minimal.store(search_query::IsMinimal(m_qdm_search_query) ? 1u : 0u);
	discogger_live_search_qa::query_detected.store(bquery ? 1u : 0u);
	discogger_live_search_qa::alt_mode.store(CONF.awt_get_alt_mode() ? 1u : 0u);
#endif

	bool bquery_type_uncompleted = search_query::CountAvailableFields(m_qdm_search_query) >= 1 && !search_query::IsMinimal(m_qdm_search_query);

	if (bquery_type_uncompleted) {

		pfc::string8 hint = search_query::IsMinimalHint(m_qdm_search_query);
		SetSearchModeText(hint, false);
#if defined(DISCOGGER_RUNNER_MEDIA_QA)
		discogger_live_search_qa::early_exit.store(2);
#endif

		//..

		return FALSE;

		//..
	}

	//todo

	m_query_mode = 0;

	pfc::string8 qartist = m_qdm_search_query.at("artist=").first.c_str();
	bool bqdm_various = pfc::stringLite::g_equalsCaseInsensitive("various", qartist);

	if (bva_in_artist && !bqdm_various) {

		SetSearchModeText("Use query syntax 'artist= various && title= ...' or enter an individual artist / appearance.", false);
#if defined(DISCOGGER_RUNNER_MEDIA_QA)
		discogger_live_search_qa::early_exit.store(3);
#endif

		//..
		return FALSE;
		//..
	}

	SetSearchModeFlag(SearchMode::VA, bqdm_various);
	SetSearchModeFlag(SearchMode::AT, search_query::IsMinimal(m_qdm_search_query));


	if (bquery) {

		std::pair<std::string, std::string> exp_pair;
		bool bres = search_query::MapToText(m_qdm_search_query, exp_pair, false, false);

		if (!exp_pair.first.compare(m_search_expression)) {

			log_msg("Query explicitly retried.");
#if defined(DISCOGGER_RUNNER_MEDIA_QA)
			discogger_live_search_qa::repeat_query.store(1);
#endif
		}

		if (bres) {

			// The API has no need for the GUI-only default type=all filter.
			search_query::MapToText(m_qdm_search_query, exp_pair, false, false);
			pfc::string8 artist_meta_scaped = exp_pair.second.c_str();

			trot_text = artist_meta_scaped;

			if (CONF.awt_get_alt_mode()) {
#if defined(DISCOGGER_RUNNER_MEDIA_QA)
				discogger_live_search_qa::early_exit.store(5);
#endif

				//..

				return FALSE;

				//..
			}
		}
	}
	else {

		if (trot_text.equals(m_search_artist)) {

			log_msg("Artist search explicitly retried.");
#if defined(DISCOGGER_RUNNER_MEDIA_QA)
			discogger_live_search_qa::repeat_query.store(1);
#endif
		}

		m_search_expression = "";
		bid = id_from_url(m_edit_artist, trot_text);

	}

	if (trot_text.get_length()) {

		// hint display the preffix 'Query results'
		SetSearchModeText("");

		#if defined(DISCOGGER_RUNNER_MEDIA_QA)
		discogger_live_search_qa::routed.store(1);
		#endif
		route_artist_search(trot_text, true, bid);

	}
	else {

		SetSearchModeText("to check syntax, right click on this message for examples.");
#if defined(DISCOGGER_RUNNER_MEDIA_QA)
		discogger_live_search_qa::early_exit.store(7);
#endif

	}
	return FALSE;
}

void CFindReleaseDialog::on_get_artist_done(cupdRelSrc updsrc, Artist_ptr& artist) {

	if (updsrc != updRelSrc::ArtistSearchAT && updsrc != updRelSrc::Undef) {

		auto list_artist_p = m_alist.Get_Selected_Id();
		bool selection_ok = !list_artist_p.get_length() || list_artist_p.equals(artist->id);

		if (!selection_ok) {
			//..
			return;
			//..
		}
	}

	m_alist.on_get_artist_done(updsrc, artist);

	if (!m_query_mode && !artist.get()) {
		return;
	}

	cupdRelSrc cupdsrc = updsrc;
	if (cupdsrc == updRelSrc::ArtistProfile) {
		cupdsrc.extended |= conf.auto_load_releases_on_select_ready()/*ol::full_cache() && conf.auto_rel_load_on_select*/;
	}

	if (cupdsrc == updRelSrc::ArtistSearchAT || cupdsrc.oninit || cupdsrc == updRelSrc::ArtistList || (!cupdsrc.oninit && cupdsrc == updRelSrc::ArtistProfile && cupdsrc.extended)) {

		{
			std::lock_guard<std::mutex> guard(g_discogs->tree_locked_mutex);
			m_dctree.EnableDispInfo(false);
		}

		m_dctree.on_get_artist_done(cupdsrc, artist);

		{
			std::lock_guard<std::mutex> guard(g_discogs->tree_locked_mutex);
			m_dctree.EnableDispInfo(true);
		}

		if (conf.auto_rel_load_on_open || cupdsrc.extended) {
			if ((cupdsrc == updRelSrc::ArtistList || updsrc == updRelSrc::UndefFast) && m_dctree.Get_Size() && m_tracer.has_master()) {
				//..
				m_dctree.OnInitExpand(mounted_param(m_tracer.master_i, ~0, true, false).lparam());
				//..
			}
		}

	}
}

pfc::string8 search_query_run_message(const QueryDefMap qdm_search_query, const size_t m_query_mode, const std::pair<size_t, size_t>& out_va_cap, bool no_matches) {

	pfc::string8 msg;
	if (no_matches) {
		if (!out_va_cap.first && !out_va_cap.second) {
			msg = "no entries found.";
			if (m_query_mode & SearchMode::AT) {
                // Restored/interrupted queries may not have a complete field map.
                // Rendering "no entries found" must never throw a map exception.
                const auto artist_field = qdm_search_query.find("artist=");
                if (artist_field != qdm_search_query.end()) {
                    pfc::string8 lwc = artist_field->second.first.c_str();
                    if (lwc.toLower().equals("various artists"))
                        msg << " Hint: use 'artist= various' for VA compilations.";
                }
			}
		}
		else if (out_va_cap.first < (SIZE_MAX - 100) && out_va_cap.second < (SIZE_MAX - 100)) {
			msg = "max reached (";
			msg << out_va_cap.first << "). Please, add more specific keywords.";
		}
	}
	else {
		if (!out_va_cap.first && !out_va_cap.second) {
			//ok
		}
		else if (out_va_cap.first < HIWORD(CONF.query_max)/*kmax_items_q_results*/ || out_va_cap.second < HIWORD(CONF.query_max)/*kmax_items_q_results*/) {

			//todo: rev stats
            // sizeof(size_t) counts bytes, not the numeric maximum.
            // SIZE_MAX is the caller's uncapped-result sentinel.
            constexpr size_t uncapped_threshold = SIZE_MAX - 100;
            if (out_va_cap.first < uncapped_threshold && out_va_cap.second < uncapped_threshold) {
				//limitted results
				msg = "max reached.";
			}
			else {
				//todo: rev stats
			}
		}
		else {
			//..
		}
	}
	return msg;
}

void CFindReleaseDialog::on_search_artist_done(const pfc::array_t<Artist_ptr>& p_artist_exact_matches, const pfc::array_t<Artist_ptr>& p_artist_other_matches, bool append, std::pair<size_t, size_t>& out_va_cap) {

    // Search providers may return an allocated artist with an empty ID.
    // It is not a selectable Discogs identity: counting it as an exact hit
    // hides real alternative results and exposes a bogus artist row. Filter
    // only this incoming delta, preserving cumulative valid prior buckets.
    const auto valid_rows = [](const pfc::array_t<Artist_ptr>& rows) {
        pfc::array_t<Artist_ptr> result;
        for (const auto& artist : rows)
            if (artist && artist->id.get_length())
                result.append_single(artist);
        return result;
    };
    const auto valid_exact = valid_rows(p_artist_exact_matches);
    const auto valid_other = valid_rows(p_artist_other_matches);
    const bool exact_only_requested =
        IsDlgButtonChecked(IDC_CHK_ONLY_EXACT_MATCHES) == BST_CHECKED;
    // Append callbacks carry only a delta, not the entire result set.
    // Resolve exact-only visibility and the no-match message against the
    // cumulative buckets, otherwise a late alternative silently unchecks
    // an exact-only filter even while exact matches are still present.
    const auto valid_count = [](const pfc::array_t<Artist_ptr>& artists) {
        size_t count = 0;
        for (const auto& artist : artists)
            if (artist) ++count;
        return count;
    };
    const size_t incoming_exact = valid_count(valid_exact);
    const size_t incoming_other = valid_count(valid_other);
    const size_t total_exact = incoming_exact +
        (append ? m_alist.Get_Exact_Artist_Count() : 0);
    const size_t total_other = incoming_other +
        (append ? m_alist.Get_Other_Artist_Count() : 0);
    const auto decision = discogger_search::resolve_result_filter(
        exact_only_requested, total_exact, total_other);
    const bool b_chk_exact_matches = decision.exact_only;
    if (decision.show_alternatives && exact_only_requested) {
        // The effective list filter must match the visible checkbox.
        CheckDlgButton(IDC_CHK_ONLY_EXACT_MATCHES, BST_UNCHECKED);
    }

    const bool no_matches = !(total_exact || total_other);
    pfc::string8 msg = search_query_run_message(
        m_qdm_search_query, m_query_mode, out_va_cap, no_matches);
    if (decision.show_alternatives)
        msg << " No exact artist match; alternative releases require review.";
    SetSearchModeText(msg);

	{
		std::lock_guard<std::mutex> guard(g_discogs->tree_locked_mutex);
		m_dctree.EnableDispInfo(true);
		//todo:
        const HWND tree_window = m_release_tree;
        fb2k::inMainThread([tree_window] {
            if (::IsWindow(tree_window) &&
                ::GetDlgCtrlID(tree_window) == IDC_RELEASE_TREE)
                ::InvalidateRect(tree_window, nullptr, TRUE);
        });
	}

	if (m_tracer.is_multi_artist()) {
		m_alist.set_artists(b_chk_exact_matches, append, nullptr, valid_exact, valid_other);
	}
	else {
		m_alist.set_artists(b_chk_exact_matches, append, nullptr, valid_exact, valid_other);
	}

	//may spawn on list item selection

	// The old debug_ok branch was permanently disabled and had no declaration.
	updRelSrc updrelsrc = updRelSrc::ArtistSearch;

	if (!append) {
		m_alist.fill_artist_list(b_chk_exact_matches, m_tracer.has_amr(), updrelsrc/*updRelSrc::ArtistSearch*/);

		if (!m_alist.Get_Artists().get_count()) {
			cupdRelSrc cupdsrc(updRelSrc::Undef);
			//..
			m_dctree.on_get_artist_done(cupdsrc, nullptr);
			//..
		}
	}

    // Late advanced-search pages must also appear in the release tree
    // without a second artist click. Never replace a tree that the user
    // explicitly opened by selecting an artist while the worker was active.
    // Empty append notifications are no-ops for the preview.
    const bool incoming_results = incoming_exact || incoming_other;
    const bool update_preview = (m_query_mode & SearchMode::AT) &&
        (!append || (incoming_results && !m_alist.Get_Selected_Id().get_length()));
    if (update_preview) {
        const auto artists = m_alist.Get_Artists();
        if (artists.get_count() == 1) {
            const Artist_ptr& only = artists[0];
            if (only && (only->releases.get_count() || only->master_releases.get_count()))
                ShowArtistSearchResults(only);
        } else if (artists.get_count() > 1) {
            Artist_ptr preview = combined_advanced_search_preview(artists);
            if (preview->releases.get_count() || preview->master_releases.get_count())
                ShowArtistSearchResults(preview);
        }
    }

	std::lock_guard<std::mutex> guard(g_discogs->tree_locked_mutex);
	m_dctree.EnableDispInfo(true);
    const HWND tree_window = m_release_tree;
    fb2k::inMainThread([tree_window] {
        if (::IsWindow(tree_window) &&
            ::GetDlgCtrlID(tree_window) == IDC_RELEASE_TREE)
            ::InvalidateRect(tree_window, nullptr, TRUE);
    });
}

void CFindReleaseDialog::OnTypeFilterTimer(WPARAM id) {

	if (id == KTypeFilterTimerID) {

		switch (++m_tickCount) {
		case 1:
			break;
		case 2:
			break;
		case 3:

			pfc::string8 strFilter = trim(uGetWindowText(m_edit_filter));
				if (!is_filter_autofill_enabled() && strFilter.get_length()) {
					rppair row;
					row.first.first = strFilter;
					try {
						add_history(oplog_type::filter, kHistoryFilterButton, row);
					}
					catch (foo_discogs_exception e) {
						pfc::string8 error;
						error << ("(skipped) ") << e.what() << "\n";
						completion_notify::ptr reply;
						popup_message_v3::query_t query = { "Information",
							error.c_str(), popup_message::icon_error,
							popup_message_v3::buttonOK,
							popup_message_v3::iconQuestion,
							reply
						};

						popup_message_v3::get()->show_query_modal(query);

					}
				}

				// launch apply filter

				::ShowWindow(m_release_tree, SW_SHOWNOACTIVATE);
				//..
				service_ptr_t<tree_apply_filter_process_callback> task =
					new service_impl_t<tree_apply_filter_process_callback>(nullptr, 0, "", GetSearchMode(),
						strFilter, false, false);

				task->start(m_hWnd);
				//..
				KillTimer(KTypeFilterTimerID);
				break;
			}
		}
}

void CFindReleaseDialog::apply_filter(pfc::string8 strFilter, bool force_redraw, bool force_rebuild, threaded_process_status& p_status, abort_callback& p_abort) {

	// forwarded apply_filter
	{
		std::lock_guard<std::mutex> guard(g_discogs->tree_locked_mutex);
		m_dctree.EnableDispInfo(false);

		bool rolemainAT = m_query_mode & SearchMode::AT || m_query_mode & SearchMode::VA;
		bool brolemain = !rolemainAT && conf.find_release_filter_flag & FilterFlag::RoleMain;

		m_dctree.apply_filter(strFilter, brolemain, force_redraw, force_rebuild, p_status, p_abort);
		m_dctree.EnableDispInfo(true);
	}

}

LRESULT CFindReleaseDialog::OnEditFilter(WORD wNotifyCode, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {

	if (is_filter_box_event_enabled() && wNotifyCode == EN_CHANGE) {

		//supress auto-fill filter box on user interaction

		block_autofill_release_filter(true);

		pfc::string8 buffer = trim(uGetWindowText(m_edit_filter));

		if (buffer.length()) {
			KFilterTimerOn(m_hWnd);
		}
		else {

			KillTimer(KTypeFilterTimerID);

			// launch apply filter

			::ShowWindow(m_release_tree, SW_SHOWNOACTIVATE);
			service_ptr_t<tree_apply_filter_process_callback> task =
				new service_impl_t<tree_apply_filter_process_callback>(nullptr, 0, "", GetSearchMode(), 
					buffer, false, false);
			task->start(m_hWnd);

		}
	}
	return false;
}


LRESULT CFindReleaseDialog::OnCheckboxBindProfilePanel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {

	bool state = IsDlgButtonChecked(IDC_CHK_RELEASE_SHOW_PROFILE);

	conf.find_release_dlg_flags =
		state ?
			conf.find_release_dlg_flags |  flg_fr::FLG_PROFILE_DLG_ATTACHED
		:	conf.find_release_dlg_flags & ~flg_fr::FLG_PROFILE_DLG_ATTACHED;

	if (state && !g_discogs->find_release_artist_dialog)
		m_alist.ShowArtistProfile();

	return FALSE;
}

LRESULT CFindReleaseDialog::OnCheckboxOnlyExactMatches(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {

	if (wID == IDC_CHK_ONLY_EXACT_MATCHES) {

		conf.display_exact_matches = IsDlgButtonChecked(IDC_CHK_ONLY_EXACT_MATCHES) == BST_CHECKED;

		m_alist.fill_artist_list(conf.display_exact_matches, m_tracer.has_amr(), updRelSrc::ArtistListCheckExact);

	}
	return FALSE;
}

LRESULT CFindReleaseDialog::OnCheckboxFindReleaseFilterFlags(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {

	bool force_refresh, force_rebuild;

	pfc::string8 strFilter = trim(uGetWindowText(m_edit_filter)); 

	if (wID == IDC_CHK_FIND_RELEASE_FILTER_ROLEMAIN) {
		bool checked = IsDlgButtonChecked(IDC_CHK_FIND_RELEASE_FILTER_ROLEMAIN) == BST_CHECKED;
		if (checked)
			conf.find_release_filter_flag |= FilterFlag::RoleMain;
		else
			conf.find_release_filter_flag &= ~(FilterFlag::RoleMain);

		force_refresh = force_rebuild = true;

		if (m_query_mode & SearchMode::AT || m_query_mode & SearchMode::VA) {
			//
			return FALSE;
			//
		}

	}
	else if (wID == IDC_CHK_FIND_RELEASE_FILTER_VERS) {
		bool checked = IsDlgButtonChecked(IDC_CHK_FIND_RELEASE_FILTER_VERS) == BST_CHECKED;
		if (checked)
			conf.find_release_filter_flag |= FilterFlag::Versions;
		else
			conf.find_release_filter_flag &= ~(FilterFlag::Versions);

		force_refresh = true; force_rebuild = false;

		set_role_label(checked);

		//skip if filter is empty

		if (!strFilter.get_length()) {

			return FALSE;
		}
	}
	else {

		return FALSE;
	}

	KillTimer(KTypeFilterTimerID);

	::ShowWindow(m_release_tree, SW_SHOWNOACTIVATE);

	// launch apply filter

	service_ptr_t<tree_apply_filter_process_callback> task =
		new service_impl_t<tree_apply_filter_process_callback>(nullptr, 0, "", GetSearchMode(),
			strFilter, force_refresh, force_rebuild);

	task->start(m_hWnd);

	//

	return FALSE;
}


LRESULT CFindReleaseDialog::OnButtonConfigure(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {

	if (!g_discogs->configuration_dialog) {

		static_api_ptr_t<ui_control>()->show_preferences(guid_pref_page);
	}
	else {

		::SetFocus(g_discogs->configuration_dialog->m_hWnd);
	}
	return FALSE;
}

//todo: rev
void CFindReleaseDialog::trigger_release(const pfc::string8& release_id) {
	if (!discogger_search::valid_release_id(release_id.c_str())) {
		uMessageBox(m_hWnd, "Empty or invalid Release ID.", "foo_discogger: Error", MB_APPLMODAL | MB_ICONASTERISK);
		return;

	}

	pfc::string8 offline_artist_id;
	bool bskip_idded_release_dlg = conf.skip_mng_flag & SkipMng::RELEASE_DLG_IDED;

	if (bskip_idded_release_dlg && m_tracer.has_artist()) {
		size_t artist_id = m_tracer.get_artist_id_store();
		offline_artist_id = std::to_string(artist_id).c_str();
	}
	else {

		//dlg button

		if (m_alist.Get_Artist()) {

			// list selection artist has preference, even if is NOT the artist_id meta
			offline_artist_id = m_alist.Get_Artist()->id;
		}
		else if (m_tracer.has_artist()) {

			// artist_id meta
			offline_artist_id = std::to_string(m_tracer.get_artist_id_store()).c_str();
		}
		else {

			//offline cache not available
			offline_artist_id = "";
		}
	}

	try {
		service_ptr_t<process_release_callback> task = new service_impl_t<process_release_callback>(this, release_id, offline_artist_id, "", m_items);
		task->start(m_hWnd);
	}
	catch (locked_task_exception e)
	{
		log_msg(e.what());
	}
}

// expand_master_release_process_done

void CFindReleaseDialog::on_expand_master_release_done(const MasterRelease_ptr& master_release, int list_index, threaded_process_status& p_status, abort_callback& p_abort) {

	// forward to tree

	m_dctree.on_expand_master_release_done(master_release, list_index, p_status, p_abort);
}

void CFindReleaseDialog::call_expand_master_service(MasterRelease_ptr& release, int pos) {

	if (release->loaded_releases && release->sub_releases.get_size()) {

		//already loaded
		return;
	}

	pfc::string8 offlineArtistId = "";

	if (m_alist.Get_Artist())
		offlineArtistId = m_alist.Get_Artist()->id;
	else
		if (m_alist.Get_Artists().get_count() > 0)
			offlineArtistId = m_alist.Get_Artists()[0]->id;

	service_ptr_t<expand_master_release_process_callback> task =
		new service_impl_t<expand_master_release_process_callback>(release, pos, offlineArtistId, GetSearchMode());

	task->start(m_hWnd);
}

// convey artist-list and artist-profile actions

void CFindReleaseDialog::convey_artist_list_selection(cupdRelSrc cupdsrc) {

	//from Default action (ArtistList) or List selection (ArtistProfile)

	PFC_ASSERT(cupdsrc == updRelSrc::ArtistList || cupdsrc == updRelSrc::ArtistProfile);

	t_size pos = m_alist.GetSingleSel();

	if (pos == ~0 || !(cupdsrc == updRelSrc::ArtistList || cupdsrc == updRelSrc::ArtistProfile)) {

		return;
	}

    const auto current_artists = m_alist.Get_Artists();
    if (pos >= current_artists.get_count() || !current_artists[pos]) return;
	Artist_ptr artist = current_artists[pos];

	bool need_releases = cupdsrc == updRelSrc::ArtistList && !artist->loaded_releases;

	bool need_data = !(artist->loaded_preview || artist->loaded);

	need_data |= need_releases;

    // Search rows can contain partial results, while the canonical artist
    // has a pending explicit refresh. Force its worker path before the AT
    // display-only shortcut, without requiring another artist click.
    if (cupdsrc == updRelSrc::ArtistList) {
        const auto canonical_artist = discogs_interface
            ? discogs_interface->get_artist(artist->id) : nullptr;
        need_data = discogger_cache::should_fetch_artist(
            need_data, true,
            canonical_artist && canonical_artist->release_refresh.pending());
    }

	if (cupdsrc == updRelSrc::ArtistProfile && conf.auto_load_releases_on_select_ready()) {
		need_data = true;
	}

	// UPDATE DLG CONTROLS OR LAUNCH TASK TO COLLECT

	if (need_data || cupdsrc == updRelSrc::UndefFast || cupdsrc == updRelSrc::ArtistList)
	{
		{
			std::lock_guard<std::mutex> guard(m_loading_selection_rw_mutex);

			m_loading_selection_id = atoi(artist.get()->id);
		}

		if (artist->id.equals("0")
			|| artist->id.equals("194"/*various artists*/)
			|| artist->id.equals("355"/*Unkown artist*/)) {
			g_discogs->find_release_dialog->on_get_artist_done(cupdsrc, artist);
			return;
		}

		// need data
		if (m_query_mode && cupdsrc == updRelSrc::ArtistList && cupdsrc.extended && need_data) {

			m_query_mode = 0;
			UpdateSearchModeDisplayMode(nullptr);

		}
		else
		{
			//'auto load on artist change'
			if ((m_query_mode & SearchMode::AT) && cupdsrc.extended && (artist->master_releases.get_count() || artist->releases.get_count())) {

				if (core_api::is_main_thread()) {

					try {

						service_ptr_t<on_get_artist_done_process_callback> task =
							new service_impl_t<on_get_artist_done_process_callback>(artist, cupdsrc);

						task->start(g_discogs->find_release_dialog->m_hWnd);
					}
					catch (locked_task_exception e)
					{
						log_msg(e.what());
					}
				}

				else {
					g_discogs->find_release_dialog->on_get_artist_done(cupdsrc, artist);
				}

				//..
				return;
				//..
			}
		}


		service_ptr_t<get_artist_process_callback> task =
			new service_impl_t<get_artist_process_callback>(cupdsrc, artist->id.get_ptr(), artist->name);

		task->start(m_hWnd);
	}
	else
	{
		{
			std::lock_guard<std::mutex> guard(m_loading_selection_rw_mutex);

			m_loading_selection_id = pfc_infinite; //pending udpates validation on done 
		}
		UpdateArtistProfile(artist);
	}
}

// route

void CFindReleaseDialog::route_artist_search(pfc::string8 artistname, bool dlgbutton, bool idded) {

	bool b_idded_from_init = !dlgbutton && idded;
	bool b_skipping_idded = !dlgbutton && conf.skip_mng_flag & SkipMng::RELEASE_DLG_IDED;

	bool b_query_from_init = search_query::IsRunnable(artistname);

	pfc::string8 artist_id;
	bool cfg_skip_get_artist_if_present = true;
	bool cfg_fast_load_artist_if_idded = true;

	updRelSrc updsrc = updRelSrc::Undef;

	bool by_any = dlgbutton ||
					conf.enable_autosearch ||
					(m_tracer.has_artist() && cfg_fast_load_artist_if_idded);

	bool by_name = artistname.get_length();
	bool by_id = false;
	bool by_id_cfg_fast = false;

	if (dlgbutton) {

		by_id = false;
		by_id |= idded;

		if (by_id)
			artist_id = artistname;
	}
	else {
		by_id = idded;
		by_id &= m_tracer.has_artist();
		by_id &= !m_tracer.get_artist_id_is_vun();

		by_id_cfg_fast = m_tracer.has_artist() && cfg_skip_get_artist_if_present;
		updsrc = (m_tracer.has_artist() && cfg_fast_load_artist_if_idded) ? updRelSrc::ArtistProfile : updsrc;
		updsrc = by_id_cfg_fast ? updRelSrc::UndefFast : updRelSrc::Undef;
		if (by_id) {
			artist_id = std::to_string(m_tracer.get_artist_id()).c_str();
		}
	}

	//skip va not idded from init in default mode (not AT)
	by_any &= !(m_query_mode & SearchMode::VA && !b_idded_from_init && !(m_query_mode & SearchMode::AT));

	if (by_any) {

		if (by_id && !b_query_from_init) {

			cupdRelSrc cupdsrc(updsrc);
			cupdsrc.oninit = !dlgbutton;

			if (updsrc == updRelSrc::UndefFast) {
				cupdsrc.extended |= conf.auto_rel_load_on_open & !b_skipping_idded;
			}
			else if (updsrc == updRelSrc::ArtistProfile) {
				cupdsrc.extended |= conf.auto_load_releases_on_select_ready();
			}

			cupdsrc.extended &= !(m_query_mode & SearchMode::AT);

			bool bskip_va_auto_load = conf.skip_mng_flag & SkipMng::RELEASE_DLG_VA_AUTO_LOAD;
			bool bmulti_artist = !bskip_va_auto_load && !(dlgbutton && idded) && m_tracer.is_multi_artist();
			if (bmulti_artist) {

				service_ptr_t<get_multi_artists_process_callback> task =
					new service_impl_t<get_multi_artists_process_callback>(cupdsrc, m_query_mode, m_tracer.get_vartist_ids());

				task->start(m_hWnd);
		}
			else {
				service_ptr_t<get_artist_process_callback> task =
					new service_impl_t<get_artist_process_callback>(cupdsrc, artist_id, artistname);

				task->start(m_hWnd);
			}
		}
		else if (by_name && !CONF.awt_get_alt_mode() && !(by_id && b_skipping_idded)) {
			service_ptr_t<search_artist_process_callback> task =
				new service_impl_t<search_artist_process_callback>(artistname.get_ptr(), dlgbutton, m_query_mode, m_qdm_search_query, 0);
			task->start(m_hWnd);
		}
	}
}

// utils

bool CFindReleaseDialog::id_from_url(HWND hwndCtrl, pfc::string8& out) {
	pfc::string8 prefix;
	pfc::string8 suffix = "]";
	pfc::string8 url = "https://www.discogs.com/";
	std::string tmpstr = out.c_str();
	auto urlpos = tmpstr.find(url);
	bool is_dc_url = urlpos != std::string::npos;

	if (is_dc_url) {
		out.remove_chars(0, urlpos + url.get_length());
	}

	char mode_param;
	pfc::string8 buffer = trim(out);

	if (hwndCtrl == m_edit_artist) {

		if (buffer.has_prefix("[a=")) {

			out = out.subString(3, buffer.get_length() - 4);
			return is_number(out.get_ptr());
		}

		prefix = "[a";
		mode_param = buffer.has_prefix("artist/") ? 'w' : 'a';

	}
	else if (hwndCtrl == m_edit_release) {

		prefix = "[r";
		url << "release/";
		mode_param = buffer.has_prefix("release/") ? 'w' : 'r';
	}
	else {

		return false;
	}

	if (mode_param != 'w' && is_dc_url) {
		if (mode_param == 'a' && !buffer.startsWith("artist")) {
			out = "";
			return false;
		}
		else if (mode_param == 'r' && !buffer.startsWith("release")) {
			out = "";
			return false;
		}

		if ((buffer = extract_max_number(buffer, 'w', true)).get_length()) {

			out = buffer.c_str();
			return true;
		}
	}
	else {

		if (mode_param == 'w' || (buffer.has_prefix(prefix) && buffer.has_suffix(suffix))) {

			if ((buffer = extract_max_number(buffer, mode_param, true)).get_length()) {

				out = buffer.c_str();
				return true;
			}
		}
	}
	return false;
}
//..

void CFindReleaseDialog::print_root_stats(rppair root_stats, bool save, bool isartist, bool onlylink) {

	pfc::string8 stat_msg;

	if (conf.find_release_dlg_flags & flg_fr::FLG_SHOW_RELEASE_TREE_STATS) {
		if (!onlylink) {
			int tot = atoi(root_stats.first.first) + atoi(root_stats.first.second);
			stat_msg << "Total: " << + tot;
			stat_msg << " - Masters: " << root_stats.first.first;
		}
	}

	if (root_stats.second.first.get_length()) {
		pfc::string8 url;
		if (root_stats.second.second.find_first("www") == SIZE_MAX) {
			url = "https://www.discogs.com/";
			if (isartist) {
				url << "artist";
			}
			else {
				url << "release";
			}
			url << "/";
		}

		if (isartist && (root_stats.second.second == search_query::k_uk_id || root_stats.second.second == search_query::k_ukm_id)) {
			url = "";
			m_artist_link.EnableWindow(false);
		}
		else {
			url << root_stats.second.second;
			m_artist_link.EnableWindow(true);
		}
		pfc::stringcvt::string_wide_from_utf8 wtext(url.get_ptr());

		m_artist_link.SetHyperLink((LPCTSTR)const_cast<wchar_t*>(wtext.get_ptr()));
		pfc::string8 spa(" ");
		spa << EscapeWin(root_stats.second.first) << " ";
		wtext.convert(spa);
		m_artist_link.SetLabel(wtext.get_ptr());
		CRect rc; ::GetWindowRect(m_artist_link, &rc);
		m_artist_link.RedrawWindow(0, 0, RDW_INVALIDATE);
	}
	else {
		stat_msg = "";
		m_artist_link.SetHyperLink(L"");
		m_artist_link.SetLabel(L"");
		CRect rc; ::GetWindowRect(m_artist_link, &rc);
		m_artist_link.RedrawWindow(0, 0, RDW_INVALIDATE);
	}

	uSetDlgItemText(m_hWnd, IDC_STATIC_FIND_REL_STATS_EXT, stat_msg);

	if (save) {
		m_row_stats = root_stats;
	}
}

void replace_artist_refs(const pfc::string8 & profile, pfc::string8& outprofile, const pfc::array_t<Artist_ptr> artists) {

	outprofile = profile;

	if (!profile.get_length()) return;

	std::vector<std::pair<std::string, std::string>> vfound;

	std::regex regex_v("\\[a\\d+?\\]");

	std::string s(profile.c_str());
	std::sregex_iterator begin = std::sregex_iterator(s.begin(), s.end(), regex_v);
	std::sregex_iterator end = std::sregex_iterator();

	//extract unique artist refs
	for (std::sregex_iterator i = begin; i != end; i++) {

		std::string id(i->str());
		id = id.substr(2, id.size() - 3); //"[a1234]" -> 1234

		auto fit = std::find_if(vfound.begin(), vfound.end(), [&](const auto& item) {
			return !i->str().compare(item.first);
			});

		if (fit == vfound.end())
			vfound.emplace_back(id, "");
	}
	//lookup artist names
	for (auto & walk_found : vfound) {
		for (auto w = artists.begin(); w < artists.end(); w++) {

			if (!walk_found.first.compare(w->get()->id.c_str())) {
				walk_found.second = w->get()->name;
				break;
			}
		}
	}
	//use ref names
	if (vfound.size()) {
		for (FieldValPair walk : vfound) {
			if (walk.second.size()) {
				pfc::string8 replace = PFC_string_formatter() << "[a" << walk.first.c_str() << "]";
				outprofile = outprofile.replace(replace, walk.second.c_str());
			}
		}
	}
}

void CFindReleaseDialog::UpdateArtistProfile(Artist_ptr artist) {

	const auto exactlist = m_alist.Get_Artists();

	if (g_discogs->find_release_artist_dialog) {
		pfc::string8 modprofile;
		if (artist) {
			replace_artist_refs(artist->profile, modprofile, exactlist);
		}
		g_discogs->find_release_artist_dialog->UpdateProfile(artist, modprofile);
		HWND hwndProfile = g_discogs->find_release_artist_dialog->m_hWnd;
		fb2k::inMainThread([hwndProfile] {
			::InvalidateRect(hwndProfile, NULL, TRUE);
			});
	}
}



#if defined(DISCOGGER_RUNNER_MEDIA_QA)
// Actual Jansson + directory-recovery integration, exercised inside both
// original Win32/x64 foobar hosts on disposable TEMP cache files only.
static void runner_qa_native_cache_json_rollback() {
    namespace fs = std::filesystem;
    const fs::path active = fs::temp_directory_path() /
        ("discogger-json-rollback-qa-" + std::to_string(GetCurrentProcessId()) +
         "-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const fs::path backup = discogger_cache::cache_backup_path(active);
    struct temp_cleanup {
        fs::path active, backup;
        ~temp_cleanup() {
            std::error_code ec;
            fs::remove_all(active, ec);
            ec.clear();
            fs::remove_all(backup, ec);
        }
    } cleanup{active, backup};
    auto write = [](const fs::path& path, const char* data) {
        fs::create_directories(path.parent_path());
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out) throw std::runtime_error("Native JSON rollback QA fixture open failed.");
        out << data;
        out.close();
        if (!out.good()) throw std::runtime_error("Native JSON rollback QA fixture write failed.");
    };
    auto read = [](const fs::path& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) throw std::runtime_error("Native JSON rollback QA fixture read failed.");
        return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    };
    const fs::path payload = active / "page-0" / "root.json";
    write(payload, R"({"backup":true})");
    write(active / "taskreg.txt", "");
    if (!ol::cache_payload_is_valid_json(payload))
        throw std::runtime_error("Native Jansson rejected valid JSON.");
    // Test both Jansson's parser and the production reader in the actual
    // foobar host, not just a synthetic callback or source-level contract.
    json_error_t literal_error = {};
    json_t* literal = json_loads(R"({"backup":true})", JSON_DECODE_ANY, &literal_error);
    if (!literal || !json_is_object(literal)) {
        if (literal) json_decref(literal);
        throw std::runtime_error("Native Jansson rejected valid in-memory literal.");
    }
    json_decref(literal);
    ol::ol_cache real_reader;
    const std::string payload_utf8 = payload.u8string();
    json_t* from_disk = real_reader.Read_JSON(payload_utf8.c_str());
    const bool disk_valid = from_disk && json_is_object(from_disk);
    if (from_disk) json_decref(from_disk);
    if (!disk_valid)
        throw std::runtime_error("Native offline cache reader rejected valid JSON.");
    // Exercise the production atomic JSON serializer on disposable data.
    json_t* generated = json_object();
    if (!generated || json_object_set_new(generated, "generated", json_true()) != 0) {
        if (generated) json_decref(generated);
        throw std::runtime_error("Native Jansson QA could not create object.");
    }
    const fs::path serialized = active / "serialized-qa.json";
    const std::string serialized_utf8 = serialized.u8string();
    const bool dump_ok = real_reader.Dump_JSON(serialized_utf8.c_str(), generated);
    json_decref(generated);
    if (!dump_ok || !ol::cache_payload_is_valid_json(serialized))
        throw std::runtime_error("Native atomic JSON serializer did not roundtrip.");

    fs::copy(active, backup, fs::copy_options::recursive);
    write(payload, R"({"broken":)");
    json_t* corrupt_read = real_reader.Read_JSON(payload_utf8.c_str());
    const bool corrupt_rejected = corrupt_read && json_is_array(corrupt_read);
    if (corrupt_read) json_decref(corrupt_read);
    if (!corrupt_rejected)
        throw std::runtime_error("Native offline reader accepted truncated JSON.");
    if (ol::cache_payload_is_valid_json(payload) ||
        !discogger_cache::cache_has_marker(active) ||
        discogger_cache::recover_cache_directory(active) ||
        !fs::exists(backup) || read(payload) != R"({"broken":)")
        throw std::runtime_error("Native malformed active JSON did not preserve rollback.");
    if (!discogger_cache::recover_cache_directory(active, ol::cache_payload_is_valid_json) ||
        read(payload) != R"({"backup":true})" || fs::exists(backup))
        throw std::runtime_error("Native Jansson did not restore valid rollback.");
    fs::copy(active, backup, fs::copy_options::recursive);
    write(backup / "page-0" / "root.json", R"({"broken":)");
    if (discogger_cache::recover_cache_directory(active, ol::cache_payload_is_valid_json) ||
        !fs::exists(backup) || read(payload) != R"({"backup":true})")
        throw std::runtime_error("Native malformed backup was promoted or discarded.");
    write(backup / "page-0" / "root.json", R"({"backup":true})");
    if (!discogger_cache::recover_cache_directory(active, ol::cache_payload_is_valid_json) ||
        fs::exists(backup))
        throw std::runtime_error("Native valid JSON rollback cleanup failed.");
    // The actual native cache publisher must use this same Jansson
    // decoder on STAGED files, not only during interrupted-run recovery.
    // Both branches operate solely on this unique disposable TEMP fixture.
    {
        discogger_cache::directory_cache_transaction tx(
            active, ol::cache_payload_is_valid_json);
        if (!tx.ready()) throw std::runtime_error("Native JSON stage guard could not start.");
        write(tx.staging_directory() / "page-0" / "root.json", R"({"broken":)");
        if (tx.publish("invalid"))
            throw std::runtime_error("Native Jansson published malformed staged JSON.");
    }
    if (read(payload) != R"({"backup":true})" || fs::exists(backup))
        throw std::runtime_error("Native malformed staged JSON changed valid active cache.");
    {
        discogger_cache::directory_cache_transaction tx(
            active, ol::cache_payload_is_valid_json);
        if (!tx.ready()) throw std::runtime_error("Native valid JSON stage could not start.");
        write(tx.staging_directory() / "page-0" / "root.json", R"({"fresh":true})");
        if (!tx.publish("fresh"))
            throw std::runtime_error("Native Jansson rejected valid staged JSON.");
    }
    if (read(payload) != R"({"fresh":true})" || fs::exists(backup))
        throw std::runtime_error("Native valid staged JSON publication did not preserve cache.");
    log_msg("DISCOGGER_NATIVE_JSON_ROLLBACK_QA=PASS");
}
#endif

#if defined(DISCOGGER_RUNNER_MEDIA_QA)
std::array<size_t, 3> CFindReleaseDialog::runner_qa_live_search_probe(const char* expected_release_id) {
    if (!::IsWindow(m_hWnd) || !::IsWindow(m_artist_list) || !::IsWindow(m_release_tree))
        throw std::runtime_error("Live result inspection requires an open native Find Releases dialog.");
    const auto artists = m_alist.Get_Artists();
    size_t known_release_matches = 0;
    for (size_t i = 0; i < artists.get_count(); ++i) {
        const Artist_ptr& artist = artists[i];
        if (!artist) continue;
        for (size_t j = 0; j < artist->releases.get_count(); ++j) {
            const Release_ptr& release = artist->releases[j];
            if (release && release->id.equals(expected_release_id)) {
                ++known_release_matches;
            }
        }
        for (size_t j = 0; j < artist->master_releases.get_count(); ++j) {
            const MasterRelease_ptr& master = artist->master_releases[j];
            if (!master) continue;
            if (master->main_release_id.equals(expected_release_id)) {
                ++known_release_matches;
            }
            // Database-search releases may be grouped below a master even
            // when the known release is not that master's main release.
            for (const Release_ptr& subrelease : master->sub_releases) {
                if (subrelease && subrelease->id.equals(expected_release_id)) {
                    ++known_release_matches;
                }
            }
        }
    }
    // Observe only. The real search must publish its own tree; the QA
    // inspector must not call the presenter and mask a missing auto-display.
    return {artists.get_count(), known_release_matches, m_dctree.Get_Size()};
}

std::pair<size_t, size_t> CFindReleaseDialog::runner_qa_populated_search_rows() {
    if (!::IsWindow(m_hWnd) || !::IsWindow(m_artist_list) || !::IsWindow(m_release_tree))
        throw std::runtime_error("Synthetic result QA requires an open native Find Releases dialog.");
    runner_qa_native_cache_json_rollback();
    const size_t previous_mode = m_query_mode;
    m_query_mode = SearchMode::AT; // exercise the real advanced-search auto-preview path

    // Preserve the five cross-provider identities from the original fixture.
    Artist_ptr artist = std::make_shared<Artist>("0");
    artist->name = "Discogger synthetic search results";
    artist->loaded_preview = true;
    artist->loaded_releases = true;
    const std::pair<const char*, const char*> known[] = {
        {"38012673", "Via Balearia"},
        {"37332711", "1000 Years"},
        {"36397609", "Only You"},
        {"36819730", "Till You Let Go"},
        {"35534398", "Consciousness EP"}
    };
    for (const auto& record : known) {
        Release_ptr release = std::make_shared<Release>(record.first);
        release->title = record.second;
        release->loaded_preview = true;
        artist->releases.append_single(release);
        artist->search_order_master.append_single(false);
    }
    pfc::array_t<Artist_ptr> alternatives, exact;
    alternatives.append_single(artist);
    CheckDlgButton(IDC_CHK_ONLY_EXACT_MATCHES, BST_CHECKED);
    on_search_artist_done(exact, alternatives, false);
    if (m_alist.Get_Artists().get_count() != 1 ||
        IsDlgButtonChecked(IDC_CHK_ONLY_EXACT_MATCHES) != BST_UNCHECKED)
        throw std::runtime_error("Native populated alternative artist list/filter failed.");
    // No manual ShowArtistSearchResults call: callback must populate the real tree.
    const size_t initial_tree = m_dctree.Get_Size();
    if (initial_tree != 5)
        throw std::runtime_error("Native auto-preview did not publish five synthetic releases.");

    // Multiple artist matches, mixed masters/releases, and duplicate IDs.
    Artist_ptr first = std::make_shared<Artist>("101");
    first->name = "First search match";
    Release_ptr first_release = std::make_shared<Release>("2844");
    first_release->title = "One More Time";
    first_release->loaded_preview = true;
    first->releases.append_single(first_release);
    first->search_order_master.append_single(false);
    MasterRelease_ptr first_master = std::make_shared<MasterRelease>("901");
    first_master->title = "Synthetic master";
    first_master->loaded_preview = true;
    first->master_releases.append_single(first_master);
    first->search_order_master.append_single(true);

    Artist_ptr second = std::make_shared<Artist>("102");
    second->name = "Second search match";
    Release_ptr duplicate_release = std::make_shared<Release>("2844");
    duplicate_release->title = "One More Time duplicate";
    duplicate_release->loaded_preview = true;
    second->releases.append_single(duplicate_release);
    second->search_order_master.append_single(false);
    Release_ptr second_release = std::make_shared<Release>("7809");
    second_release->title = "Hey Boy Hey Girl";
    second_release->loaded_preview = true;
    second->releases.append_single(second_release);
    second->search_order_master.append_single(false);
    MasterRelease_ptr duplicate_master = std::make_shared<MasterRelease>("901");
    duplicate_master->title = "Synthetic master duplicate";
    duplicate_master->loaded_preview = true;
    second->master_releases.append_single(duplicate_master);
    second->search_order_master.append_single(true);

    pfc::array_t<Artist_ptr> two_artists;
    two_artists.append_single(first);
    two_artists.append_single(second);
    on_search_artist_done(two_artists, exact, false);
    if (m_alist.Get_Artists().get_count() != 2 || m_dctree.Get_Size() != 3)
        throw std::runtime_error("Multi-artist auto-preview did not deduplicate and display three releases/masters.");

    // Two provider phases can return artist 101 with DISJOINT payloads.
    // Both the automatic multi-artist tree and the manually chosen artist
    // must retain the union without changing the original cached objects.
    const size_t same_id_original_role =
        m_alist.Get_Artists()[0]->search_role_list_pos;
    Artist_ptr same_id_delta = std::make_shared<Artist>("101");
    same_id_delta->name = "Same artist, later provider page";
    Release_ptr same_id_release = std::make_shared<Release>("410001");
    same_id_release->title = "Late same-ID release";
    same_id_release->loaded_preview = true;
    same_id_delta->releases.append_single(same_id_release);
    same_id_delta->search_order_master.append_single(false);
    MasterRelease_ptr same_id_master = std::make_shared<MasterRelease>("410002");
    same_id_master->title = "Late same-ID master";
    same_id_master->loaded_preview = true;
    same_id_delta->master_releases.append_single(same_id_master);
    same_id_delta->search_order_master.append_single(true);
    pfc::array_t<Artist_ptr> same_id_delta_rows;
    same_id_delta_rows.append_single(same_id_delta);
    on_search_artist_done(same_id_delta_rows, exact, true);
    auto merged_same_id_rows = m_alist.Get_Artists();
    if (merged_same_id_rows.get_count() != 2 || m_dctree.Get_Size() != 5 ||
        merged_same_id_rows[0]->releases.get_count() != 2 ||
        merged_same_id_rows[0]->master_releases.get_count() != 2 ||
        merged_same_id_rows[0]->search_role_list_pos != same_id_original_role ||
        merged_same_id_rows[0]->loaded_releases ||
        first->releases.get_count() != 1 || first->master_releases.get_count() != 1 ||
        same_id_delta->releases.get_count() != 1 ||
        same_id_delta->master_releases.get_count() != 1)
        throw std::runtime_error("Native same-ID disjoint release/master union lost payload, role or cache isolation.");

    // Negative: repeat identical phase and same-ID duplicate with only old
    // IDs. Neither may add a tree node, change row identity, or grow arrays.
    on_search_artist_done(same_id_delta_rows, exact, true);
    Artist_ptr same_id_repeat = std::make_shared<Artist>("101");
    same_id_repeat->releases.append_single(first_release);
    same_id_repeat->search_order_master.append_single(false);
    same_id_repeat->master_releases.append_single(first_master);
    same_id_repeat->search_order_master.append_single(true);
    pfc::array_t<Artist_ptr> same_id_repeat_rows;
    same_id_repeat_rows.append_single(same_id_repeat);
    on_search_artist_done(same_id_repeat_rows, exact, true);
    merged_same_id_rows = m_alist.Get_Artists();
    if (merged_same_id_rows.get_count() != 2 || m_dctree.Get_Size() != 5 ||
        merged_same_id_rows[0]->releases.get_count() != 2 ||
        merged_same_id_rows[0]->master_releases.get_count() != 2 ||
        merged_same_id_rows[0]->search_role_list_pos != same_id_original_role)
        throw std::runtime_error("Native repeated same-ID payload duplicated rows or releases.");

    // Default_Action uses ShowArtistSearchResults on the selected partial
    // artist. Exercise its actual projection, not just the combined tree.
    ShowArtistSearchResults(merged_same_id_rows[0]);
    if (m_dctree.Get_Size() != 4)
        throw std::runtime_error("Native manually selected same-ID artist lost a late release or master.");

    // Exact-only remains a distinct visible projection, including a late
    // same-ID exact update, without introducing alternative-only artists.
    CheckDlgButton(IDC_CHK_ONLY_EXACT_MATCHES, BST_CHECKED);
    on_search_artist_done(two_artists, exact, false);
    on_search_artist_done(same_id_delta_rows, exact, true);
    if (m_alist.Get_Artists().get_count() != 2 ||
        IsDlgButtonChecked(IDC_CHK_ONLY_EXACT_MATCHES) != BST_CHECKED ||
        m_dctree.Get_Size() != 5)
        throw std::runtime_error("Native exact-only same-ID union lost visibility or release tree.");
    CheckDlgButton(IDC_CHK_ONLY_EXACT_MATCHES, BST_UNCHECKED);
    on_search_artist_done(two_artists, exact, false);
    if (m_dctree.Get_Size() != 3)
        throw std::runtime_error("Native new search did not reset previous same-ID union.");

    // Native regression: asynchronous updates append cumulative exact/other
    // buckets. Old code counted the visible alternatives as exact rows, lost
    // the newly appended exact artist, then appended old alternatives twice.
    Artist_ptr other_artist = std::make_shared<Artist>("105");
    other_artist->name = "Initial alternative";
    pfc::array_t<Artist_ptr> initial_others;
    initial_others.append_single(other_artist);
    on_search_artist_done(two_artists, initial_others, false);
    const auto first_rows = m_alist.Get_Artists();
    if (first_rows.get_count() != 3 || !first_rows[0] || !first_rows[1] ||
        !first_rows[2] || !first_rows[0]->id.equals("101") ||
        !first_rows[1]->id.equals("102") || !first_rows[2]->id.equals("105"))
        throw std::runtime_error("Native initial exact/other artist rows were incorrectly ordered.");
    const size_t original_first_role = first_rows[0]->search_role_list_pos;
    const size_t original_second_role = first_rows[1]->search_role_list_pos;
    const size_t original_other_role = first_rows[2]->search_role_list_pos;
    if (!original_first_role || !original_second_role || !original_other_role)
        throw std::runtime_error("Native initial artist rows have invalid search roles.");

    Artist_ptr added_exact = std::make_shared<Artist>("106");
    added_exact->name = "Late exact artist";
    Release_ptr late_exact_release = std::make_shared<Release>("32559");
    late_exact_release->title = "Joga (late exact)";
    late_exact_release->loaded_preview = true;
    added_exact->releases.append_single(late_exact_release);
    added_exact->search_order_master.append_single(false);
    pfc::array_t<Artist_ptr> late_exact;
    late_exact.append_single(added_exact);
    on_search_artist_done(late_exact, exact, true);
    auto appended_rows = m_alist.Get_Artists();
    if (appended_rows.get_count() != 4 ||
        !appended_rows[0]->id.equals("101") ||
        !appended_rows[1]->id.equals("102") ||
        !appended_rows[2]->id.equals("106") ||
        !appended_rows[3]->id.equals("105") ||
        appended_rows[0]->search_role_list_pos != original_first_role ||
        appended_rows[1]->search_role_list_pos != original_second_role ||
        appended_rows[3]->search_role_list_pos != original_other_role ||
        !appended_rows[2]->search_role_list_pos)
        throw std::runtime_error("Native append lost a new exact artist or changed an existing role.");
    if (m_dctree.Get_Size() != 4)
        throw std::runtime_error("Native late exact search results were not automatically added to the release tree.");

    Artist_ptr added_other = std::make_shared<Artist>("107");
    added_other->name = "Late alternative";
    Release_ptr late_other_release = std::make_shared<Release>("78731");
    late_other_release->title = "Late alternative release";
    late_other_release->loaded_preview = true;
    added_other->releases.append_single(late_other_release);
    added_other->search_order_master.append_single(false);
    pfc::array_t<Artist_ptr> late_other;
    late_other.append_single(added_other);
    on_search_artist_done(exact, late_other, true);
    on_search_artist_done(exact, pfc::array_t<Artist_ptr>(), true);
    appended_rows = m_alist.Get_Artists();
    if (appended_rows.get_count() != 5 ||
        !appended_rows[0]->id.equals("101") ||
        !appended_rows[1]->id.equals("102") ||
        !appended_rows[2]->id.equals("106") ||
        !appended_rows[3]->id.equals("105") ||
        !appended_rows[4]->id.equals("107") ||
        appended_rows[3]->search_role_list_pos != original_other_role ||
        !appended_rows[4]->search_role_list_pos)
        throw std::runtime_error("Native repeated append duplicated alternatives or lost artist roles.");
    if (m_dctree.Get_Size() != 5)
        throw std::runtime_error("Native late alternative results were not automatically added to the release tree.");

    // Same IDs may be returned by two phases, and failed async lookups may
    // contribute nulls. Neither may create a new visible row or change roles.
    pfc::array_t<Artist_ptr> duplicate_exact, duplicate_other, null_exact;
    duplicate_exact.append_single(added_exact);
    duplicate_other.append_single(other_artist);
    null_exact.append_single(Artist_ptr());
    on_search_artist_done(duplicate_exact, duplicate_other, true);
    on_search_artist_done(null_exact, exact, true);
    appended_rows = m_alist.Get_Artists();
    if (appended_rows.get_count() != 5 || m_dctree.Get_Size() != 5 ||
        appended_rows[0]->search_role_list_pos != original_first_role ||
        appended_rows[3]->search_role_list_pos != original_other_role)
        throw std::runtime_error("Native duplicate/null search updates created rows or changed roles.");

    // Exact-only is a separate visible projection: newly appended artist
    // objects previously retained their default role zero, and later row
    // selection could be confused with another search-result generation.
    CheckDlgButton(IDC_CHK_ONLY_EXACT_MATCHES, BST_CHECKED);
    on_search_artist_done(two_artists, exact, false);
    auto exact_rows = m_alist.Get_Artists();
    if (exact_rows.get_count() != 2 ||
        !exact_rows[0]->id.equals("101") || !exact_rows[1]->id.equals("102") ||
        !exact_rows[0]->search_role_list_pos || !exact_rows[1]->search_role_list_pos)
        throw std::runtime_error("Native exact-only artist list initial projection failed.");
    // An append containing alternatives but no *new* exact matches must
    // not disable an exact-only filter while earlier exact matches exist.
    on_search_artist_done(exact, late_other, true);
    exact_rows = m_alist.Get_Artists();
    if (exact_rows.get_count() != 2 ||
        IsDlgButtonChecked(IDC_CHK_ONLY_EXACT_MATCHES) != BST_CHECKED)
        throw std::runtime_error("Native late alternatives disabled an existing exact-only filter.");
    const size_t exact_role_one = exact_rows[0]->search_role_list_pos;
    const size_t exact_role_two = exact_rows[1]->search_role_list_pos;
    Artist_ptr exact_late_artist = std::make_shared<Artist>("108");
    exact_late_artist->name = "Late exact-only artist";
    pfc::array_t<Artist_ptr> exact_late;
    exact_late.append_single(exact_late_artist);
    on_search_artist_done(exact_late, exact, true);
    on_search_artist_done(exact, pfc::array_t<Artist_ptr>(), true);
    exact_rows = m_alist.Get_Artists();
    if (exact_rows.get_count() != 3 ||
        !exact_rows[0]->id.equals("101") ||
        !exact_rows[1]->id.equals("102") ||
        !exact_rows[2]->id.equals("108") ||
        exact_rows[0]->search_role_list_pos != exact_role_one ||
        exact_rows[1]->search_role_list_pos != exact_role_two ||
        !exact_rows[2]->search_role_list_pos ||
        exact_rows[2]->search_role_list_pos == exact_role_one ||
        exact_rows[2]->search_role_list_pos == exact_role_two)
        throw std::runtime_error("Native exact-only append lost or duplicated artist selection roles.");
    CheckDlgButton(IDC_CHK_ONLY_EXACT_MATCHES, BST_UNCHECKED);

    // Same ID 0 preview sentinel, a new query result: must replace stale tree.
    Artist_ptr refreshed = std::make_shared<Artist>("103");
    Release_ptr new_release = std::make_shared<Release>("32559");
    new_release->title = "Joga";
    new_release->loaded_preview = true;
    refreshed->releases.append_single(new_release);
    refreshed->search_order_master.append_single(false);
    pfc::array_t<Artist_ptr> changed_artists;
    changed_artists.append_single(refreshed);
    changed_artists.append_single(std::make_shared<Artist>("104"));
    on_search_artist_done(changed_artists, exact, false);
    if (m_dctree.Get_Size() != 1)
        throw std::runtime_error("Repeated multi-artist search retained an obsolete preview tree.");

    on_search_artist_done(exact, alternatives, false);
    if (m_dctree.Get_Size() != 5)
        throw std::runtime_error("Returning to a single search bucket did not refresh results.");

    // A failed async exact lookup may return a null Artist_ptr. It must
    // never suppress real alternatives or masquerade as a valid exact hit.
    CheckDlgButton(IDC_CHK_ONLY_EXACT_MATCHES, BST_CHECKED);
    on_search_artist_done(null_exact, alternatives, false);
    if (m_alist.Get_Artists().get_count() != 1 ||
        IsDlgButtonChecked(IDC_CHK_ONLY_EXACT_MATCHES) != BST_UNCHECKED ||
        m_dctree.Get_Size() != 5)
        throw std::runtime_error("Native null-only exact result incorrectly hid valid alternatives.");
    on_search_artist_done(null_exact, exact, false);
    if (m_alist.Get_Artists().get_count() != 0 || m_dctree.Get_Size() != 0)
        throw std::runtime_error("Native null-only search retained stale visible results.");

    // An allocated-but-empty ID is as unusable as a null artist pointer.
    // Exact-only must fall back to valid alternatives; late empty-ID
    // notifications must neither add rows nor replace a correct preview.
    pfc::array_t<Artist_ptr> empty_id_exact;
    empty_id_exact.append_single(std::make_shared<Artist>(""));
    CheckDlgButton(IDC_CHK_ONLY_EXACT_MATCHES, BST_CHECKED);
    on_search_artist_done(empty_id_exact, alternatives, false);
    if (m_alist.Get_Artists().get_count() != 1 ||
        !m_alist.Get_Artists()[0]->id.equals("0") ||
        IsDlgButtonChecked(IDC_CHK_ONLY_EXACT_MATCHES) != BST_UNCHECKED ||
        m_dctree.Get_Size() != 5)
        throw std::runtime_error("Native empty-ID exact artist hid valid alternatives.");
    on_search_artist_done(empty_id_exact, empty_id_exact, true);
    if (m_alist.Get_Artists().get_count() != 1 || m_dctree.Get_Size() != 5)
        throw std::runtime_error("Native empty-ID append polluted rows or preview.");
    m_query_mode = previous_mode;
    return {1, initial_tree};
}
#endif

void CFindReleaseDialog::ShowArtistSearchResults(Artist_ptr artist) {
	cupdRelSrc cupdsrc(updRelSrc::ArtistSearchAT);
	m_dctree.on_get_artist_done(cupdsrc, artist);
}

bool CFindReleaseDialog::add_history(oplog_type optype, std::string cmd, pfc::string8 ff, pfc::string8 fs, pfc::string8 sf, pfc::string8 ss) {

	size_t inc_id = pfc_infinite;

	if (optype == oplog_type::artist) {

		// artist

		if (sf.equals("194")) {
			//..
			return false;
			//..
		}

		rppair row = std::pair(std::pair("", ""), std::pair(sf, ss));

		if (m_oplogger.add_history_row(optype, row)) {
			sqldb db;
			inc_id = db.insert_history(optype, cmd, row);
		}
	}
	else if (optype == oplog_type::query) {

		// search query

		rppair row = std::pair(std::pair("", ""), std::pair(sf, ss));

		if (m_oplogger.add_history_row(optype, row)) {
			sqldb db;
			inc_id = db.insert_history(optype, cmd, row);
		}
	}
	else if (optype == oplog_type::release) {

		// release

		rppair row = rppair(std::pair(ff, fs), std::pair(sf, ss));

		if (m_oplogger.add_history_row(optype, row)) {
			sqldb db;
			inc_id = db.insert_history(optype, cmd, row);
		}
	}
	else if (optype == oplog_type::filter) {

		// filter

		rppair row = rppair(std::pair(ff, fs), std::pair(sf, ss));

		if (m_oplogger.add_history_row(optype, row)) {
			sqldb db;
			inc_id = db.insert_history(optype, cmd, row);
		}
	}
	return inc_id;
}

bool CFindReleaseDialog::add_history(oplog_type optype, std::string cmd, rppair row) {
	return add_history(optype, cmd, row.first.first, row.first.second, row.second.first, row.second.second);
}

bool CFindReleaseDialog::SetConfigFlag(int ID, int flag, bool flagvalue) {

	FlgMng fv_stats;

	conf.GetFlagVar(ID, fv_stats);
	fv_stats.SetFlag(flag, flagvalue);

	return true;
}
