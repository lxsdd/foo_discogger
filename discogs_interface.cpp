#include "stdafx.h"
#include "discogger_search_fallback.h"
#if defined(DISCOGGER_RUNNER_MEDIA_QA)
#include "tests/runner-live-search-telemetry.h"
#endif
#include <fstream>
#include "discogger_cache_pages.h"
#include <cstdlib>

#include "pfc/other.h"
#include "discogs_interface.h"
#include "exception.h"
#include "utils.h"
#include "conf.h"

namespace ol = Offline;

Release_ptr DiscogsInterface::get_release(const size_t lkey, bool bypass_is_cache, bool bypass) {

	pfc::string8 release_id = std::to_string(decode_mr(lkey).second).c_str();
	assert_release_id_not_deleted(release_id);

	Release_ptr release = bypass_is_cache && bypass ? nullptr : get_release_from_cache(lkey);

	if (!release) {

		std::pair<int, unsigned long> dec = decode_mr(lkey);
		release = std::make_shared<Release>(release_id);

		if (!(bypass_is_cache && bypass)) {
			add_release_to_cache(lkey, release);
		}
	}
	return release;
}

Release_ptr DiscogsInterface::get_release(const size_t lkey, threaded_process_status& p_status, abort_callback& p_abort, bool bypass_cache, bool throw_all) {
	//todo: used by depricate codes (ej. bulk updates), 
	const pfc::string8 dummy_offline_artist_id;
	return get_release(lkey, dummy_offline_artist_id, p_status, p_abort, bypass_cache, throw_all);
}

Release_ptr DiscogsInterface::get_release(const size_t lkey, const pfc::string8 &offline_artist_id, threaded_process_status &p_status, abort_callback &p_abort, bool bypass_cache, bool throw_all) {

	pfc::string8 release_id = std::to_string(decode_mr(lkey).second).c_str();
	assert_release_id_not_deleted(release_id);

	Release_ptr release = bypass_cache ? nullptr : get_release_from_cache(lkey);

	if (!release) {
		release = std::make_shared<Release>(release_id);
		if (!bypass_cache) {
			add_release_to_cache(lkey, release);
		}
	}

	std::lock_guard<std::mutex> ul(cache_releases->modify_mutex);
	if (!release->loaded) {
		try {
			release->load(p_status, p_abort, throw_all, offline_artist_id /*, nullptr*/);
		}
		catch (http_404_exception) {
			add_deleted_release_id(release_id);
			throw;
		}
	}
	return release;
}
MasterRelease_ptr DiscogsInterface::get_master_release(const size_t lkey, bool bypass_cache) {

	MasterRelease_ptr master = bypass_cache ? nullptr : get_master_release_from_cache(lkey);

	if (!master) {

		auto decoded_mr = decode_mr(lkey);
		pfc::string8 strlkey_master = std::to_string(decoded_mr.second).c_str();

		master = std::make_shared<MasterRelease>(strlkey_master);
		if (!bypass_cache) {
			add_master_release_to_cache(lkey, master);
		}
	}
	return master;
}

MasterRelease_ptr DiscogsInterface::get_master_release(const pfc::string8& master_id, const pfc::string8& artist_id, bool bypass_cache) {

	unsigned long lkey = encode_mr(atoi(artist_id), atoi(master_id));

	MasterRelease_ptr master = bypass_cache ? nullptr : get_master_release_from_cache(lkey);

	if (!master) {
		master = std::make_shared<MasterRelease>(master_id);
		if (!bypass_cache) {
			add_master_release_to_cache(lkey, master);
		}
	}
	return master;
}

MasterRelease_ptr DiscogsInterface::get_master_release(const pfc::string8& master_id, bool bypass_cache) {

	size_t lkey = encode_mr(0, atol(master_id));

	MasterRelease_ptr master = bypass_cache ? nullptr : get_master_release_from_cache(lkey);

	if (!master) {
		master = std::make_shared<MasterRelease>(master_id);
		if (!bypass_cache) {
			add_master_release_to_cache(lkey, master);
		}
	}
	return master;
}

MasterRelease_ptr DiscogsInterface::get_master_release(const pfc::string8 &master_id, threaded_process_status &p_status, abort_callback &p_abort, bool bypass_cache, bool throw_all) {

	size_t lkey = encode_mr(0, atol(master_id));

	MasterRelease_ptr master = bypass_cache ? nullptr : get_master_release_from_cache(lkey);
	if (!master) {
		master = std::make_shared<MasterRelease>(master_id);
		if (!bypass_cache) {
			add_master_release_to_cache(lkey, master);
		}
	}

	if (!master->loaded) {
		try {
			master->load(p_status, p_abort, throw_all);
		}
		catch (http_404_exception) {
			throw;
		}
	}
	return master;
}

Artist_ptr DiscogsInterface::get_artist(const pfc::string8 &artist_id, bool bypass_cache) {

	Artist_ptr artist = bypass_cache ? nullptr : get_artist_from_cache(artist_id);

	if (!artist) {
		artist = std::make_shared<Artist>(artist_id);
		if (!bypass_cache) {
			add_artist_to_cache(artist);
		}
	}
	return artist;
}

Artist_ptr DiscogsInterface::get_artist(const pfc::string8 &artist_id, bool _load_releases, threaded_process_status &p_status, abort_callback &p_abort, bool bypass_cache, bool throw_all, bool throw_404, bool mem_only) {

	Artist_ptr artist = bypass_cache ? nullptr : get_artist_from_cache(artist_id);

	if (!artist) {
		artist = std::make_shared<Artist>(artist_id);
		if (!bypass_cache) {
			add_artist_to_cache(artist);
		}
	}

	if (!artist->loaded) {
		try {
			if (mem_only) {
				artist->load_mem_only(p_status, p_abort, throw_all);
			}
			else {
				artist->load(p_status, p_abort, throw_all);
			}
		}
		catch (http_404_exception) {
			if (throw_404)
				throw;
			else {
				artist->profile = "404";
				return artist;
			}
		}
	}

	if (_load_releases) {
		artist->load_releases(p_status, p_abort, false, nullptr);
	}
	return artist;
}

