#pragma once
#include "sqlite3.h"
#include "db_fetcher.h"

#include "discogs.h"
#include "file_info_manager.h"
#include "find_release_dialog.h"
#include "error_manager.h"
#include "tag_writer.h"
#include "discogger_async_write_lease.h"
#include "discogger_artist_load_ownership.h"

#include "track_matching_dialog.h"
#include "preview_dialog.h"
#include "preview_leading_tag_dialog.h"


using namespace Discogs;

class foo_discogs_threaded_process_callback : public threaded_process_callback, public ErrorManager
{
public:
	db_fetcher* get_dbfetcher() { return nullptr; }

protected:
	// implement task here. catches foo_discogs exceptions.
	virtual void safe_run(threaded_process_status &p_status, abort_callback &p_abort) = 0;

	// success callback
	virtual void on_success(HWND p_wnd) {};

	// aborted callback
	virtual void on_abort(HWND p_wnd) {};

	// error callback. display errors if return true
	virtual void on_error(HWND p_wnd) {}

private:
	void run(threaded_process_status &p_status, abort_callback &p_abort) override final;
	void on_done(HWND p_wnd, bool p_was_aborted) override final;
};

class locked_task_exception : public foo_discogs_exception
{
public:
	locked_task_exception() : foo_discogs_exception("Locked Task Exception") {}
	locked_task_exception(const char* msg) : foo_discogs_exception(msg) {}
};

class foo_discogs_threaded_locked_process_callback : public foo_discogs_threaded_process_callback
{
public:
	foo_discogs_threaded_locked_process_callback() {
		std::lock_guard<std::mutex> guard(g_discogs->locked_operation_rw_mutex);
		if (g_discogs->locked_operation) {
			throw locked_task_exception("Operation locked");
		}
		++g_discogs->locked_operation;
	}

	~foo_discogs_threaded_locked_process_callback() {
		std::lock_guard<std::mutex> guard(g_discogs->locked_operation_rw_mutex);
		--g_discogs->locked_operation;
	}
};

// Tag writes have their own operation gate, independent of artist/search operations.
// Acquire before launching the worker and release when its callback is destroyed.
// Legacy read/filter task name uses the same mutex-protected operation gate.
// Keep this separate from the asynchronous physical tag-write gate.
using foo_discogs_locked_threaded_process_callback = foo_discogs_threaded_locked_process_callback;

class generate_tags_task : public foo_discogs_threaded_process_callback
{
public:

	generate_tags_task(CPreviewTagsDialog *preview_dialog, TagWriter_ptr tag_writer);
	generate_tags_task(CTrackMatchingDialog *release_dialog, TagWriter_ptr tag_writer, bool show_preview_dialog);
	generate_tags_task(CTagCreditDialog* credits_dialog, TagWriter_ptr tag_writer, tag_mapping_list_type* alt_mappings);
	~generate_tags_task();
	void start();

private:

	CPreviewTagsDialog *m_preview_dialog = nullptr;
	CTrackMatchingDialog *m_track_matching_dialog = nullptr;
	CTagCreditDialog *m_credits_dialog = nullptr;
    HWND m_owner_window = nullptr; // Captured once before the modeless worker.
    bool origin_is_current() const;

	bool m_show_preview_dialog;
	bool m_quick_abort = false;

	TagWriter_ptr m_tag_writer;
	tag_mapping_list_type* m_alt_mappings;

	void safe_run(threaded_process_status &p_status, abort_callback &p_abort) override;
	void on_success(HWND p_wnd) override;
	void on_abort(HWND p_wnd) override;
	void on_error(HWND p_wnd) override;
};


class write_tags_task : public foo_discogs_threaded_process_callback
{
public:
	write_tags_task(TagWriter_ptr tag_writer) : m_tag_writer(tag_writer),
        m_write_lease(discogger_write_safety::acquire_physical_write_lease()) {
        if (!m_write_lease) throw locked_task_exception("Another Discogger tag/artwork write is active");
    }
	void start();

private:
	TagWriter_ptr m_tag_writer;
    discogger_write_safety::async_write_gate::lease m_write_lease;

	void safe_run(threaded_process_status &p_status, abort_callback &p_abort) override;
	void on_success(HWND p_wnd) override;
	void on_abort(HWND p_wnd) override;
	void on_error(HWND p_wnd) override;
};


class download_art_task : public foo_discogs_threaded_process_callback
{
public:
	download_art_task(pfc::string8 release_id, metadb_handle_list items, bool file_match,
        discogger_write_safety::async_write_gate::lease write_lease = {});
	void start();

private:
	pfc::string8 release_id;
	metadb_handle_list items;
	bool m_file_match;
    discogger_write_safety::async_write_gate::lease m_write_lease;

