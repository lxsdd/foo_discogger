#include "stdafx.h"
#include "discogger_search_fallback.h"

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

				mr->load_preview(p_status, p_abort, false);

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
					tmpArtist->master_releases.add_item(std::move(mr));
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
			no_checkArtist->master_releases.add_item(std::move(mr));
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

					r->load_preview(p_status, p_abort, false);

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
				
					tmpArtist->releases.add_item(std::move(r));
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

			no_checkArtist->releases.add_item(std::move(r));
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
			return rppair_t({ out_max_to_abort,out_max_to_abort }, { 0,0 });
		}

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

	// A collaboration can be indexed under a participating artist rather than
	// the combined name. Retry once with the trailing collaborator and album
	// title only when the original Discogs query returned no releases.
	// Keep the original query unmodified for the user and limit the fallback
	// to a single extra API search (never retry it recursively).
	if (!fake_artist->master_releases.get_count() && !fake_artist->releases.get_count()) {
		const auto& original_artist = qdm_search_query.at("artist=").first;
		const auto& release_title = qdm_search_query.at("title=").first;
		const std::string collaborator = discogger_search::trailing_collaborator(original_artist);
		if (!collaborator.empty() && !release_title.empty()) {
			pfc::string8 fallback_expression;
			fallback_expression << "q=" << collaborator.c_str() << " & title=" << release_title.c_str();

			QueryDefMap fallback_map;
			std::pair<std::string, std::string> fallback_query;
			if (search_query::TextToMap(fallback_expression, fallback_map) &&
				search_query::MapToText(fallback_map, fallback_query, false) &&
				!fallback_query.second.empty()) {
				p_abort.check();
				p_status.set_item("No exact release found; searching collaboration partner...");
				pfc::string8 fallback_url("https://api.discogs.com/database/search");
				pfc::string8 fallback_params(fallback_query.second.c_str());
				size_t fallback_max = HIWORD(CONF.query_max);
				pfc::array_t<JSONParser_ptr> fallback_pages =
					get_all_pages(fallback_url, fallback_params, fallback_max, p_abort,
						"Fetching collaboration fallback results...", p_status);
				for (size_t i = 0; i < fallback_pages.get_count(); ++i) {
					parse_amt_page(fake_artist, fallback_pages[i], pai, p_status, p_abort);
				}
			}
		}
	}

	va_res_cap.first = fake_artist->master_releases.get_count();
	va_res_cap.second = fake_artist->releases.get_count();

	//to load while parsing
	for (auto mr : pai.mva_masters) {
		mr.second->loaded_preview = false;
	}

	process_amt_parsed_fake_artist(fake_artist, pai, true, true, va_res_cap, va_done_cap, p_status, p_abort);

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

	return rppair_t({ va_res_cap }, { va_done_cap });

}

pfc::array_t<JSONParser_ptr> DiscogsInterface::get_all_pages(pfc::string8 &url, pfc::string8 params, abort_callback &p_abort) {

	pfc::array_t<JSONParser_ptr> results;
	if (params.get_length()) {
		params << "&";
	}
	params << "per_page=100";
	size_t page = 1;
	size_t last = 0;

	do {
		pfc::string8 page_params;
		page_params << params;
		page_params << "&page=" << page;

		pfc::string8 json;
		fetcher->fetch_html(url, page_params, json, p_abort);
		JSONParser_ptr jp = pfc::rcnew_t<JSONParser>(json);
		results.append_single(std::move(jp));
		if (page == 1) {
			last = jp->get_object_int("pagination", "pages");
		}
		page++;

	} while (page <= last && !p_abort.is_aborting());

	return results;
}

pfc::array_t<JSONParser_ptr> DiscogsInterface::get_all_pages(pfc::string8& url, pfc::string8 params, abort_callback& p_abort, const char* msg, threaded_process_status& p_status) {
	size_t max_to_abort = SIZE_MAX;
	return get_all_pages(url, params, max_to_abort, p_abort, msg, p_status);
}