void DiscogsInterface::search_artist(const pfc::string8 &query, pfc::array_t<Artist_ptr> &exact_matches, pfc::array_t<Artist_ptr> &other_matches, threaded_process_status &p_status, abort_callback &p_abort) {

	pfc::string8 json;

	exact_matches.force_reset();
	other_matches.force_reset();

	pfc::array_t<Artist_ptr> good_matches;
	pfc::array_t<Artist_ptr> wtf_matches;
	pfc::array_t<Artist_ptr> matches;

	pfc::string8 params;
	params << "type=artist&q=" << urlEscape(query) << "&per_page=100";

	fetcher->fetch_html("https://api.discogs.com/database/search", params, json, p_abort);

	JSONParser jp(json);
	parseArtistResults(jp.root, matches);

	pfc::string8 lowercase_query = lowercase(query);
	pfc::string8 stripped_query = strip_artist_name(query);

	for (size_t i = 0; i < matches.get_size() && i < MAX_ARTISTS; i++) {
		Artist_ptr &artist = matches[i];
		pfc::string8 lowercase_name = lowercase(artist->name);
		pfc::string8 stripped_name = strip_artist_name(artist->name);
		if (lowercase_query == lowercase_name) {
			exact_matches.append_single(std::move(artist));
		}
		else if (stripped_query == stripped_name) {
			good_matches.append_single(std::move(artist));
		}
		else if (lowercase_name.find_first(stripped_query) != pfc::infinite_size) {
			other_matches.append_single(std::move(artist));
		}
		else {
			wtf_matches.append_single(std::move(artist));
		}
	}
	exact_matches.append(good_matches);
	other_matches.append(wtf_matches);
}

