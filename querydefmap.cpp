#pragma once
#include "stdafx.h"
#include "utils.h"
#include "conf.h"

#include "querydefmap.h"

//check various artists prefixes
extern pfc::string8 has_csv_prefix(const pfc::string& field, const pfc::string& csv, bool only_first_word, bool exact) {

	bool cfg_px_lnk_enabled = CONF.on_init_query_flags & InitQueryMng::INIT_QUERY_PX_LNK_ENABLED;
	if (!cfg_px_lnk_enabled) return "";

	size_t res = SIZE_MAX;

	std::vector<pfc::string8> vprefixes;
	split(csv, ",", 0, vprefixes);
	auto found_it = std::find_if(vprefixes.begin(), vprefixes.end(), [&](const pfc::string8& e) {
		pfc::string first_word = trim(e);
		if (exact) {
			bool beq = pfc::stringLite::g_equalsCaseInsensitive(first_word, field);
			return beq;
		}
		if (only_first_word) {
			first_word << " ";
		}

		if (field.get_length() < first_word.get_length()) {
			return false;
		}
		else {
			return pfc::stringLite::g_equalsCaseInsensitive(first_word, field.subString(0, first_word.get_length()));
		}
		});

	if (found_it == std::end(vprefixes)) return pfc::string8();
	return trim(found_it->c_str());
}

//check multiple artists joins
extern bool has_csv_links(pfc::string& field, const pfc::string& csv, bool replace) {

	bool cfg_px_lnk_enabled = CONF.on_init_query_flags & InitQueryMng::INIT_QUERY_PX_LNK_ENABLED;
	if (!cfg_px_lnk_enabled) return false;

	bool bres = false;
	pfc::array_t<pfc::string8> prepos;
	tokenize(csv, ",", prepos, false);
	for (auto& w : prepos) {
		w = PFC_string_formatter() << " " << w.trim(' ') << " ";
	}

	if (field.get_length()) {
		bool bcontains = false;
		for (auto wprepo : prepos) {

			if (bcontains = field.contains(wprepo)) {
				bres |= bcontains;
				if (!replace) {
					break;
				}
				else {
					field = field.replace(wprepo, " & ");
				}
			}
		}
	}

	return bres;
}

namespace search_query {

	bool MapToText(QueryDefMap qdm_search_query, std::pair<std::string, std::string>& out, bool alltypes, bool artist_or_q) {

		if (!qdm_search_query.size()) return {};

		pfc::string8 scaped;
		pfc::string8 sanitized;

		for (std::string fld : kv_query_fields_ordered) {

			if (qdm_search_query.at(fld).first.length()) {

				if (!fld.compare("type=") && !alltypes) {
					//skip type
					continue;
				}

				if (!fld.compare("artist=") && qdm_search_query.at("q=").first.size() && artist_or_q) {
					continue;
				}

				if (sanitized.length()) { sanitized += " & "; };
				if (scaped.length())    { scaped += "&"; };

				sanitized += (PFC_string_formatter() << fld.c_str() << qdm_search_query.at(fld).first.c_str()).c_str();
				scaped += (PFC_string_formatter() << fld.c_str() << qdm_search_query.at(fld).second.c_str()).c_str();

			}
		}
		out.first = sanitized.get_ptr();
		out.second = scaped.get_ptr();
		return sanitized.get_length();
	}

