#pragma once
#include <fcntl.h>
#include <io.h>
#include <ctime>

#include <errno.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <cstdint>
#include "discogger_cache_expiry.h"
#include "discogger_atomic_cache_file.h"
#include "discogger_directory_cache.h"
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
		const fs::path os_path = fs::u8path(n8_path.c_str());

		const bool paged = getFrom == GetFrom::ArtistReleases || getFrom == GetFrom::Versions;
		// A shape preflight only: is_data_avail() still requires the completed
		// marker, freshness and contiguous page/payload validation separately.
		return discogger_cache::cache_has_minimum_entries(
			os_path, paged ? 1u : 2u, paged ? 1u : 0u);
	}


    // Read with this module's C++ stream, then parse bytes with Jansson.
    // Passing CRT file descriptors across native library boundaries can
    // reject valid JSON if the component and Jansson use different CRTs.
    // Bound untrusted cache files before allocation; fail closed on changes.
    static json_t* load_cache_json(const fs::path& path, json_error_t* error = nullptr) {
        std::error_code ec;
        if (fs::is_symlink(path, ec) || ec ||
            !fs::is_regular_file(path, ec) || ec) return nullptr;
        const auto size = fs::file_size(path, ec);
        constexpr std::uintmax_t max_cache_json_bytes = 128ull * 1024ull * 1024ull;
        if (ec || size == 0 || size > max_cache_json_bytes) return nullptr;
        std::ifstream input(path, std::ios::binary);
        if (!input) return nullptr;
        std::string bytes(static_cast<std::size_t>(size), '\0');
        input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        if (input.gcount() != static_cast<std::streamsize>(bytes.size()) ||
            input.bad()) return nullptr;
        if (input.peek() != std::char_traits<char>::eof() || input.bad())
            return nullptr;
        return json_loadb(bytes.data(), bytes.size(), JSON_DECODE_ANY, error);
    }

    static bool cache_payload_is_valid_json(const fs::path& path) {
        json_error_t error = {};
        json_t* parsed = load_cache_json(path, &error);
        if (!parsed) return false;
        json_decref(parsed);
        return true;
    }

    // Directory cache transactions supersede destructive stamp_download.

    // Check the completed marker's timestamp without reading unbounded content.
    bool static check_download(pfc::string8 path, bool& out_expired, GetFrom kind) {
        out_expired = false;
        std::error_code ec;
        const fs::path root = fs::u8path(path.c_str());
        // A timestamp alone is not evidence that every paged JSON file
        // survived an interrupted download. Treat incomplete caches as misses.
        if (!discogger_cache::cache_has_marker(root)) return false;
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


    static pfc::string8 staged_cache_json_path(
        const discogger_cache::directory_cache_transaction& tx, size_t page = SIZE_MAX) {
        fs::path file = tx.staging_directory();
        if (page != SIZE_MAX) file /= "page-" + std::to_string(page);
        file /= "root.json";
        const auto utf8 = file.u8string();
        const std::string bytes(utf8.begin(), utf8.end());
        return pfc::string8(bytes.c_str());
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
        if (!discogger_cache::recover_cache_directory(fs::u8path(out_relative_path.c_str()),
                cache_payload_is_valid_json))
            return false;
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

			// Recovery and the ordinary reader must agree on valid JSON.
            json_error_t json_error = {};
            json_t* root = load_cache_json(os_ol, &json_error);
            if (!root) {
                pfc::string8 err_msg = "invalid or unreadable offline cache file: ";
                err_msg << path;
                if (strlen(json_error.text)) err_msg << json_error.text;
                log_msg(err_msg);
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
                    // Jansson's fd writer may use a different Windows CRT
                    // than the one that opened jf. Keep all descriptor I/O
                    // in the component CRT; Jansson only emits byte chunks.
                    const bool serialized = json_dump_callback(root,
                        [](const char* data, size_t length, void* context) -> int {
                            const int fd = *static_cast<int*>(context);
                            while (length) {
                                const unsigned int chunk = static_cast<unsigned int>(
                                    length > 1024u * 1024u ? 1024u * 1024u : length);
                                const int written = _write(fd, data, chunk);
                                if (written <= 0) return -1;
                                data += written;
                                length -= static_cast<size_t>(written);
                            }
                            return 0;
                        }, &jf, 0) == 0;
                    const bool synced = serialized && _commit(jf) == 0;
                    const bool closed = _close(jf) == 0;
                    return serialized && synced && closed;
                });
        }

	private:

		//..
	};
}