void DiscogsInterface::process_amt_parsed_fake_artist(Artist_ptr fakeArtist, parse_amt_info& pai, bool load_master_preview, bool load_release_preview,
	std::pair<size_t, size_t>& va_res_cap, std::pair<size_t, size_t>& va_done_cap, threaded_process_status& p_status, abort_callback& p_abort) {

	const size_t k_limit_master_traffic = LOWORD(CONF.query_max);
	const size_t k_limit_release_traffic = LOWORD(CONF.query_max);

	size_t ctraffic = 0;

	const std::string query_artist = pai.qdm_search_query.at("artist=").first;

	va_res_cap.first = fakeArtist->master_releases.get_count();
	va_res_cap.second = fakeArtist->releases.get_count();

	Artist_ptr ukArtist;
	Artist_ptr ukmArtist;

	ukArtist = std::make_shared<Artist>(search_query::k_uk_id);
	ukmArtist = std::make_shared<Artist>(search_query::k_ukm_id);

	ukArtist->name = search_query::k_uk_artist_name;
	ukmArtist->name = search_query::k_ukm_artist_name;
	ukArtist->loaded = true;
	ukArtist->loaded_preview = true;
	ukArtist->loaded_releases = false;
	ukmArtist->loaded = true;
	ukmArtist->loaded_preview = true;
	ukmArtist->loaded_releases = false;

	for (MasterRelease_ptr mr : fakeArtist->master_releases) {

		bool bskip_preview_and_stats = false;

		if (va_done_cap.first >= k_limit_master_traffic) {

			--va_res_cap.first;
			++va_done_cap.first;

			bskip_preview_and_stats = true;
		}

		if (load_master_preview && !bskip_preview_and_stats) {

			try {

				--va_res_cap.first;

				//missing main_release_id

				pfc::string8 msg(PFC_string_formatter() << "loading master preview " << mr->id << "...");
				p_status.set_item(msg);

				//* * * * * * * *

#if defined(DISCOGGER_RUNNER_MEDIA_QA)
                discogger_live_search_qa::preview_attempts.fetch_add(1);
#endif
				mr->load_preview(p_status, p_abort, false);
#if defined(DISCOGGER_RUNNER_MEDIA_QA)
                discogger_live_search_qa::preview_successes.fetch_add(1);
#endif

				//* * * * * * * *

				++va_done_cap.first; // not counting E500, E404...

			}
			catch (foo_discogs_exception ex) {

				bool b500, b404;
				if (b500 = (pfc::string8(ex.what()).find_first("500") == ~0) && (b404 = pfc::string8(ex.what()).find_first("404") == ~0)) {
					throw ex;
				}
				else {
					log_msg(PFC_string_formatter() << "E500/E404 Master: " << mr->title);
				}
			}
		} 

		if (mr->artists.get_count()) {

			for (ReleaseArtist_ptr a : mr->artists) {

				bool b_add_new_artist = true;
				bool bexact_name = pfc::stringLite::g_equalsCaseInsensitive(a->name, query_artist.c_str());

				Artist_ptr tmpArtist;

				if (bexact_name) {
					if (pai.exact_matches.get_count()) {
						Artist_ptr last_exact = pai.exact_matches[pai.exact_matches.get_count() - 1];
						b_add_new_artist = atoi(last_exact->id) != atoi(a->id);
						if (!b_add_new_artist) {
							tmpArtist = last_exact;
						}
					}
				}
				else {
					if (pai.other_matches.get_count()) {
						Artist_ptr last_other = pai.other_matches[pai.other_matches.get_count() - 1];
						b_add_new_artist = atoi(last_other->id) != atoi(a->id);
						if (!b_add_new_artist) {
							tmpArtist = last_other;
						}
					}
				}

				if (b_add_new_artist) {

					auto find_art = pai.map_artists.find(a->id.c_str());
					b_add_new_artist = find_art == pai.map_artists.end();

					if (b_add_new_artist) {
						tmpArtist = std::make_shared<Artist>(a->id);
						pai.map_artists.emplace(a->id, tmpArtist);
					}
					else {
						tmpArtist = find_art->second;
					}
				}

				bool mr_found = false;

				for (MasterRelease_ptr wm : tmpArtist->master_releases) {
					if ((mr_found = wm->id.equals(mr->id))) {
						break;
					}
				}
				if (!mr_found) {
					tmpArtist->master_releases.add_item(mr);
					tmpArtist->search_order_master.add_item(true);
				}

				if (!tmpArtist->loaded) {

					try {

						//* * * * * * * *

						tmpArtist->load_mem_only(p_status, p_abort, false);

						//* * * * * * * *

					}
					catch (foo_discogs_exception ex) {
						pfc::string8 msg(ex.what());
						if (msg.find_first("404") != ~0 || msg.find_first("500") != ~0) {
							pai.catch_artists.insert(tmpArtist->id.c_str());
						}
						log_msg(msg);
						continue;
					}
				}

				a->full_artist = tmpArtist;

				size_t lkey = encode_mr(atoi(tmpArtist->id), atoi(mr->id));
				add_master_release_to_cache(lkey, mr);

				if (b_add_new_artist) {
					if (bexact_name) {
						pai.exact_matches.add_item(std::move(tmpArtist));
					}
					else {
						pai.other_matches.add_item(std::move(tmpArtist));
					}
				}
			} //walk master releases artists
		}
		else {

			Artist_ptr no_checkArtist;
			no_checkArtist = ukmArtist;
			bool b_add_new_artist = true;
			bool bexact_name = false;
			std::map<std::string, Artist_ptr>::iterator it;
			it = pai.map_artists.find(no_checkArtist->id.c_str());
			b_add_new_artist = it == pai.map_artists.end();

			if (b_add_new_artist) {
				pai.map_artists.emplace(no_checkArtist->id, no_checkArtist);
			}
			else {
				no_checkArtist = it->second;
			}
			no_checkArtist->master_releases.add_item(mr);
			no_checkArtist->search_order_master.add_item(true);

				size_t lkey = encode_mr(atoi(no_checkArtist->id), atoi(mr->id));
				add_master_release_to_cache(lkey, mr);
			if (b_add_new_artist) {
				if (bexact_name) {
					pai.exact_matches.add_item(std::move(no_checkArtist));
				}
				else {
					pai.other_matches.add_item(std::move(no_checkArtist));
				}
			}
		}
	}

	//non master releases

	ctraffic = 0;

	for (Release_ptr r : fakeArtist->releases) {

		bool bskip_preview_and_stats = false;

		if (atoi(r->master_id)) {

			--va_res_cap.second;
			continue;
		}

		if (va_done_cap.second >= k_limit_release_traffic) {

			// add it to unknown

			--va_res_cap.second;
			++va_done_cap.second;

			bskip_preview_and_stats = true;
		}


		if (load_master_preview && !bskip_preview_and_stats) {

		try {

			if (!r->artists.get_count()) {

				if (va_done_cap.second < k_limit_release_traffic) {

					pfc::string8 backtitle = r->title;
					r->title = "";


					--va_res_cap.second;

					pfc::string8 msg(PFC_string_formatter() << "loading non-master release " << r->id << "...");
					p_status.set_item(msg);

					//* * * * * * * *

#if defined(DISCOGGER_RUNNER_MEDIA_QA)
                    discogger_live_search_qa::preview_attempts.fetch_add(1);
#endif
					r->load_preview(p_status, p_abort, false);
#if defined(DISCOGGER_RUNNER_MEDIA_QA)
                    discogger_live_search_qa::preview_successes.fetch_add(1);
#endif

					//* * * * * * * *

					++va_done_cap.second;
				}
			}
			else {
			
				--va_res_cap.second;
				++va_done_cap.second;
			}

		}
		catch (foo_discogs_exception ex) {
			//titleformat in the tree won't be happy
			bool b500, b404;
			if (b500 = (pfc::string8(ex.what()).find_first("500") == ~0) && (b404 = pfc::string8(ex.what()).find_first("404") == ~0)) {
				throw ex;
			}
			else {
				log_msg(PFC_string_formatter() << "E500/E404 r: " << r->title);
				continue;
			}
		}
		} //end skip stats

		if (r->artists.get_count()) {

			//cached releases have artists...

			for (ReleaseArtist_ptr a : r->artists) {

				bool b_add_new_artist = true;
				bool bexact_name = pfc::stringLite::g_equalsCaseInsensitive(a->name, query_artist.c_str());

				Artist_ptr tmpArtist;

				std::map<std::string, Artist_ptr>::iterator it;
				it = pai.map_artists.find(a->id.get_ptr());

				b_add_new_artist = it == pai.map_artists.end();

				if (b_add_new_artist) {
					tmpArtist = std::make_shared<Artist>(a->id);
					pai.map_artists.emplace(a->id, tmpArtist);
				}
				else {
					tmpArtist = it->second;
				}

				bool r_found = false;
				for (Release_ptr wr: tmpArtist->releases) {
					if (r_found = wr->id.equals(r->id)) {
						break;
					}
				}
				if (!r_found) {
				
					tmpArtist->releases.add_item(r);
					tmpArtist->search_order_master.add_item(false);
				}

				// load artist

				if (pai.catch_artists.find(tmpArtist->id.c_str()) != pai.catch_artists.end()) {

					continue;

				}
				
				if (!tmpArtist->loaded) {

					try {

						//* * * * * * * *

                        tmpArtist->load_mem_only(p_status, p_abort, false);

						//* * * * * * * *

					}
					catch (foo_discogs_exception ex) {
						pfc::string8 msg;
						msg << "Artist Id:" << tmpArtist->id << ", Release Id:" << r->id;
						msg << " - " << ex.what();
						if (msg.find_first("404") != ~0 || msg.find_first("500") != ~0) {
							pai.catch_artists.insert(tmpArtist->id.c_str());
						}
						log_msg(msg);
						continue;
					}
				}

				a->full_artist = tmpArtist;

				// end load artist

				size_t lkey = encode_mr(atoi(tmpArtist->id), atoi(r->id));
				add_release_to_cache(lkey, r);

				if (b_add_new_artist) {
					if (bexact_name) {
						pai.exact_matches.add_item(std::move(tmpArtist));
					}
					else {
						pai.other_matches.add_item(std::move(tmpArtist));
					}
				}
			}
		}
		else {

			//this is not a cached release
			Artist_ptr no_checkArtist;

			no_checkArtist = ukArtist;
			bool b_add_new_artist = true;
			bool bexact_name = false;

			std::map<std::string, Artist_ptr>::iterator it;
			it = pai.map_artists.find(no_checkArtist->id.c_str());

			b_add_new_artist = it == pai.map_artists.end();

			if (b_add_new_artist) {
				pai.map_artists.emplace(no_checkArtist->id, no_checkArtist);
			}
			else {
				no_checkArtist = it->second;
			}

			no_checkArtist->releases.add_item(r);
			no_checkArtist->search_order_master.add_item(false);

			if (!bskip_preview_and_stats) {
				size_t lkey = encode_mr(atoi(no_checkArtist->id), atoi(r->id));
				add_release_to_cache(lkey, r);
			}

			if (b_add_new_artist) {
				if (bexact_name) {
					pai.exact_matches.add_item(std::move(no_checkArtist));
				}
				else {
					pai.other_matches.add_item(std::move(no_checkArtist));
				}
			}
		}
	}

	return;
}

