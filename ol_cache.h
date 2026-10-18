#pragma once
#include <fcntl.h>
#include <io.h>
#include <ctime>

#include <errno.h>
#include <filesystem>
#include "jansson/jansson.h"
#include "utils.h"
#include "utils_path.h"
#include "track_matching_utils.h"

namespace Offline {

	namespace fs = std::filesystem;

	const pfc::string8 MARK_LOADING_NAME = "loading";
	const pfc::string8 MARK_CHECK_NAME = "taskreg.txt";

	enum CacheFlags {
		OC_READ = 1 << 0,
		OC_WRITE = 1 << 1,
		OC_OVERWRITE = 1 << 2,

		MERGE_HIDDEN_TRACKS = 1 << 4, //todo
		MERGE_SUBTRACKS = 1 << 5,
		MERGE_TRACKS_TO_HEADER = 1 << 6,
	};

	enum GetFrom {
		ArtistReleases,
		Versions,
		Artist,
		Release,
		Thumbs,
	};

	static bool can_read(const foo_conf& conf) { return conf.cache_offline_cache_flag & CacheFlags::OC_READ; }
	static bool can_read() { return CONF.cache_offline_cache_flag & CacheFlags::OC_READ; }
	static bool can_write() { return CONF.cache_offline_cache_flag & CacheFlags::OC_WRITE; }
	static bool can_ovr() { return CONF.cache_offline_cache_flag & CacheFlags::OC_OVERWRITE; }
	static bool full_cache() { return can_read() && can_write(); }

	static pfc::string8 get_offline_path(pfc::string8 id, GetFrom gfFrom, pfc::string8 secId, bool native) {

		pfc::string8 ol_path(core_api::pathInProfile(OC_NAME));

		switch(gfFrom) {
		case GetFrom::Artist:
			ol_path << "\\artists\\" << id << "\\artist";
			break;
		case GetFrom::ArtistReleases:
			ol_path << "\\artists\\" << id << "\\releases";
			break;
		case GetFrom::Release:
			ol_path << "\\artists\\" << id << "\\release\\" << secId;
			break;
		case GetFrom::Versions:
			ol_path << "\\artists\\" << id << "\\masters\\" << secId << "\\versions";
			break;
		case GetFrom::Thumbs:
			if (!secId.get_length()) {
				ol_path << "\\thumbnails\\artists\\" << id;
			}
			else {
				if (!id.get_length()) {
					ol_path << "\\thumbnails\\releases\\" << secId;
				}
				else {
					//..
				}
			}
			break;
		default:{
			PFC_ASSERT(false);
		}
		}

		if (native) {
			extract_native_path(ol_path, ol_path);
		}

		return ol_path;
	}

	static pfc::string8 get_thumbnail_cache_path(pfc::string8 id, art_src artSrc, bool native) {

		pfc::string8 path =  profile_path(OC_NAME, true);
		path << "\\thumbnails\\";
		path << (artSrc == art_src::alb ? "releases" :	artSrc == art_src::art ? "artists" : "unknown") << "\\" << id;
		return path;
	}

	static pfc::array_t<pfc::string8> get_thumbnail_cache_path_filenames(pfc::string8 id, art_src artSrc, size_t iImageList, bool native, size_t ndx) {

		pfc::array_t<pfc::string8> folders;
		pfc::array_t<pfc::string8> filenames;

		pfc::string8 n8_rel_path = get_thumbnail_cache_path(id, artSrc, native);

		pfc::string8 n8_full_path(n8_rel_path);
		n8_full_path << "\\thumb_" << (iImageList == LVSIL_NORMAL ? "150x150" : "48x48") << "_";

		if (ndx == SIZE_MAX) {

			filenames.append_single(n8_full_path);
			//..
			return filenames;
			//..
		}

		n8_full_path << ndx << THUMB_EXTENSION;

		std::filesystem::path os_path = std::filesystem::u8path(n8_rel_path.get_ptr());

		try {

			std::filesystem::directory_iterator dirpos{ os_path };

			for (std::filesystem::directory_entry walk_dir : dirpos) {

				auto u8str = walk_dir.path().u8string();

				folders.add_item(u8str.c_str());
			}

		}
		catch (std::filesystem::filesystem_error e) {
			//log_msg(e.what());
			return filenames;
		}
		catch (...) {
			return filenames;
		}

		for (t_size walk = 0; walk < folders.get_size(); ++walk) {
			pfc::string8 tmp_path(folders[walk]);
			if (!stricmp_utf8(tmp_path, n8_full_path)) {
				filenames.append_single(tmp_path);
				break;
			}
		}
		return filenames;
	}