	void safe_run(threaded_process_status &p_status, abort_callback &p_abort) override;
};


//album_art_ids (front, back...) based art download
class download_art_paths_task : public foo_discogs_threaded_process_callback
{
public:
	download_art_paths_task(CTrackMatchingDialog* m_dialog, pfc::string8 release_id, metadb_handle_list items, pfc::array_t<GUID> album_art_ids, bool to_path_only, bool file_match);
	void start();

private:
	CTrackMatchingDialog* m_dialog;
    HWND m_owner_window = nullptr; // Captured before asynchronous launch.
	pfc::array_t<GUID> m_album_art_ids;
	pfc::string8 m_release_id;
	metadb_handle_list m_items;
	bool m_to_path_only; // Preview mode must never mutate external/embedded artwork.
	bool m_file_match;
    discogger_write_safety::async_write_gate::lease m_write_lease;

	std::vector<std::pair<pfc::string8, bit_array_bittable>> m_vres;
	void safe_run(threaded_process_status& p_status, abort_callback& p_abort) override;
	void on_success(HWND p_wnd) override;
};


class find_deleted_releases_task : public foo_discogs_locked_threaded_process_callback
{
public:
	find_deleted_releases_task(metadb_handle_list items);
	void start();

private:
	file_info_manager m_finfo_manager;
	metadb_handle_list m_items;
	metadb_handle_list m_deleted_items;

	void safe_run(threaded_process_status &p_status, abort_callback &p_abort) override;
	void on_success(HWND p_wnd) override;
	void on_abort(HWND p_wnd) override; 
	void on_error(HWND p_wnd) override;

	void finish();
};


class find_releases_not_in_collection_task : public foo_discogs_locked_threaded_process_callback
{
public:
	find_releases_not_in_collection_task(metadb_handle_list items);
	void start();

private:
	file_info_manager m_finfo_manager;
	metadb_handle_list items;
	metadb_handle_list missing_items;

	void safe_run(threaded_process_status &p_status, abort_callback &p_abort) override;
	void on_success(HWND p_wnd) override;
	void on_abort(HWND p_wnd) override;
	void on_error(HWND p_wnd) override;

	void finish();
};


class get_artist_process_callback : public foo_discogs_threaded_process_callback

{
public:
	get_artist_process_callback(cupdRelSrc cupdsrc, const char *artist_id, const char* artist_name)
			: m_artist_id(artist_id), m_artist_name(artist_name), m_cupdsrc(cupdsrc) {
		m_bload_releases = m_cupdsrc != updRelSrc::ArtistProfile && m_cupdsrc != updRelSrc::UndefFast;
        		m_bload_releases |= m_cupdsrc.extended;
	}
	void start(HWND parent);

private:
	pfc::string8 m_artist_id;
	pfc::string8 m_artist_name;
    HWND m_owner_window = nullptr;
	Artist_ptr m_artist;
	cupdRelSrc m_cupdsrc;
	bool m_bload_releases = false;
	// Only the task that inserted the artist ID may remove its in-flight marker.
	bool m_artist_load_registered = false;

	bool check_in();
	void check_out();

	void safe_run(threaded_process_status &p_status, abort_callback &p_abort) override;
	void on_success(HWND p_wnd) override;
	void on_error(HWND p_wnd) override;
	void on_abort(HWND p_wnd) override;
};


class get_multi_artists_process_callback : public foo_discogs_threaded_process_callback

{
public:
	get_multi_artists_process_callback(cupdRelSrc cupdsrc, const int va, const std::vector<size_t> & artist_ids)
		: m_artist_ids(discogger_async::unique_artist_ids(artist_ids)), m_cupdsrc(cupdsrc), m_va(va) {
		
		m_bload_releases = m_cupdsrc != updRelSrc::ArtistProfile && m_cupdsrc != updRelSrc::UndefFast;
		m_bload_releases |= m_cupdsrc.extended;	
	}
	void start(HWND parent);

private:
	std::vector<size_t> m_artist_ids; // Owned across modeless dialog destruction.
    HWND m_owner_window = nullptr;
	std::vector<pfc::string8> m_checked_in_artist_ids;
	pfc::array_t<Artist_ptr> m_artists;
	Release_ptr m_release;
	cupdRelSrc m_cupdsrc;
	bool m_bload_releases = false;
	const int m_va;