void DiscogsInterface::parse_amt_page(Artist_ptr fakeArtist, JSONParser_ptr jp, parse_amt_info& pai,
	threaded_process_status& p_status, abort_callback& p_abort) {

	parseArtistReleases(jp->root, fakeArtist.get(), SearchMode::AT, pai.mva_masters, pai.qry_mrp_found);

	return;
}

rppair_t DiscogsInterface::search_amt_artist(const pfc::string8& query, const QueryDefMap qdm_search_query, pfc::array_t<Artist_ptr>& exact_matches, pfc::array_t<Artist_ptr>& other_matches, threaded_process_status& p_status, abort_callback& p_abort) {

	if (!search_query::IsMinimal(qdm_search_query)) {
		//..
		return rppair_t({ 0,0 }, { 0,0 });
		//..
	}

	std::pair<size_t, size_t> va_res_cap(0, 0);
	std::pair<size_t, size_t> va_done_cap(0, 0);

	pfc::string8 json;

	exact_matches.force_reset();
	other_matches.force_reset();

	pfc::string8 params;

	params = PFC_string_formatter() << query;

	size_t max_to_abort;
	size_t out_max_to_abort;
	max_to_abort = out_max_to_abort = HIWORD(CONF.query_max);

	pfc::string8 url;
	pfc::array_t<JSONParser_ptr> pages;
	url << "https://api.discogs.com/database/search";
	try {

		pages = discogs_interface->get_all_pages(url, params, out_max_to_abort, p_abort, "Fetching query results...", p_status);
		if (out_max_to_abort > max_to_abort) {
#if defined(DISCOGGER_RUNNER_MEDIA_QA)
            discogger_live_search_qa::query_limit.fetch_add(1);
#endif
			return rppair_t({ out_max_to_abort,out_max_to_abort }, { 0,0 });
		}

	}
    catch (const exception_aborted&) {
        // User cancellation is not a search failure. Preserve the SDK's
        // cancellation signal so no fallback, cache update or UI error is
        // triggered for a deliberately stopped network request.
        throw;
    }
	catch (foo_discogs_exception& e) {

		pfc::string8 error("Error loading search releases. ");
		error << e.what();
		throw foo_discogs_exception(error);
	}
	catch (...) {
		foo_discogs_exception ex;
		ex << "Unknown error loading releases.";
		throw ex;
	}

	const size_t cpages = pages.get_count();
#if defined(DISCOGGER_RUNNER_MEDIA_QA)
    discogger_live_search_qa::response_pages.fetch_add(static_cast<unsigned>(cpages));
    for (size_t p = 0; p < cpages; ++p) {
        const json_t* results = json_object_get(pages[p]->root, "results");
        if (json_is_array(results))
            discogger_live_search_qa::response_items.fetch_add(
                static_cast<unsigned>(json_array_size(results)));
    }
#endif

#if defined(DISCOGGER_RUNNER_MEDIA_QA)
    discogger_live_search_qa::worker_phase.store(1);
#endif
	bool bCacheSaved = true;
	bool bmark_pending = false;

	Artist_ptr fake_artist;
	fake_artist = std::make_shared<Artist>("0"); //VA
	fake_artist->name = "Search MT";
	fake_artist->loaded = true;
	fake_artist->loaded_preview = true;
	fake_artist->loaded_releases = true;

	std::pair<size_t, size_t> qry_m_r_found = { 0,0 };

	std::map<std::string, Artist_ptr> map_found_artists;
	std::set<std::string> set_bad_artists;
	std::map<std::string, MasterRelease_ptr> mva_masters;

	parse_amt_info pai = {
		qdm_search_query,
		map_found_artists,
		set_bad_artists,
		mva_masters,
		exact_matches,
		other_matches,
		qry_m_r_found
	};

	for (size_t i = 0; i < cpages; i++) {
		//need preview working to avoid unloaded duplicated masters erasing valid data 
		parse_amt_page(fake_artist, pages[i], pai, p_status, p_abort);
	}
#if defined(DISCOGGER_RUNNER_MEDIA_QA)
    discogger_live_search_qa::worker_phase.store(2);
#endif

    // A nonempty structured response is not necessarily a usable release:
    // e.g. a broad artist query can return only unrelated release titles.
    // Preserve plausible structured matches; otherwise offer at most three
    // bounded, review-only discovery queries. Never auto-approve a fallback.
    const auto& original_artist = qdm_search_query.at("artist=").first;
    const auto& release_title = qdm_search_query.at("title=").first;
    const auto& original_track = qdm_search_query.at("track=").first;
    const bool original_has_candidates =
        fake_artist->master_releases.get_count() || fake_artist->releases.get_count();
    bool original_title_plausible = false;
    if (original_has_candidates) {
        for (const auto& master : fake_artist->master_releases) {
            if (master && discogger_search::plausible_release_group_title(
                    release_title, master->title.c_str(), master->sub_releases,
                    [](const Release_ptr& child) -> std::string_view {
                        return child ? std::string_view(child->title.c_str())
                                     : std::string_view{};
                    })) {
                original_title_plausible = true;
                break;
            }
        }
        if (!original_title_plausible) {
            for (const auto& release : fake_artist->releases) {
                if (release && discogger_search::plausible_release_title(
                        release_title, release->title.c_str())) {
                    original_title_plausible = true;
                    break;
                }
            }
        }
    }
    bool used_broad_fallback = false;
    if (!original_has_candidates || !original_title_plausible) {
        // Keep the exact original query untouched; the plan only suggests
        // additional Discogs candidates and never writes tags.
        const auto plans = discogger_search::build_fallback_plan(
            original_artist, release_title, original_track);
        discogger_search::fallback_query_budget budget(plans.size());
        for (const auto& candidate : plans) {
            p_abort.check();
            // Do not round-trip untrusted release titles/artist names through
            // TextToMap: e.g. "Love & title=Mix" contains a literal field
            // delimiter, not another user-selected Discogs filter.
            const std::string fallback_query =
                discogger_search::encode_fallback_query(candidate);
            if (fallback_query.empty()) continue;
            if (!budget.begin()) break;

            pfc::string8 status(original_has_candidates
                ? "No plausible release-title match: " : "No exact result: ");
            status << candidate.label.c_str();
            p_status.set_item(status);
            pfc::string8 fallback_url("https://api.discogs.com/database/search");
            pfc::string8 fallback_params(fallback_query.c_str());
            size_t fallback_max = HIWORD(CONF.query_max);
#if defined(DISCOGGER_RUNNER_MEDIA_QA)
            discogger_live_search_qa::fallback_attempts.fetch_add(1);
            discogger_live_search_qa::worker_phase.store(3);
#endif
            pfc::array_t<JSONParser_ptr> fallback_pages =
                get_all_pages(fallback_url, fallback_params, fallback_max, p_abort,
                    "Fetching bounded Discogs fallback candidates...", p_status);
            if (fallback_max > max_to_abort) {
#if defined(DISCOGGER_RUNNER_MEDIA_QA)
                discogger_live_search_qa::query_limit.fetch_add(1);
#endif
                p_status.set_item("Fallback result limit exceeded; refine the search.");
                budget.stop();
                break;
            }
#if defined(DISCOGGER_RUNNER_MEDIA_QA)
            discogger_live_search_qa::response_pages.fetch_add(
                static_cast<unsigned>(fallback_pages.get_count()));
            for (size_t p = 0; p < fallback_pages.get_count(); ++p) {
                const json_t* results = json_object_get(fallback_pages[p]->root, "results");
                if (json_is_array(results))
                    discogger_live_search_qa::response_items.fetch_add(
                        static_cast<unsigned>(json_array_size(results)));
            }
#endif
            const size_t prior_masters = fake_artist->master_releases.get_count();
            const size_t prior_releases = fake_artist->releases.get_count();
            for (size_t i = 0; i < fallback_pages.get_count(); ++i) {
                p_abort.check();
                parse_amt_page(fake_artist, fallback_pages[i], pai, p_status, p_abort);
            }
#if defined(DISCOGGER_RUNNER_MEDIA_QA)
            discogger_live_search_qa::worker_phase.store(4);
#endif
            // A duplicate ID from fallback must not demote an otherwise
            // unchanged original candidate into the review-only bucket.
            const bool have_new_candidates =
                fake_artist->master_releases.get_count() > prior_masters ||
                fake_artist->releases.get_count() > prior_releases;
            budget.observe(have_new_candidates);
            if (have_new_candidates) {
                used_broad_fallback = true;
                p_status.set_item("Broader Discogs results: review release/version before tagging.");
                // Never stop merely because a fallback returned something:
                // guest/title may return an unrelated release while the next
                // bounded phase contains the requested ID. The parser merges
                // distinct IDs; presentation stays review-only.
            }
        }
    }

	va_res_cap.first = fake_artist->master_releases.get_count();
	va_res_cap.second = fake_artist->releases.get_count();
#if defined(DISCOGGER_RUNNER_MEDIA_QA)
    // These are parsed preview candidates, never independently fetched probes.
    discogger_live_search_qa::parsed_masters.store(static_cast<unsigned>(va_res_cap.first));
    discogger_live_search_qa::parsed_releases.store(static_cast<unsigned>(va_res_cap.second));
#endif

	//to load while parsing
	for (auto mr : pai.mva_masters) {
		mr.second->loaded_preview = false;
	}

	// Low-confidence fallback suggestions stay in the 'other' section:
    // the UI cannot mistake them for exact original-artist hits.
    QueryDefMap presentation_query = qdm_search_query;
    if (used_broad_fallback)
        presentation_query["artist="].first.clear();
    parse_amt_info presentation_pai = {
        presentation_query, map_found_artists, set_bad_artists, mva_masters,
        exact_matches, other_matches, qry_m_r_found
    };
#if defined(DISCOGGER_RUNNER_MEDIA_QA)
    discogger_live_search_qa::worker_phase.store(5);
#endif
    process_amt_parsed_fake_artist(fake_artist, presentation_pai, true, true,
        va_res_cap, va_done_cap, p_status, p_abort);
#if defined(DISCOGGER_RUNNER_MEDIA_QA)
    discogger_live_search_qa::worker_phase.store(6);
#endif

	size_t cmatches = pai.other_matches.get_count();

	pfc::array_t<t_size> order; order.set_size(cmatches);
	for (size_t i = 0; i < cmatches; i++) { order[i] = i; }
	pfc::bit_array_bittable select; select.resize(cmatches);

	auto& om = pai.other_matches;
	pfc::string8 undef_pref = search_query::k_ukm_artist_name.subString(0, search_query::k_ukm_artist_name.find_first(' ')+1);
	for (size_t i = 0; i < cmatches; i++) {
		if (om[i]->name.has_prefix(undef_pref)) {
			select.set(i, true);
		}
	}
	pfc::create_drop_permutation(order.get_ptr(), cmatches, select, cmatches);

	pfc::list_t<Artist_ptr> lp;
	lp.add_items_fromptr(om.get_ptr(), cmatches);
	lp.reorder(order.get_ptr());

	pfc::list_to_array(om, lp);

#if defined(DISCOGGER_RUNNER_MEDIA_QA)
    discogger_live_search_qa::worker_phase.store(7);
#endif
	return rppair_t({ va_res_cap }, { va_done_cap });

}