	bool TextToMap(const pfc::string8 expr, QueryDefMap& qdm_search_query) {

		auto c = search_query::CountAvailableFields(qdm_search_query);
		PFC_ASSERT(c == 0);

		for (const std::string fld : kv_query_fields_ordered) {
			qdm_search_query[fld] = {};
		}

		bool bquery = false;
		pfc::string8 trot = expr;

		// remove continous spaces

		pfc::string8 tmpstr = trot;
		std::regex regex_v_rcy_cont_sp;
		remove_continous_space(trot, trot, regex_v_rcy_cont_sp);

		for (const std::pair<std::string, FieldValPair> w : qdm_search_query) {
			std::string  tmp = w.first.substr(0, w.first.length() - 1).c_str();
			tmp += " =";
			std::string res = any_case_substring(trot.c_str(), tmp, w.first.c_str());
			if (res.compare(trot)) {
				remove_continous_space(res.c_str(), trot, regex_v_rcy_cont_sp);
			}
		}

		for (const std::pair<std::string, FieldValPair> w : qdm_search_query) {
			pfc::string8  tmp = w.first.substr(0, w.first.length() - 1).c_str();
			tmp += "=";
			std::string res = any_case_substring(trot.c_str(), tmp.c_str(), w.first.c_str());
			if (res.compare(trot)) {
				remove_continous_space(res.c_str(), trot, regex_v_rcy_cont_sp);
			}
		}

		for (std::pair<std::string, FieldValPair> w : qdm_search_query) {
			pfc::string8  tmp = w.first.substr(0, w.first.length() - 1).c_str();
			tmp += " =";
			trot = trot.replace(tmp, w.first.c_str());
		}

		for (const std::pair<std::string, FieldValPair> w : qdm_search_query) {
			pfc::string8  tmprep = "���";
			pfc::string8  tmp = "&";
			tmp += w.first.c_str();
			tmprep += w.first.c_str();
			trot = trot.replace(tmp, tmprep);

			tmp = "& ";
			tmp += w.first.c_str();
			trot = trot.replace(tmp, tmprep);
		}

		for (const std::pair<std::string, FieldValPair> w : qdm_search_query) {

			std::vector<pfc::string8> vsplit;

			split(trot, w.first.c_str(), 0, vsplit);

			if (vsplit.size() > 1) {

				auto uspos = vsplit[1].find_first("���");

				pfc::string8 query_fld_val_scaped;
				pfc::string8 query_fld_val_sani;

				if (uspos != SIZE_MAX) {

					query_fld_val_sani = vsplit[1].subString(0, uspos);

				}
				else {

					query_fld_val_sani = vsplit[1];

				}

				query_fld_val_sani = trim(query_fld_val_sani);
				query_fld_val_sani = query_fld_val_sani.replace("+ ", "+");
				query_fld_val_sani = query_fld_val_sani.replace(" +", "+");

				pfc::string8 no_apos = query_fld_val_sani;
				if (no_apos.replace_string_ex(no_apos, "\'", "")) {
					//..
				}
				pfc::string8 trot_scaped; trot_scaped.move(no_apos);
				if (!w.first.compare("q=")) {
						pfc::string8 tmp_str;
						if (trot_scaped.firstChar() != '\"') {
							tmp_str << "\"";
						}
						tmp_str << trot_scaped;
						if (trot_scaped.lastChar() != '\"') {
							tmp_str << "\"";
						}
						trot_scaped = tmp_str;
				}

				query_fld_val_scaped = urlEscape(trot_scaped);

				qdm_search_query[w.first].first = query_fld_val_sani;
				qdm_search_query[w.first].second = query_fld_val_scaped;

				bquery = true;
			}
		}

		pfc::string8 artist_name_mod = qdm_search_query.at("artist=").first.c_str();
		//artist_name = artist_name.toLower();

		pfc::string8 album_name = qdm_search_query.at("title=").first.c_str();
		pfc::string8 track_name = qdm_search_query.at("track=").first.c_str();
		bool has_title = album_name.get_length();

		bool use_q = false;

		use_q |= has_csv_links(artist_name_mod, CONF.multiple_artists_links, true);

		if (use_q) {
			qdm_search_query.at("q=") = FieldValPair(artist_name_mod, urlEscape(artist_name_mod));
			pfc::string8 new_q = urlEscape("\"");
			new_q << qdm_search_query.at("q=").second.c_str() << urlEscape("\"");
			qdm_search_query.at("q=").second = new_q;
		}

		if (bquery) {
			qdm_search_query["type="].first = "all";
			qdm_search_query["type="].second = "all";
		}

		return bquery;
	}