	bool check_in();
	void check_out();

	void safe_run(threaded_process_status& p_status, abort_callback& p_abort) override;
	void on_success(HWND p_wnd) override;
	void on_error(HWND p_wnd) override;
	void on_abort(HWND p_wnd);
};


class search_artist_process_callback : public foo_discogs_threaded_locked_process_callback
{
public:
	search_artist_process_callback(const char* search, bool dlgbutton, const int va, const QueryDefMap qdm_search_query, const int db_dc_flags);
	~search_artist_process_callback();
	void start(HWND parent);

private:
	pfc::string8 m_search;
    HWND m_owner_window = nullptr; // Captured before the online worker starts.
	bool m_dlgbutton = false;
	const int m_va;
	QueryDefMap m_qdm_search_query = {};
	int m_db_dc_flags;

	rppair_t m_res_searchquery = {};

	pfc::array_t<Artist_ptr> m_artist_exact_matches;
	pfc::array_t<Artist_ptr> m_artist_other_matches;

	void safe_run(threaded_process_status &p_status, abort_callback &p_abort) override;

	void on_success(HWND p_wnd) override;
	void on_abort(HWND p_wnd) override;
	void on_error(HWND p_wnd) override;

};


class expand_master_release_process_callback : public foo_discogs_threaded_process_callback
{
public:
	expand_master_release_process_callback(const MasterRelease_ptr& master_release, const int pos, pfc::string8 offlineArtistId, int va_artists) :
			m_master_release(master_release), m_pos(pos), m_offlineArtist_id(offlineArtistId), m_query_mode(va_artists) {}
	void start(HWND parent);

private:
	MasterRelease_ptr m_master_release;
	int m_pos;
	pfc::string8 m_offlineArtist_id;
	int m_query_mode;
    HWND m_owner_window = nullptr; // Exact window captured before worker dispatch.

	void safe_run(threaded_process_status& p_status, abort_callback& p_abort) override;
	void on_success(HWND p_wnd) override;
	void on_abort(HWND p_wnd) override;
	void on_error(HWND p_wnd) override;
};


class tree_apply_filter_process_callback : public foo_discogs_threaded_locked_process_callback /*foo_discogs_threaded_process_callback*/
{
public:
	tree_apply_filter_process_callback(const MasterRelease_ptr& master_release, const int pos, pfc::string8 offlineArtistId, int va_artists,
		pfc::string8 filter, bool force_redraw, bool force_rebuild) :
		m_master_release(master_release), m_pos(pos), m_offlineArtist_id(offlineArtistId), m_query_mode(va_artists),
		m_strFilter(filter), m_force_redraw(force_redraw), m_force_rebuild(force_rebuild) {}

	void start(HWND parent);

private:
	MasterRelease_ptr m_master_release;
	int m_pos;
	pfc::string8 m_offlineArtist_id;
	int m_query_mode;
	pfc::string8 m_strFilter;
	bool m_force_redraw;
	bool m_force_rebuild;
    HWND m_owner_window = nullptr; // Only the originating live dialog may be updated.

	void safe_run(threaded_process_status& p_status, abort_callback& p_abort) override;
	void on_success(HWND p_wnd) override;
	void on_abort(HWND p_wnd) override;
	void on_error(HWND p_wnd) override;
};


// Presentation-only follow-up of search_artist_process_callback::on_success.
// Parent still owns the search lock; acquiring it again would throw
// "Operation locked" and suppress otherwise valid Discogs results.
class on_search_artist_done_process_callback : public foo_discogs_threaded_process_callback {

public:
	on_search_artist_done_process_callback(const pfc::array_t<Artist_ptr> p_artist_exact_matches,
		const pfc::array_t<Artist_ptr> p_artist_other_matches,
		bool append, std::pair<size_t, size_t>& out_va_cap) :
		m_p_artist_exact_matches(p_artist_exact_matches), m_p_artist_other_matches(p_artist_other_matches), m_append(append), m_out_va_cap(out_va_cap) {};

	void start(HWND parent);

private:

	const pfc::array_t<Artist_ptr> m_p_artist_exact_matches;
	const pfc::array_t<Artist_ptr> m_p_artist_other_matches;
	bool m_append;
	std::pair<size_t, size_t> m_out_va_cap;
    HWND m_owner_window = nullptr; // Captured before dispatch, checked at UI completion.

	void safe_run(threaded_process_status& p_status, abort_callback& p_abort) override;
	void on_success(HWND p_wnd) override;
	void on_abort(HWND p_wnd) override;
	void on_error(HWND p_wnd) override;
};