	static pfc::string8 get_offline_pages_path(pfc::string8 id, size_t page, GetFrom getFrom, pfc::string8 secid, bool native) {

		PFC_ASSERT(getFrom == GetFrom::ArtistReleases || getFrom == GetFrom::Versions);
		PFC_ASSERT(getFrom != GetFrom::Versions || (!STR_EQUAL(id, secid) && secid.get_length()));

		pfc::string8 page_path(get_offline_path(id, getFrom, secid, native));
		page_path << "\\page-";

		if (page != SIZE_MAX) {
			page_path << page;
		}

		return page_path;
	}

	static bool check_offline_entity_folder(pfc::string8 id, GetFrom getFrom, pfc::string8 secid) {

		PFC_ASSERT(getFrom == GetFrom::Artist || getFrom == GetFrom::Release || getFrom == GetFrom::ArtistReleases || getFrom == GetFrom::Versions);
		PFC_ASSERT((getFrom != GetFrom::Release && getFrom != GetFrom::Versions) || (!STR_EQUAL(id, secid) && secid.get_length()));

		pfc::string8 n8_path = get_offline_path(id, getFrom, secid, true);
		fs::path os_path = fs::u8path(n8_path.c_str());

		bool debug_path_exists = false;
		bool req_check = false;

		size_t req_files;
		size_t req_dirs;

		if (getFrom == GetFrom::ArtistReleases || getFrom == GetFrom::Versions) {
			req_files = 1;
			req_dirs = 1;
		}
		else {
			req_files = 2;
			req_dirs = 0;
		}

		try
		{
			if (fs::exists(os_path) && fs::is_directory(os_path))
			{
				fs::recursive_directory_iterator dirpos{ os_path };

				for (std::filesystem::directory_entry walk_dir : dirpos) {

					if (walk_dir.is_regular_file()) {
						req_files--;
					}
					else if (walk_dir.is_directory()) {
						req_dirs--;
					}

					req_check = req_files < 1 && req_dirs < 1;

					if (req_check) break;
				}
			}
		}
		catch (const fs::filesystem_error& err)
		{
			return false;
		}
		catch (const std::exception& ex)
		{
			return false;
		}
		return req_check;
	}

#ifdef CACHE_EXPIRATION
	inline bool check_expired(pfc::string8 &dst_buf, size_t src_length_checked) {

		bool expired = false;
		size_t cr_pos = dst_buf.find_first('\n');

		if (CONF.expiration_enabled()) {

			// first carriage return ?

			if (cr_pos < src_length_checked) {
				dst_buf = dst_buf.subString(cr_pos);
				size_t sp_pos = dst_buf.find_first(" ");

				// first space after first carriage return

				if (sp_pos < dst_buf.get_length() - 1) {
					//"2024-09-30T16:02:39+02000" -> len 24
					dst_buf = dst_buf.subString(sp_pos + 1, 25 - 1);

					struct tm* tmStruct = {};

					char buffer[25];
					strncpy(buffer, dst_buf.c_str(), 24);
					buffer[24] = '\0';

					std::tm tm_stored;
					std::stringstream ss(buffer);

					try {

						// parse stored time
						ss >> std::get_time(&tm_stored, "%Y-%m-%dT%H:%M:%S%z");

						time_t t_stored;
						t_stored = mktime(&tm_stored);

						auto t_now = std::time(nullptr);

						double difference = std::difftime(t_stored, t_now) / (60 * 60 * 24);
						expired = (difference + (CONF.expiration_days())) <= 0;

					}
					catch (...) {
						expired = false;
					}
				}
			}
		}

		return expired;
	}
#endif
#ifdef CACHE_EXPIRATION
	inline void add_now_stamp(pfc::string8& out) {

		//2004-05-22T12:35:02-07:00
		auto t = std::time(nullptr);
		auto tm = *std::localtime(&t);

		std::ostringstream oss;
		oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S%z");
		pfc::string8 utf_created = oss.str().c_str();

		oss.clear();
		oss.str("");
		pfc::string8 utf_expires;
		if (CONF.expiration_enabled()) {

			size_t def_expires = CONF.expiration_days() * 24 * 60 * 60;

			t += def_expires;
			tm = *std::localtime(&t);

			oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S%z");
			utf_expires = oss.str().c_str();
		}

		out = PFC_string_formatter() << out << "\n" << utf_created;

		if (utf_expires.get_length()) {
			out << " " << utf_expires;
		}
	}
#endif