pfc::array_t<JSONParser_ptr> DiscogsInterface::get_all_pages(pfc::string8 &url, pfc::string8 params, size_t& max_to_abort, abort_callback &p_abort, const char *msg, threaded_process_status &p_status) {

	pfc::array_t<JSONParser_ptr> results;
	if (params.get_length()) {
		params << "&";
	}
	params << "per_page=100";
	size_t page = 1;
	size_t last = 0;

	do {
		pfc::string8 status(msg);
		if (page > 1) {
			status << " page " << page << "/" << last << " (100 x page)";
		}
		p_status.set_item(status);

		pfc::string8 page_params;
		page_params << params;
		page_params << "&page=" << page;

		pfc::string8 json;

		fetcher->fetch_html(url, page_params, json, p_abort);
		JSONParser_ptr jp = pfc::rcnew_t<JSONParser>(json);

		results.append_single(std::move(jp));

		if (page == 1) {
			size_t items = jp->get_object_int("pagination", "items");
			if (items > max_to_abort) {
				results.force_reset();
				max_to_abort = items;
				return results;
			}
			last = jp->get_object_int("pagination", "pages");
		}
		page++;

	} while (page <= last && !p_abort.is_aborting());

	return results;
}

namespace fs =std::filesystem;

pfc::array_t<JSONParser_ptr> DiscogsInterface::get_all_pages_offline_cache(ol::GetFrom getFrom, pfc::string8& id, pfc::string8 &secid, pfc::string8 params, abort_callback& p_abort, const char* msg, threaded_process_status& p_status) {

	pfc::string8 path_container = get_offline_path(id, getFrom, secid, true);
	std::filesystem::path os_path_container = std::filesystem::u8path(path_container.c_str());

	pfc::array_t<JSONParser_ptr> jparsers;

	pfc::string8 status_msg(msg);

	try
	{
		if (fs::exists(os_path_container) && fs::is_directory(os_path_container))
		{
			//alt search recursive and detect root.json files
			std::filesystem::directory_iterator dirpos{ os_path_container };
			size_t msg_pos = 0;
			for (std::filesystem::directory_entry walk_dir : dirpos) {
				if (walk_dir.is_directory()) {

					if (p_abort.is_aborting()) break;

					p_status.set_item(status_msg);
					fs::path os_root = walk_dir;
					os_root += "\\root.json";
					const auto os_root_u8 = os_root.u8string();
				const std::string os_root_utf8(os_root_u8.begin(), os_root_u8.end());
				json_t* json_obj = offline_cache.Read_JSON(os_root_utf8.c_str());

					if (json_obj == nullptr) {
						foo_discogs_exception ex(PFC_string_formatter() << "Invalid cache file:" << os_root.c_str());
						throw ex;
					}

					pfc::string8 json(json_dumps(json_obj, 0));

					JSONParser_ptr jp = pfc::rcnew_t<JSONParser>(json);
					jparsers.append_single(std::move(jp));

					status_msg = msg;
					status_msg << PFC_string_formatter() << " page " << std::to_string(++msg_pos).c_str() << " (100 x page)";
				}
			}
		}
		else {

			//try transient
			return get_all_pages(id, params, p_abort, msg, p_status);
		}
	}
	catch (const fs::filesystem_error& err)
	{
		log_msg(PFC_string_formatter() << "Can not access file: " << path_container);
	}
	catch (const std::exception& ex)
	{
		log_msg(PFC_string_formatter() << "Error in path: " << path_container);
	}

	return jparsers;
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

	json_t* js_obj = offline_cache.Read_JSON(json_path.get_ptr());

	// checking js_obj prevents crash on invalid offline files

	if (js_obj) {

		size_t obj_size = json_object_size(js_obj);

		if (obj_size > 0) {
			html = pfc::string8(json_dumps(js_obj, 0));
		}
	}
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

	bool bres = false;

	pfc::string8 id;
	pfc::string8 url;
	pfc::array_t<Image_ptr>* images;
	size_t local_ndx = img_ndx;

	if (!isArtist) {

		id = release->id;
		if (!release->images.get_size() || release->images.get_size() < (img_ndx + 1) || (!release->images[img_ndx]->url150.get_length())) {

			pfc::string8 msg = "Unable to read album thumbnail from cache, index: ";
			msg << img_ndx;
			log_msg(msg);

			return false;
		}
		else {
			images = &release->images;
		}
	}
	else {

		Artist_ptr this_artist;

		img_artists_ndx_to_artist(release, img_ndx, this_artist, local_ndx);

		id = this_artist->id;

		if (!release->artists.get_size() || !this_artist.get() ||
			!this_artist->images.get_size() || this_artist->images.get_count() < (local_ndx + 1) ||
			!this_artist->images[local_ndx]->url150.get_length()) {

			pfc::string8 msg = "Unable to read artist thumbnail from cache, index: ";
			msg << local_ndx;
			log_msg(msg);

			return false;
		}
		else {
			images = &this_artist->images;
		}
	}

	size_t artist_offset = isArtist ? release->images.get_count() : 0;

	pfc::array_t<pfc::string8> n8_cache_thumbs;
	n8_cache_thumbs = ol::get_thumbnail_cache_path_filenames(id, isArtist ? art_src::art : art_src::alb, LVSIL_NORMAL, true, local_ndx);

	if (n8_cache_thumbs.get_count()) {

		p_status.set_item("fetching artwork preview small album art from cache ...");

		bool bexists = true;
		pfc::string8 n8_file_name = n8_cache_thumbs[0];

		int jf = -1;

		try {

			std::filesystem::path os_file = std::filesystem::u8path(n8_file_name.get_ptr());

			if (std::filesystem::exists(os_file)) {

				int filesize = std::filesystem::file_size(os_file);
				errno = 0;
				jf = _wopen(os_file.wstring().c_str(), _O_RDONLY | _O_BINARY);

				if (jf == -1) {
					foobar2000_io::exception_io e("Open failed on input file");
					throw e;
				}
				else {
					small_art.set_size(filesize);
					size_t done = _read(jf, small_art.get_ptr(), filesize);
					bres = done;
					_close(jf);
				}

				if (errno || !bres) {
					log_msg(PFC_string_formatter() << "can't open " << n8_file_name);
					return false;
				}
			}
			else {
				return false;
			}
		}
		catch (foo_discogs_exception& e) {
			if (jf != -1) {
				_close(jf);
			}
			throw(e);
			return false;
		}
		catch (...) {
			if (jf != -1) {
				_close(jf);
			}
			foo_discogs_exception e;
			e << "Unknown exception";
			throw(e);
			return false;
		}
	}
	return bres;
}

bool DiscogsInterface::delete_artist_cache(const pfc::string8& artist_id, const pfc::string8& release_id) {

	bool bonlyRelease = release_id.get_length();
	bool bmissingArtist = !artist_id.get_length();
	bool bmissingRelease = !release_id.get_length();

	Artist_ptr artist = get_artist_from_cache(artist_id);

	bool delres = true;

	//cache memory

	if (!bmissingArtist && artist.get()) {

		size_t lkey = encode_mr(0, pfc::string8(release_id));
		delres = cache_releases->remove(lkey);
		delres = cache_artists->remove(artist_id);

		if (bonlyRelease) {
			size_t lkey = encode_mr(artist->search_role_list_pos, pfc::string8(release_id));
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
			size_t lkey = encode_mr(0, pfc::string8(release_id));
			delres = cache_releases->remove(lkey);
			//todo: clean-up encode_mr with 0, artist_id and search_artist_rol pos
			if (artist) {
				lkey = encode_mr(artist->search_role_list_pos, pfc::string8(release_id));
				delres &= cache_releases->remove(lkey);
				lkey = encode_mr(atoi(artist->id), pfc::string8(release_id));
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