pfc::array_t<JSONParser_ptr> DiscogsInterface::get_all_pages(pfc::string8 &url, pfc::string8 params, abort_callback &p_abort) {
    pfc::array_t<JSONParser_ptr> results;
    if (params.get_length()) params << "&";
    params << "per_page=100";
    size_t page = 1, last = 0, first_items = 0, first_per_page = 0;

    for (;;) {
        p_abort.check();
        pfc::string8 page_params;
        page_params << params << "&page=" << page;
        pfc::string8 json;
        fetcher->fetch_html(url, page_params, json, p_abort);
        JSONParser_ptr jp = pfc::rcnew_t<JSONParser>(json);

        // Inspect the response BEFORE moving jp into the result array.
        const size_t returned_page = jp->get_object_int("pagination", "page");
        const size_t returned_pages = jp->get_object_int("pagination", "pages");
        const size_t per_page = jp->get_object_int("pagination", "per_page");
        const size_t items = jp->get_object_int("pagination", "items");
        if (!discogger_cache::valid_online_page(page, returned_page, returned_pages,
                per_page, items, last, first_per_page, first_items))
            throw foo_discogs_exception("Discogs returned invalid or changing pagination; refresh and retry.");
        // Search, artist releases, collection and master-version endpoints
        // return one of these known paginated arrays. A truncated/missing
        // array with a plausible count must never be shown as a complete
        // search result or overwrite the last good cache.
        const json_t* page_entries = json_object_get(jp->root, "results");
        if (!page_entries) page_entries = json_object_get(jp->root, "releases");
        if (!page_entries) page_entries = json_object_get(jp->root, "versions");
        if (!json_is_array(page_entries) ||
            !discogger_cache::valid_online_payload_count(
                page, returned_pages, per_page, items, json_array_size(page_entries)))
            throw foo_discogs_exception(
                "Discogs returned a missing or incomplete result page; refresh and retry.");

        if (page == 1) {
            last = returned_pages;
            first_items = items;
            first_per_page = per_page;
        }
        p_abort.check();
        results.append_single(std::move(jp));
        if (page >= last) break;
        ++page;
    }
    p_abort.check();
    return results;
}