	//sets folder job: 'loading.' or 'TaskReg.txt' (done param value)

	bool static stamp_download(pfc::string8 fcontent, pfc::string8 path, bool done) {

		bool bok = false;
		bool brelease = false;

		if (!done) {

			//delete if exists

			fs::path os_file = fs::u8path(path.c_str());

			try {
				fs::remove_all(os_file);
			}
			catch (...) {
				return false;
			}

			//pending

			bool bfolder_created = false;

			std::error_code ec;

			try {
				bfolder_created = fs::create_directories(os_file, ec);
			}
			catch (...) {
				return false;
			}

			if (!ec.value() && bfolder_created) {

				os_file = fs::u8path((PFC_string_formatter() << path << "\\" << MARK_LOADING_NAME).c_str());

				int flags = 0; // JSON_ENCODE_ANY | JSON_INDENT(1);

				auto jf = _wopen(os_file.wstring().c_str(), _O_CREAT | _O_TRUNC | _O_RDWR | _O_TEXT/*_O_U8TEXT*/, _S_IWRITE);

				if (jf != -1) {
#ifdef CACHE_EXPIRATION
					if (CONF.expiration_enabled()) {
						add_now_stamp(fcontent);
					}
#endif
					int w = _write(jf, fcontent.get_ptr(), fcontent.get_length());
					bok = ((w || !fcontent.get_length()) && !_close(jf));
					return bok;
				}
			}

			bok = false;
		}
		else {

			//done

			pfc::string8 oldname(path); oldname << "\\" << MARK_LOADING_NAME;
			pfc::string8 newname(path); newname << "\\" << MARK_CHECK_NAME;
			fs::path os_old = fs::u8path(oldname.c_str());
			fs::path os_new = fs::u8path(newname.c_str());

			std::error_code ec;

			try {
				fs::rename(os_old, os_new, ec);
			}
			catch (...) {
				return false;
			}

			bok = !ec.value();
		}

		return bok;
	}

	bool static check_download(pfc::string8 path, bool &out_expired) {

		pfc::string8 path_loading;
		pfc::string8 path_taskreg;

		path_loading << path << "\\" << MARK_LOADING_NAME;
		path_taskreg << path << "\\" << MARK_CHECK_NAME;

		bool stat_result_loading = false;
		int src_length_loading = -1;
		int src_length_checked = -1;

		fs::path p_loading = fs::u8path(path_loading.get_ptr());

		if (fs::exists(p_loading)) {
			stat_result_loading = true;
			src_length_loading = fs::file_size(p_loading);
		}

		fs::path p_taskreg = fs::u8path(path_taskreg.get_ptr());

		bool stat_result_checked = fs::exists(p_taskreg);

		if (stat_result_checked) {

			src_length_checked = fs::file_size(p_taskreg);

#ifdef CACHE_EXPIRATION
			if (CONF.expiration_enabled() && stat_result_checked && src_length_checked > 0) {

				stat_result_checked = false;

				errno = 0;
				auto jf = _wopen(p_taskreg.wstring().c_str(), _O_RDONLY | _O_TEXT);

				if (jf != -1) {

					char buffer[MAX_PATH];
					pfc::string8 dst_buf;
					int res = _read(jf, buffer, src_length_checked);
					stat_result_checked = res > 0;

					dst_buf.add_string(buffer);
					out_expired = check_expired(dst_buf, src_length_checked);
					stat_result_checked &= !out_expired;

					close(jf);
				}

				if (errno) {
					log_msg(PFC_string_formatter() << "can't open " << path_taskreg);
				}
			}
#endif
		}

		return !stat_result_loading &&(stat_result_checked && src_length_checked != -1);
	}

	// checks precondition and stamp

	static bool is_data_avail(pfc::string8 id, pfc::string8 secid, GetFrom gfFrom, pfc::string8& out_relative_path, bool native, bool& expired) {

		// precondition

		if (atoi(id) == pfc_infinite || !id.get_length() || !is_number(id.c_str())) return false;
		if ((gfFrom == GetFrom::Release) || (gfFrom == GetFrom::Versions)) {
			if (atoi(secid) == pfc_infinite || !secid.get_length() || !is_number(secid.c_str())) return false;
		}

		//..

		out_relative_path = get_offline_path(id, gfFrom, secid, native);
		bool bres = check_offline_entity_folder(id, gfFrom, secid);
		bres &= check_download(out_relative_path, expired);

		return bres;
	}

