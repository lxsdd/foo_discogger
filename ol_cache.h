#pragma once
#include <fcntl.h>
#include <io.h>
#include <ctime>

#include <errno.h>
#include <filesystem>
#include "discogger_cache_expiry.h"
#include "discogger_atomic_cache_file.h"
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

				const auto u8str = walk_dir.path().u8string();
				// Copy UTF-8 code units into the narrow-byte SDK string; char8_t* is not char* in C++20.
				const std::string utf8_path(u8str.begin(), u8str.end());
				folders.add_item(utf8_path.c_str());
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
					int w = _write(jf, fcontent.get_ptr(), fcontent.get_length());
					const int closed = _close(jf);
                    bok = w >= 0 && static_cast<size_t>(w) == fcontent.get_length() && closed == 0;
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

    // Check the completed marker's timestamp without reading unbounded content.
    bool static check_download(pfc::string8 path, bool& out_expired, GetFrom kind) {
        out_expired = false;
        std::error_code ec;
        const fs::path root = fs::u8path(path.c_str());
        const fs::path loading = root / MARK_LOADING_NAME.c_str();
        const fs::path complete = root / MARK_CHECK_NAME.c_str();
        if (fs::exists(loading, ec) || ec) return false;
        if (!fs::is_regular_file(complete, ec) || ec) return false;
        const auto stamp = fs::last_write_time(complete, ec);
        if (ec) { out_expired = true; return false; }
        const auto age = std::chrono::duration_cast<std::chrono::seconds>(
            fs::file_time_type::clock::now() - stamp);
        const auto category = kind == GetFrom::ArtistReleases
            ? discogger_cache::group::artist_releases : discogger_cache::group::other_entity;
        const auto lifetime = discogger_cache::ttl(
            category, CONF.artist_release_cache_hours,
            CONF.expiration_enabled(), CONF.expiration_days());
        out_expired = discogger_cache::expired(age, lifetime);
        return !out_expired;
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
		bres &= check_download(out_relative_path, expired, gfFrom);

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

			// Do not allocate a placeholder before a successful json_loadfd:
            // overwriting that pointer leaked a Jansson reference on every hit.
            json_t* root = nullptr;

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

                    // One bad JSON page must not erase other cached pages.
                    // The reader will reject it and fetch the entity online.
                    if (root) {
                        json_decref(root);
                        root = nullptr;
                    }
                }
            }
            // Keep the historical empty-array fallback for missing/bad cache data.
            // Callers that require an object must validate its type explicitly.
            return root ? root : json_array();
		}

		// serves offline_cache_save

		bool Dump_JSON(pfc::string8 path, json_t* root) {
            if (!root) return false;
            const fs::path destination = fs::u8path(path.c_str());
            return discogger_cache::atomic_cache_replace(destination,
                [root](const fs::path& temporary) {
                    // Exclusive creation, full serialization, disk flush,
                    // close, then atomic publication of the new JSON file.
                    const int jf = _wopen(temporary.wstring().c_str(),
                        _O_CREAT | _O_EXCL | _O_WRONLY | _O_BINARY,
                        _S_IREAD | _S_IWRITE);
                    if (jf == -1) return false;
                    const bool serialized = json_dumpfd(root, jf, 0) == 0;
                    const bool synced = serialized && _commit(jf) == 0;
                    const bool closed = _close(jf) == 0;
                    return serialized && synced && closed;
                });
        }

	private:

		//..
	};
}