	bool UrlToMap(const pfc::string8 expr, QueryDefMap& qdm_search_query) {

		pfc::string8 url_prefix;
		url_prefix << DISCOGS_PUBLIC_SEARCH_URL << "?type=all&";

		if (expr.has_prefix(url_prefix)) {
			pfc::string8 buffer = expr.subString(url_prefix.length());
			return TextToMap(buffer, qdm_search_query);
		}
		else {
			pfc::string8 buffer = url_prefix.subString(0, url_prefix.get_length() - 4);
			if (expr.has_prefix(buffer)) {
				size_t as_pos = expr.find_first('&');
				if (as_pos != SIZE_MAX) {
					buffer = expr.subString(as_pos+1);
					return TextToMap(buffer, qdm_search_query);
				}
			}
		}
		return false;
	}

	bool IsMinimal(const QueryDefMap qdm_search_query) {

		bool b_artist = qdm_search_query.at("artist=").second.size();
		bool b_title = qdm_search_query.at("title=").second.size();
		bool b_track = qdm_search_query.at("track=").second.size();
		bool b_credit = qdm_search_query.at("credit=").second.size();
		bool b_catno = qdm_search_query.at("catno=").second.size();
		bool b_format = qdm_search_query.at("format=").second.size();
		bool b_year = qdm_search_query.at("year=").second.size();
		bool b_anv = qdm_search_query.at("anv=").second.size();
		bool b_q = qdm_search_query.at("q=").second.size();

		if ((b_artist && b_title)
			|| (b_artist && b_track)
			|| (b_artist && b_credit)
			|| (b_artist && b_format)
			|| (b_artist && b_year)
			|| (b_anv && b_title)
			|| (b_anv && b_track)
			|| (b_title && b_track)
			|| (b_title && b_credit)
			|| (b_title && b_format)
			|| (b_title && b_year)
			|| (b_credit && b_track && b_format)
			|| (b_title && b_catno)
			)
		{
			return true;
		}
		else if (b_q) {
			return true;
		}
		return false;
	}

	pfc::string8 IsMinimalHint(const QueryDefMap qdm_search_query) {

		if (IsMinimal(qdm_search_query)) {

			return "";

		}

		size_t c = 0;
		pfc::string8 str("Query is too short. Add ");

		for (auto h : kv_query_main_fields) {
			if (!qdm_search_query.at(h).second.size()) {
				if (c) str << ", ";
				str << h.c_str(); str.trim('=');
				if (++c > 1) {
					break;
				}
			}
		}

		str << "...";
		return str;
	}

	bool isVAQueryMinimal(const QueryDefMap qdm_search_query) {

		if (!qdm_search_query.size()) {
			//todo
			return false;
		}

		bool res = false;
		bool v = !qdm_search_query.at("artist=").first.compare("various");
		bool t = qdm_search_query.at("title=").first.size();
		bool c = qdm_search_query.at("credit=").first.size();
		bool trk = qdm_search_query.at("track=").first.size();

		res = (v && t) || (v && c) || (v && trk) || (t && c) || (t && trk);

		return res;
	}

	bool IsRunnable(const pfc::string8 search_expression) {

		QueryDefMap qdm_search_query = {};

		for (const std::string fld : kv_query_fields_ordered) {
			qdm_search_query[fld] = {};
		}

		bool bquery = search_query::TextToMap(search_expression, qdm_search_query);

		return bquery && search_query::IsMinimal(qdm_search_query);
	}

	size_t CountAvailableFields(const QueryDefMap qdm_search_query) {
		return std::count_if(qdm_search_query.begin(), qdm_search_query.end(), [](const auto vp) {
					return vp.second.first.size(); });
	}

	bool sanitize_various_artists_with_csv(const std::string& csv, pfc::string8& out) {

		pfc::string8 pfx = has_csv_prefix(out, csv.c_str(), true, false);

		if (pfx.get_length()) {

			out = "various";
			return true;
		}
		else {

			pfx = has_csv_prefix(out, csv.c_str(), false, true);

			if (pfx.get_length()) {
				out = "various";
				return true;
			}
		}
		return false;
	}