pfc::array_t<JSONParser_ptr> DiscogsInterface::get_all_pages(pfc::string8& url, pfc::string8 params, abort_callback& p_abort, const char* msg, threaded_process_status& p_status) {
	size_t max_to_abort = SIZE_MAX;
	return get_all_pages(url, params, max_to_abort, p_abort, msg, p_status);
}

pfc::array_t<JSONParser_ptr> DiscogsInterface::get_all_pages(pfc::string8 &url, pfc::string8 params, size_t& max_to_abort, abort_callback &p_abort, const char *msg, threaded_process_status &p_status) {
    pfc::array_t<JSONParser_ptr> results;
    if (params.get_length()) params << "&";
    params << "per_page=100";
    size_t page = 1, last = 0, first_items = 0, first_per_page = 0;

    for (;;) {
        p_abort.check();
        pfc::string8 status(msg);
        if (page > 1) status << " page " << page << "/" << last << " (100 x page)";
        p_status.set_item(status);
        pfc::string8 page_params;
        page_params << params << "&page=" << page;
        pfc::string8 json;
        fetcher->fetch_html(url, page_params, json, p_abort);
        JSONParser_ptr jp = pfc::rcnew_t<JSONParser>(json);

        // Validate first: std::move(jp) transfers ownership of the parser.
        const size_t returned_page = jp->get_object_int("pagination", "page");
        const size_t returned_pages = jp->get_object_int("pagination", "pages");
        const size_t per_page = jp->get_object_int("pagination", "per_page");
        const size_t items = jp->get_object_int("pagination", "items");
        if (!discogger_cache::valid_online_page(page, returned_page, returned_pages,
                per_page, items, last, first_per_page, first_items))
            throw foo_discogs_exception("Discogs returned invalid or changing pagination; refresh and retry.");
        // Search, artist releases, collection and master-version endpoints
        // return one of these known paginated arrays. A truncated/missing
        // array with a plausible count must never be shown as a complete
        // search result or overwrite the last good cache.
        const json_t* page_entries = json_object_get(jp->root, "results");
        if (!page_entries) page_entries = json_object_get(jp->root, "releases");
        if (!page_entries) page_entries = json_object_get(jp->root, "versions");
        if (!json_is_array(page_entries) ||
            !discogger_cache::valid_online_payload_count(
                page, returned_pages, per_page, items, json_array_size(page_entries)))
            throw foo_discogs_exception(
                "Discogs returned a missing or incomplete result page; refresh and retry.");


        if (page == 1) {
            // Preserve the existing explicit query-volume refusal, with no
            // partially populated result and no additional network requests.
            if (items > max_to_abort) {
                max_to_abort = items;
                return results;
            }
            last = returned_pages;
            first_items = items;
            first_per_page = per_page;
        }
        p_abort.check();
        results.append_single(std::move(jp));
        if (page >= last) break;
        ++page;
    }
    p_abort.check();
    return results;
}

namespace fs =std::filesystem;

pfc::array_t<JSONParser_ptr> DiscogsInterface::get_all_pages_offline_cache(ol::GetFrom getFrom, pfc::string8& id, pfc::string8 &secid, pfc::string8 params, abort_callback& p_abort, const char* msg, threaded_process_status& p_status) {
    (void)params; // The caller owns the correct API URL for an online retry.
    p_abort.check();
    if (getFrom != ol::GetFrom::ArtistReleases && getFrom != ol::GetFrom::Versions)
        throw foo_discogs_exception("Unsupported offline cache page type.");

    const pfc::string8 path_container = get_offline_path(id, getFrom, secid, true);
    const fs::path directory = fs::u8path(path_container.c_str());
    std::map<std::size_t, fs::path> ordered_pages;
    pfc::array_t<JSONParser_ptr> staged;

    try {
        if (!fs::is_directory(directory))
            throw foo_discogs_exception("Offline cache page directory is missing.");

        for (const auto& entry : fs::directory_iterator(directory)) {
            p_abort.check();
            if (entry.is_symlink())
                throw foo_discogs_exception("Offline cache contains a symlink.");
            if (!entry.is_directory()) continue; // Loading/task markers are files.
            const auto utf8 = entry.path().filename().u8string();
            const std::string name(utf8.begin(), utf8.end());
            std::size_t index = 0;
            if (!discogger_cache::parse_cache_page_folder(name, index) ||
                !ordered_pages.emplace(index, entry.path()).second)
                throw foo_discogs_exception("Offline cache page index is invalid or duplicated.");
        }

        std::vector<std::size_t> indexes;
        for (const auto& page : ordered_pages) indexes.push_back(page.first);
        if (!discogger_cache::complete_cache_page_sequence(indexes))
            throw foo_discogs_exception("Offline cache has missing or duplicate pages.");

        std::size_t expected_items = 0, expected_per_page = 0;
        for (const auto& page : ordered_pages) {
            p_abort.check();
            pfc::string8 status(msg);
            status << " page " << (page.first + 1) << "/" << ordered_pages.size();
            p_status.set_item(status);

            const fs::path file = page.second / "root.json";
            if (fs::is_symlink(file) || !fs::is_regular_file(file) || fs::file_size(file) == 0)
                throw foo_discogs_exception("Offline cache page JSON is missing or empty.");
            const auto utf8 = file.u8string();
            const std::string file_name(utf8.begin(), utf8.end());

            const auto cleanup = [](json_t* ptr) { if (ptr) json_decref(ptr); };
            std::unique_ptr<json_t, decltype(cleanup)> raw(
                offline_cache.Read_JSON(file_name.c_str()), cleanup);
            if (!json_is_object(raw.get()))
                throw foo_discogs_exception("Offline cache page JSON root is not an object.");

            const json_t* pagination = json_object_get(raw.get(), "pagination");
            auto read_count = [pagination](const char* key, std::size_t& output) {
                const json_t* value = json_object_get(pagination, key);
                if (!json_is_integer(value) || json_integer_value(value) < 0) return false;
                output = static_cast<std::size_t>(json_integer_value(value));
                return true;
            };
            std::size_t api_page = 0, api_pages = 0, api_per_page = 0, api_items = 0;
            const bool metadata_valid = json_is_object(pagination) &&
                read_count("page", api_page) && read_count("pages", api_pages) &&
                read_count("per_page", api_per_page) && read_count("items", api_items) &&
                discogger_cache::valid_cache_pagination(page.first, ordered_pages.size(),
                    api_page, api_pages, api_per_page, api_items);
            const char* payload_name = getFrom == ol::GetFrom::ArtistReleases ? "releases" : "versions";
            const json_t* payload = json_object_get(raw.get(), payload_name);
            const bool payload_valid = metadata_valid && json_is_array(payload) &&
                discogger_cache::valid_cache_payload_count(page.first, ordered_pages.size(),
                    api_per_page, api_items, json_array_size(payload));
            const bool consistent = page.first == 0 ||
                (expected_items == api_items && expected_per_page == api_per_page);
            if (!payload_valid || !consistent)
                throw foo_discogs_exception("Offline cache pagination or page payload is inconsistent.");

            if (page.first == 0) {
                expected_items = api_items;
                expected_per_page = api_per_page;
            }
            // JSONParser retains its own Jansson reference; raw is always released.
            JSONParser_ptr parsed = pfc::rcnew_t<JSONParser>(raw.get());
            staged.append_single(std::move(parsed));
        }
    }
    catch (const exception_aborted&) { throw; }
    catch (const fs::filesystem_error&) {
        throw foo_discogs_exception("Offline cache filesystem access failed.");
    }
    return staged; // No partial page array escapes on failure.
}