	//create offline folders for paged json content/artwork
	//returns true if folder is available after check/creation
	bool static create_offline_subpage_folder(pfc::string8 id, art_src artSrc, size_t subpage, GetFrom getFrom, pfc::string8 secid, bool thumb_type = false) {

		PFC_ASSERT(getFrom == GetFrom::ArtistReleases || getFrom == GetFrom::Versions);
		PFC_ASSERT(getFrom != GetFrom::Versions || (!STR_EQUAL(id, secid) && secid.get_length()));

		pfc::string8 n8_rel_path;

		if (subpage == SIZE_MAX) {
			if (thumb_type)
				n8_rel_path = get_thumbnail_cache_path(id, artSrc, true);
			else {
				n8_rel_path = get_offline_path(id, getFrom, secid, true);
			}
		}
		else {
			n8_rel_path = get_offline_pages_path(id, subpage, getFrom, secid, true);
		}

		fs::path os_path = fs::u8path(n8_rel_path.c_str());

		std::error_code ec;

		if (fs::exists(os_path, ec)) {
			return !ec && fs::is_directory(os_path, ec) && !ec;
		}
		if (ec) return false;

		// create_directories() returns false if the directory already exists.
		// Validate the resulting path rather than treating that as failure.
		fs::create_directories(os_path, ec);
		return !ec && fs::is_directory(os_path, ec) && !ec;
	}

	// create offline path for artist and release ids

	bool static create_offline_entity_folder(pfc::string8 id, GetFrom getFrom, pfc::string8 secid = "") {

		PFC_ASSERT(getFrom == GetFrom::Artist || getFrom == GetFrom::Release);
		PFC_ASSERT(getFrom != GetFrom::Release || ((!STR_EQUAL(id, secid) || std::atoi(secid) < 4 ) && secid.get_length()));

		pfc::string8 n8_rel_path = get_offline_path(id, getFrom, secid, true);

		fs::path os_path = fs::u8path(n8_rel_path.c_str());

		try {
			std::error_code ec;
			bool bdone = fs::create_directories(os_path, ec);
			return !ec.value();
		}
		catch (...) {
			return false;
		}
	}


	class ol_cache
	{

	public:

		ol_cache() {

			//..

		}

		json_t* Read_JSON(const char* offlinepath = nullptr) const {

			pfc::string8 path = offlinepath;
			pfc::string8 n8_cache_prefix = profile_path(OC_NAME, true);

			log_msg(path.replace(n8_cache_prefix, "(cache)"));

			fs::path os_ol = fs::u8path(path.c_str());

			int srclen = -1;

			if (fs::exists(os_ol)) {

				srclen = fs::file_size(os_ol);

			}

			json_t* root = json_array();

			if (srclen > 0) {

				errno = 0;
				json_error_t json_error = { 0 };

				int jf = -1;
				jf = _wopen(os_ol.wstring().c_str(), _O_RDONLY);

				if (jf != -1) {
					root = json_loadfd(jf, JSON_DECODE_ANY, &json_error);
					_close(jf);
				}

				if (errno || strlen(json_error.text) || !root) {

					pfc::string8 err_msg = "removing non-valid offline cache file: ";
					err_msg << path;
					if (strlen(json_error.text)) {
						err_msg << json_error.text;
					}
					log_msg(err_msg);

					try {

						pfc::string8 debug;
						debug << os_ol.remove_filename().c_str();
						fs::remove_all(os_ol.remove_filename());
					}
					catch (...) {
						//
					}
				}
			}
			return root;
		}

		// serves offline_cache_save

		bool Dump_JSON(pfc::string8 path, json_t* root) {

			const fs::path os_full = fs::u8path(path.c_str());
			const fs::path os_parent = os_full.parent_path();

			std::error_code ec;

			bool bfolder_ready = fs::exists(os_parent, ec);

			if (!bfolder_ready && !ec.value()) {

				bfolder_ready = fs::create_directories(os_parent, ec);
			}

			if (bfolder_ready) {

				int flags = 0; 

				int jf = _wopen(os_full.wstring().c_str(), _O_CREAT | _O_TRUNC | _O_RDWR | _O_TEXT/*_O_U8TEXT*/, _S_IWRITE);

				if (jf != -1) {
					//-1 or 0
					int result = json_dumpfd(root, jf, flags);
					return (!_close(jf) && !(bool)result);
				}

			}
			return false;
		}

	private:

		//..
	};
}