	//todo: depricate
	bool TestArtistInTitle(pfc::string8& frm_album_artist, pfc::string8& frm_artist, pfc::string8& frm_album,
		pfc::string8& frm_track_title, const pfc::string& csv) {

		bool no_artist = !(frm_album_artist.get_length() || frm_artist.get_length());

		size_t spos = frm_album.find_first(' ', 0);
		if (spos == SIZE_MAX) {
			return false;
		}

		pfc::string8 first_album_word = trim(frm_album.subString(0, spos));
		first_album_word << " ";

		pfc::string8 pfx = has_csv_prefix(first_album_word, csv, true, false);

		if (pfx.get_length()) {
			frm_album_artist = first_album_word;
			frm_album = frm_album.subString(spos + 1);

			if (trim(frm_album).firstChar() == '-') {
				frm_album = trim(frm_album.subString(1));
			}
			return true;
		}

		return false;

		std::vector<pfc::string8>vsplit;
		split(frm_album, "-", 0, vsplit);
		if (vsplit.size() > 2) {
			if (trim(vsplit[0]).equals(first_album_word)) {
				//..
			}
		}

		return false;
	}

	bool GetSuggestion(pfc::string8 frm_album_artist, pfc::string8 frm_artist, pfc::string8 frm_album, pfc::string8 frm_track_title, pfc::string8 frm_sani_artist, pfc::string8 frm_custom_tf, size_t ndx, pfc::string8& out) {

		if(frm_album_artist.equals("various artist")) {
			frm_artist = "various";
		}
		if (!frm_album_artist)
		{
			frm_album_artist = frm_artist;
		}

		if (ndx == 0) {
			//custom search
			out = frm_custom_tf;
			return frm_custom_tf.get_length();
		}
		else if (ndx == 9) {
			//plain artist search (no query)
			if (frm_album_artist.get_length()) out = frm_album_artist;
			else if (frm_artist.get_length()) out = frm_artist;
			else out = "";
			return out.get_length();
		}
		else if (ndx == 1) {
			// artist & title
			if (frm_album_artist.get_length()) {
				pfc::string8 lbl;
				lbl << "artist= " << frm_album_artist;
				if (frm_album.get_length()) {
					lbl << " & title= " << frm_album;
				}

				out = lbl;
				return true;
			}
		}
		else if (ndx == 2) {
			// artist & track
			if (frm_album_artist.get_length()) {
				pfc::string8 lbl;
				lbl << "artist= " << frm_album_artist;
				if (frm_track_title.get_length()) {
					lbl << " & track= " << frm_track_title;
				}

				out = lbl;
				return true;
			}
		}
		else if (ndx == 3) {
			//title & track
			if (frm_album.get_length()) {
				pfc::string8 lbl;
				lbl << "title= " << frm_album;
				if (frm_track_title.get_length()) {
					lbl << " & track= " << frm_track_title;
				}

				out = lbl;
				return true;
			}
		}
		else if (ndx == 4) {
			//title & credit
			if (frm_album.get_length()) {
				pfc::string8 lbl;
				lbl << "title= " << frm_album;
				if (frm_artist.get_length()) {
					if (!frm_sani_artist.equals("various")) {
						lbl << " & credit= " << frm_artist;
					}
					out = lbl;
					return true;
				}
			}
		}
		else if (ndx == 5) {
			// q & title
			if (frm_album_artist.get_length() && frm_album.get_length() && !frm_album_artist.equals("various")) {
				pfc::string8 lbl;
				lbl << "q= " << frm_album_artist;
				if (frm_album.get_length()) {
					lbl << " & title= " << frm_album;
				}

				out = lbl;
				return true;
			}
		}
		else if (ndx == 6) {
			//artist & title & credit
			if (frm_album_artist.get_length()) {
				pfc::string8 lbl;
				lbl << "artist= " << frm_album_artist;
				if (frm_album.get_length()) {
					lbl << " & title= " << frm_album;
					if (frm_artist.get_length() && !frm_sani_artist.equals("various")) {
						lbl << " & credit= " << frm_artist;
						out = lbl;
						return true;
					}
				}
			}
		}
		else if (ndx == 7) {
			//title & track & credit (track artist)
			if (frm_album.get_length()) {
				pfc::string8 lbl;
				lbl << "title= " << frm_album;
				if (frm_track_title.get_length()) {
					lbl << " & track= " << frm_track_title;
					if (frm_artist.get_length()) {
						lbl << " & credit= " << frm_artist;
						out = lbl;
						return true;
					}
				}
			}
		}
		return false;
	}

}