void DiscogsInterface::get_entity_offline_cache(ol::GetFrom getfrom, pfc::string8& artist_id, pfc::string8& release_id, pfc::string8& html, abort_callback& p_abort, const char* msg, threaded_process_status& p_status) {

	PFC_ASSERT(getfrom == ol::GetFrom::Artist || getfrom == ol::GetFrom::Release);
	PFC_ASSERT(!(getfrom == ol::GetFrom::Artist && release_id.get_length()));
	PFC_ASSERT(!(getfrom == ol::GetFrom::Release && !release_id.get_length()));


	pfc::string8 json_path;

	if (getfrom == ol::GetFrom::Artist) {
		json_path = ol::get_offline_path(artist_id, ol::GetFrom::Artist, "", true);
	}
	else {
		json_path = ol::get_offline_path(artist_id, ol::GetFrom::Release, release_id, true);
	}

	json_path << "\\root.json";

    // Fail closed on missing, invalid or empty offline data: never leave the
    // caller's previous release JSON in the output argument.
    html = "";
    const auto json_cleanup = [](json_t* ptr) { if (ptr) json_decref(ptr); };
    std::unique_ptr<json_t, decltype(json_cleanup)> js_obj(
        offline_cache.Read_JSON(json_path.get_ptr()), json_cleanup);
    if (!json_is_object(js_obj.get()) || json_object_size(js_obj.get()) == 0)
        return;

    const auto text_cleanup = [](char* ptr) { if (ptr) std::free(ptr); };
    std::unique_ptr<char, decltype(text_cleanup)> serialized(
        json_dumps(js_obj.get(), 0), text_cleanup);
    if (serialized) html = serialized.get();
}

pfc::string8 DiscogsInterface::get_username(threaded_process_status &p_status, abort_callback &p_abort) {
	if (!username.get_length()) {
		username = load_username(p_status, p_abort);
	}
	return username;
}

pfc::string8 DiscogsInterface::load_username(threaded_process_status &p_status, abort_callback &p_abort) {
	try {
		pfc::string8 status("loading identity...");
		p_status.set_item(status);

		pfc::string8 json;
		pfc::string8 url;
		url << "https://api.discogs.com/oauth/identity";

		fetcher->fetch_html(url, "", json, p_abort);
		JSONParser jp(json);

		Identity i;
		parseIdentity(jp.root, &i);
		return i.username;
	}
	catch (network_exception &) {
		throw;
	}
}

pfc::array_t<pfc::string8> DiscogsInterface::get_collection(threaded_process_status &p_status, abort_callback &p_abort) {

	if (collection.get_count()) {
		return collection;
	}

	pfc::string8 username = get_username(p_status, p_abort);

	try {
		pfc::string8 json;
		pfc::string8 url;
		url << "https://api.discogs.com/users/" << username << "/collection/folders";

		fetcher->fetch_html(url, "per_page=100", json, p_abort);
		JSONParser jp(json);

		pfc::array_t<pfc::string8> urls = jp.get_object_array_string("folders", "resource_url");

		url = urls[0];
		url << "/releases";
		pfc::array_t<JSONParser_ptr> pages = get_all_pages(url, "", p_abort, "Loading collection...", p_status);

		for (size_t i = 0; i < pages.get_count(); i++) {

			parseCollection(pages[i]->root, collection);
		}
	}
	catch (network_exception &) {
		throw;
	}
	return collection;
}