class on_get_artist_done_process_callback : public foo_discogs_threaded_locked_process_callback {

public:
	on_get_artist_done_process_callback(const Artist_ptr artist, cupdRelSrc updsrc) :
		 m_artist(artist), m_updsrc(updsrc) {};

	void start(HWND parent);

private:

	cupdRelSrc m_updsrc;
	Artist_ptr m_artist;
    HWND m_owner_window = nullptr; // Do not target a replaced modeless dialog.

	void safe_run(threaded_process_status& p_status, abort_callback& p_abort) override;
	void on_success(HWND p_wnd) override;
	void on_abort(HWND p_wnd) override;
	void on_error(HWND p_wnd) override;
};


class process_release_callback : public foo_discogs_threaded_process_callback
{
public:
	process_release_callback(CFindReleaseDialog *dialog, const pfc::string8 &release_id, const pfc::string8& offline_artist_id, pfc::string8 inno, const metadb_handle_list &items);
	void start(HWND parent);

private:
	TagWriter_ptr m_tag_writer;
	file_info_manager_ptr m_finfo_manager;
	metadb_handle_list m_items;

	CFindReleaseDialog *m_dialog;
	pfc::string8 m_release_id;
	pfc::string8 m_offline_artist_id;
	pfc::string8 m_inno;
	Release_ptr m_release;

	void generate_track_mapping(TagWriter& tagwriter, const Release *p_release);

	void safe_run(threaded_process_status &p_status, abort_callback &p_abort) override;
	void on_success(HWND p_wnd) override;
	void on_abort(HWND p_wnd) override;
	void on_error(HWND p_wnd) override;
};


class process_artwork_preview_callback : public foo_discogs_threaded_process_callback
{
public:
	process_artwork_preview_callback(CTrackMatchingDialog*dialog, const Release_ptr &release, const size_t img_ndx, const bool bartist, const bool onlycache, const bool get_mibs = false);
	void start(HWND parent);

private:
	MemoryBlock m_small_art;
	CTrackMatchingDialog* m_dialog;
	Release_ptr m_release;
	size_t m_img_ndx;
	bool m_bartist;
	bool m_onlycache;
	bool m_get_mibs;

	musicbrainz_info m_musicbrainz_mibs;

	void safe_run(threaded_process_status& p_status, abort_callback& p_abort) override;
	void on_success(HWND p_wnd) override;
	void on_abort(HWND p_wnd) override;
	void on_error(HWND p_wnd) override;
};


class process_file_artwork_preview_callback : public foo_discogs_threaded_process_callback
{
public:
	process_file_artwork_preview_callback(CTrackMatchingDialog* dialog, const Release_ptr& m_release, const metadb_handle_list items, const size_t img_ndx, const bool bartist);
	void start(HWND parent);

private:
	imgpairs m_small_art;
	std::pair<pfc::string8, pfc::string8> m_temp_file_names;
	CTrackMatchingDialog* m_dialog;
	pfc::string8 m_release_id;
	Release_ptr m_release;
	metadb_handle_list m_items;
	size_t m_img_ndx;
	bool m_bartist;

	void safe_run(threaded_process_status& p_status, abort_callback& p_abort) override;
	void on_success(HWND p_wnd) override;
	void on_abort(HWND p_wnd) override;
	void on_error(HWND p_wnd) override;
};


class test_oauth_process_callback : public foo_discogs_threaded_process_callback
{
public:
	test_oauth_process_callback(const pfc::string8 &token, const pfc::string8 &token_secret) : test_token(token), test_token_secret(token_secret) {}
	void start(HWND parent);

private:
	const pfc::string8 &test_token;
	const pfc::string8 &test_token_secret;

	void safe_run(threaded_process_status &p_status, abort_callback &p_abort) override;
	void on_success(HWND p_wnd) override;
	void on_error(HWND p_wnd) override;
};


class authorize_oauth_process_callback : public foo_discogs_threaded_process_callback
{
public:
	authorize_oauth_process_callback() {}
	void start(HWND parent);

private:
	void safe_run(threaded_process_status &p_status, abort_callback &p_abort) override;
};


class generate_oauth_process_callback : public foo_discogs_threaded_process_callback
{
public:
	generate_oauth_process_callback(pfc::string8 pin_code) : code(pin_code), token(nullptr) {}
	void start(HWND parent);

private:
	pfc::string8 code;
	OAuth::Token *token;

	void safe_run(threaded_process_status &p_status, abort_callback &p_abort) override;
	void on_success(HWND p_wnd) override;
};