bool DiscogsInterface::get_thumbnail_from_cache(Release_ptr release, bool isArtist, size_t img_ndx, MemoryBlock& small_art,
	threaded_process_status& p_status, abort_callback& p_abort) {

	// Never expose a previous preview after a failed cache read.
	small_art.set_size(0);
	if (!release) return false;

	pfc::string8 id;
	size_t local_ndx = img_ndx;
	if (!isArtist) {
		if (img_ndx >= release->images.get_count() ||
			!release->images[img_ndx] ||
			!release->images[img_ndx]->url150.get_length()) {
			log_msg(PFC_string_formatter() << "Unavailable album thumbnail, index: " << img_ndx);
			return false;
		}
		id = release->id;
	} else {
		Artist_ptr artist;
		if (!img_artists_ndx_to_artist(release, img_ndx, artist, local_ndx) ||
			!artist || local_ndx >= artist->images.get_count() ||
			!artist->images[local_ndx] ||
			!artist->images[local_ndx]->url150.get_length()) {
			log_msg(PFC_string_formatter() << "Unavailable artist thumbnail, index: " << img_ndx);
			return false;
		}
		id = artist->id;
	}

	const auto cache_paths = ol::get_thumbnail_cache_path_filenames(
		id, isArtist ? art_src::art : art_src::alb, LVSIL_NORMAL, true, local_ndx);
	if (!cache_paths.get_count()) return false;

	p_abort.check();
	p_status.set_item("Reading artwork thumbnail from cache...");
	const pfc::string8& cache_path = cache_paths[0];
	try {
		const auto file_path = std::filesystem::u8path(cache_path.get_ptr());
		if (!std::filesystem::is_regular_file(file_path)) return false;
		const auto bytes = std::filesystem::file_size(file_path);
		if (!discogger_thumbnail::valid_cache_size(bytes)) {
			log_msg("Ignoring empty or oversized cached artwork thumbnail.");
			return false;
		}
		std::ifstream input(file_path, std::ios::binary);
		if (!input) return false;
		const auto size = static_cast<size_t>(bytes);
		small_art.set_size(size);
		input.read(reinterpret_cast<char*>(small_art.get_ptr()),
			static_cast<std::streamsize>(size));
		const bool complete = discogger_thumbnail::complete_read(input.gcount(), size);
		if (!complete || input.peek() != std::char_traits<char>::eof()) {
			small_art.set_size(0);
			log_msg("Ignoring incomplete or changed cached artwork thumbnail.");
			return false;
		}
		return true;
	} catch (const std::filesystem::filesystem_error&) {
		small_art.set_size(0);
		return false;
	}
}

bool DiscogsInterface::delete_artist_cache(const pfc::string8& artist_id, const pfc::string8& release_id) {

	bool bonlyRelease = release_id.get_length();
	bool bmissingArtist = !artist_id.get_length();
	bool bmissingRelease = !release_id.get_length();

	Artist_ptr artist = get_artist_from_cache(artist_id);

	bool delres = true;

	//cache memory

	if (!bmissingArtist && artist.get()) {

		size_t lkey = encode_mr(0, release_id);
		delres = cache_releases->remove(lkey);
		delres = cache_artists->remove(artist_id);

		if (bonlyRelease) {
			size_t lkey = encode_mr(artist->search_role_list_pos, release_id);
			delres &= cache_releases->remove(lkey);
		}
		else {
			for (size_t walk = 0; walk < artist->master_releases.get_count(); walk++) {
				unsigned long lkey = encode_mr(atoi(artist_id), atoi(artist->master_releases[walk]->id));

				delres &= cache_master_releases->remove(lkey);

				//todo: clean-up encode_mr with 0, artist_id and search_artist_rol pos
				lkey = encode_mr(0, atoi(artist->master_releases[walk]->id));
				delres &= cache_master_releases->remove(lkey);
				lkey = encode_mr(artist->search_role_list_pos, atoi(artist->master_releases[walk]->id));
				delres &= cache_master_releases->remove(lkey);

				//remove main releases
				lkey = encode_mr(artist->search_role_list_pos, artist->master_releases[walk]->main_release->id);
				delres &= cache_releases->remove(lkey);

				//todo: clean-up encode_mr with 0, artist_id and search_artist_rol pos
				lkey = encode_mr(0, atoi(artist->master_releases[walk]->main_release->id));
				delres &= cache_releases->remove(lkey);
				lkey = encode_mr(atoi(artist->id), atoi(artist->master_releases[walk]->main_release->id));
				delres &= cache_releases->remove(lkey);


			}
			for (size_t walk = 0; walk < artist->releases.get_count(); walk++) {
				size_t lkey = encode_mr(artist->search_role_list_pos, artist->releases[walk]->id);
				delres &= cache_releases->remove(lkey);

				//todo: clean-up encode_mr with 0, artist_id and search_artist_rol pos
				lkey = encode_mr(0, atoi(artist->releases[walk]->id));
				delres &= cache_releases->remove(lkey);
				lkey = encode_mr(atoi(artist->id), atoi(artist->releases[walk]->id));
				delres &= cache_releases->remove(lkey);
			}
		}
	}
	else {
		if (!bmissingRelease) {
			size_t lkey = encode_mr(0, release_id);
			delres = cache_releases->remove(lkey);
			//todo: clean-up encode_mr with 0, artist_id and search_artist_rol pos
			if (artist) {
				lkey = encode_mr(artist->search_role_list_pos, release_id);
				delres &= cache_releases->remove(lkey);
				lkey = encode_mr(atoi(artist->id), release_id);
				delres &= cache_releases->remove(lkey);
			}
			// exit

			return delres;
		}
	}

	if (artist.get()) {

		// disk cache

		pfc::string8 parent_path = ol::get_offline_path(artist_id, ol::GetFrom::Artist, "", true);
		std::filesystem::path os_path = std::filesystem::u8path(parent_path.c_str());

		os_path = os_path.parent_path();

		if (fs::exists(os_path)) {

			if (bonlyRelease) {

				//del json
				os_path.append("release");
				os_path.append(release_id.c_str());
				if (fs::exists(os_path)) {
					std::error_code ec;
					std::filesystem::remove_all(os_path, ec);
					delres &= !(!!ec.value());
				}
				//del thumbs
				parent_path = ol::get_offline_path("", ol::GetFrom::Thumbs, release_id, true);
				os_path = std::filesystem::u8path(parent_path.c_str());
				if (!fs::exists(os_path)) {
					return false;
				}
				std::error_code ec;
				std::filesystem::remove_all(os_path, ec);
				return !(!!ec.value());
			}

			std::error_code ec;
			std::filesystem::remove_all(os_path, ec);
			delres &= !(!!ec.value());

			//remove artist thumbs

			parent_path = ol::get_offline_path(artist_id, ol::GetFrom::Thumbs, "", true);
			os_path = std::filesystem::u8path(parent_path.c_str());

			std::filesystem::remove_all(os_path, ec);
			delres &= !(!!ec.value());

			//remove all artwork from its releases
			for (size_t walk = 0; walk < artist->releases.get_count(); walk++) {

				parent_path = ol::get_offline_path("", ol::GetFrom::Thumbs, artist->releases[walk]->id, true);
				os_path = std::filesystem::u8path(parent_path.c_str());
				if (!fs::exists(os_path)) {
					//..
				}
				std::filesystem::remove_all(os_path, ec);
				delres &= !(!!ec.value());
			}
			return !ec.value();
		}
	}
	if (!bonlyRelease && artist.get()) {
		artist->loaded_releases_offline = true;
	}
	return false;
}
