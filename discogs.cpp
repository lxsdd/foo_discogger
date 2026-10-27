#include "stdafx.h"
#include <unordered_set>
#include <string>
#include <vector>
#include "jansson/jansson.h"

#include "discogs_interface.h"

#include "foo_discogs.h"
#include "utils.h"
#include "json_helpers.h"
#include "querydefmap.h"
#include "discogs.h"
#include "ol_cache.h"
#include "discogger_cache_pages.h"  // paginated release list completeness guard
#include "discogger_master_versions.h"
#include "discogger_master_page_transaction.h"
#include "discogger_release_id.h"

using namespace Discogs;

const ExposedMap<ReleaseLabel> ExposedTags<ReleaseLabel>::exposed_tags = ReleaseLabel::create_tags_map();
const ExposedMap<ReleaseCompany> ExposedTags<ReleaseCompany>::exposed_tags = ReleaseCompany::create_tags_map();
const ExposedMap<ReleaseSeries> ExposedTags<ReleaseSeries>::exposed_tags = ReleaseSeries::create_tags_map();
const ExposedMap<ReleaseTrack> ExposedTags<ReleaseTrack>::exposed_tags = ReleaseTrack::create_tags_map();
const ExposedMap<ReleaseHeading> ExposedTags<ReleaseHeading>::exposed_tags = ReleaseHeading::create_tags_map();
const ExposedMap<ReleaseIndexes> ExposedTags<ReleaseIndexes>::exposed_tags = ReleaseIndexes::create_tags_map();
const ExposedMap<Release> ExposedTags<Release>::exposed_tags = Release::create_tags_map();
const ExposedMap<MasterRelease> ExposedTags<MasterRelease>::exposed_tags = MasterRelease::create_tags_map();
const ExposedMap<Artist> ExposedTags<Artist>::exposed_tags = Artist::create_tags_map();
const ExposedMap<ReleaseArtist> ExposedTags<ReleaseArtist>::exposed_tags = ReleaseArtist::create_tags_map();
const ExposedMap<ReleaseCredit> ExposedTags<ReleaseCredit>::exposed_tags = ReleaseCredit::create_tags_map();
const ExposedMap<ReleaseFormat> ExposedTags<ReleaseFormat>::exposed_tags = ReleaseFormat::create_tags_map();
const ExposedMap<ReleaseDisc> ExposedTags<ReleaseDisc>::exposed_tags = ReleaseDisc::create_tags_map();
const ExposedMap<ExpTagsImage> ExposedTags<ExpTagsImage>::exposed_tags = ExpTagsImage::create_tags_map();


string_encoded_array Discogs::ReleaseArtist::get_data(pfc::string8 &tag_name, threaded_process_status &p_status, abort_callback &p_abort) {
	try {
		return ExposedTags<ReleaseArtist>::get_data(tag_name, p_status, p_abort);
	}
	catch (missing_data_exception) {
		return full_artist->get_data(tag_name, p_status, p_abort);
	}
}


string_encoded_array Discogs::Artist::get_sub_data(pfc::string8 &tag_name, threaded_process_status &p_status, abort_callback &p_abort) {
	pfc::string8 sub_tag_name;
	string_encoded_array result;
	if (STR_EQUALN(tag_name, "IMAGES_", 7)) {
		result.force_array(); 
		sub_tag_name = substr(tag_name, 7);
		load(p_status, p_abort);
		for (size_t i = 0; i < images.get_size(); i++) {
			result.append_item_val(images[i]->get_data(sub_tag_name, p_status, p_abort));
		}
	}
	else if (STR_EQUALN(tag_name, "RELEASES_", 9)) {
		result.force_array();
		sub_tag_name = substr(tag_name, 9);
		load_releases(p_status, p_abort, false, nullptr);
		for (size_t i = 0; i < releases.get_size(); i++) {
			result.append_item_val(releases[i]->get_data(sub_tag_name, p_status, p_abort));
		}
	}
	else {
		return ExposedTags<Artist>::get_sub_data(tag_name, p_status, p_abort);
	}
	result.encode();
	return result;
}


string_encoded_array Discogs::ReleaseCredit::get_sub_data(pfc::string8 &tag_name, threaded_process_status &p_status, abort_callback &p_abort) {
	pfc::string8 sub_tag_name;
	string_encoded_array result;
	if (strncmp(tag_name.get_ptr(), "ARTISTS_", 8) == 0) {
		result.force_array(); 
		sub_tag_name = pfc::string8(tag_name.get_ptr() + 8);
		for (size_t i = 0; i < artists.get_size(); i++) {
			result.append_item_val(artists[i]->get_data(sub_tag_name, p_status, p_abort));
		}
	}
	else {
		return ExposedTags<ReleaseCredit>::get_sub_data(tag_name, p_status, p_abort);
	}
	result.encode();
	return result;
}


string_encoded_array Discogs::ReleaseTrack::get_sub_data(pfc::string8 &tag_name, threaded_process_status &p_status, abort_callback &p_abort) {
	pfc::string8 sub_tag_name;
	string_encoded_array result;
	if (strncmp(tag_name.get_ptr(), "ARTISTS_", 8) == 0) {
		result.force_array();
		sub_tag_name = pfc::string8(tag_name.get_ptr() + 8);
		for (size_t i = 0; i < artists.get_size(); i++) {
			result.append_item_val(artists[i]->get_data(sub_tag_name, p_status, p_abort));
		}
	}
	else if (strncmp(tag_name.get_ptr(), "CREDITS_", 8) == 0) {
		result.force_array(); 
		sub_tag_name = pfc::string8(tag_name.get_ptr() + 8);
		for (size_t i = 0; i < credits.get_size(); i++) {
			result.append_item_val(credits[i]->get_data(sub_tag_name, p_status, p_abort));
		}
	}
	else if (strncmp(tag_name.get_ptr(), "HIDDEN_TRACKS_", 14) == 0) {
		result.force_array(); 
		sub_tag_name = pfc::string8(tag_name.get_ptr() + 14);
		for (size_t i = 0; i < hidden_tracks.get_size(); i++) {
			result.append_item_val(hidden_tracks[i]->get_data(sub_tag_name, p_status, p_abort));
		}
	}
	else {
		return ExposedTags<ReleaseTrack>::get_sub_data(tag_name, p_status, p_abort);
	}
	result.encode();
	return result;
}

string_encoded_array Discogs::ReleaseDisc::get_sub_data(pfc::string8 &tag_name, threaded_process_status &p_status, abort_callback &p_abort) {
	pfc::string8 sub_tag_name;
	string_encoded_array result;
	if (STR_EQUALN(tag_name, "TRACKS_", 7)) {
		result.force_array();
		sub_tag_name = substr(tag_name, 7);
		for (size_t i = 0; i < tracks.get_size(); i++) {
			result.append_item_val(tracks[i]->get_data(sub_tag_name, p_status, p_abort));
		}
	}
	else
	{
		return ExposedTags<ReleaseDisc>::get_sub_data(tag_name, p_status, p_abort);
	}
	result.encode();
	return result;
}

string_encoded_array Discogs::Release::get_sub_data(pfc::string8 &tag_name, threaded_process_status &p_status, abort_callback &p_abort) {
	pfc::string8 sub_tag_name;
	string_encoded_array result;
	if (STR_EQUALN(tag_name, "TRACKS_", 7)) {
		result.force_array(); 
		load(p_status, p_abort);
		for (size_t i = 0; i < discs.get_size(); i++) {
			if (i == 0) {
				result = discs[i]->get_data(tag_name, p_status, p_abort);
			}
			else {
				result.shallow_extend(discs[i]->get_data(tag_name, p_status, p_abort));
			}
		}
	}
	else if (STR_EQUALN(tag_name, "DISCS_", 6)) {
		result.force_array(); 
		sub_tag_name = substr(tag_name, 6);
		load(p_status, p_abort); 
		for (size_t i = 0; i < discs.get_size(); i++) {
			result.append_item_val(discs[i]->get_data(sub_tag_name, p_status, p_abort));
		}
	}
	else if (STR_EQUALN(tag_name, "ARTISTS_", 8)) {
		result.force_array(); 
		sub_tag_name = substr(tag_name, 8);
		load(p_status, p_abort); 
		for (size_t i = 0; i < artists.get_size(); i++) {
			result.append_item_val(artists[i]->get_data(sub_tag_name, p_status, p_abort));
		}
	}
	else if (STR_EQUALN(tag_name, "LABELS_", 7)) {
		result.force_array(); 
		sub_tag_name = substr(tag_name, 7);
		load(p_status, p_abort); 
		for (size_t i = 0; i < labels.get_size(); i++) {
			result.append_item_val(labels[i]->get_data(sub_tag_name, p_status, p_abort));
		}
	}
	else if (STR_EQUALN(tag_name, "COMPANIES_", 10)) {
		result.force_array();
		sub_tag_name = substr(tag_name, 10);
		load(p_status, p_abort);
		for (size_t i = 0; i < companies.get_size(); i++) {
			result.append_item_val(companies[i]->get_data(sub_tag_name, p_status, p_abort));
		}
	}
	else if (STR_EQUALN(tag_name, "SERIES_", 7)) {
		result.force_array(); 
		sub_tag_name = substr(tag_name, 7);
		load(p_status, p_abort); 
		for (size_t i = 0; i < series.get_size(); i++) {
			result.append_item_val(series[i]->get_data(sub_tag_name, p_status, p_abort));
		}
	}
	else if (STR_EQUALN(tag_name, "FORMATS_", 8)) {
		result.force_array(); 
		sub_tag_name = substr(tag_name, 8);
		load(p_status, p_abort); 
		for (size_t i = 0; i < formats.get_size(); i++) {
			result.append_item_val(formats[i]->get_data(sub_tag_name, p_status, p_abort));
		}
	}
	else if (STR_EQUALN(tag_name, "CREDITS_", 8)) {
		result.force_array(); 
		sub_tag_name = substr(tag_name, 8);
		load(p_status, p_abort);
		for (size_t i = 0; i < credits.get_size(); i++) {
			result.append_item_val(credits[i]->get_data(sub_tag_name, p_status, p_abort));
		}
	}
	else if (STR_EQUALN(tag_name, "IMAGES_", 7)) {
		result.force_array(); 
		sub_tag_name = substr(tag_name, 7);
		load(p_status, p_abort);
		for (size_t i = 0; i < images.get_size(); i++) {
			result.append_item_val(images[i]->get_data(sub_tag_name, p_status, p_abort));
		}
	}
	else if (STR_EQUALN(tag_name, "MASTER_RELEASE_", 15)) {
		sub_tag_name = substr(tag_name, 15);
		load_preview(p_status, p_abort);
		if (master_release) {
			result = master_release->get_data(sub_tag_name, p_status, p_abort);
		}
	}
	else {
		return ExposedTags<Release>::get_sub_data(tag_name, p_status, p_abort);
	}
	result.encode();
	return result;
}


string_encoded_array Discogs::MasterRelease::get_sub_data(pfc::string8 &tag_name, threaded_process_status &p_status, abort_callback &p_abort) {
	pfc::string8 sub_tag_name;
	string_encoded_array result;
	if (STR_EQUALN(tag_name, "TRACKS_", 7)) {
		result.force_array(); 
		load(p_status, p_abort);
		for (size_t i = 0; i < discs.get_size(); i++) {
			if (i == 0) {
				result = discs[i]->get_data(tag_name, p_status, p_abort);
			}
			else {
				result.shallow_extend(discs[i]->get_data(tag_name, p_status, p_abort));
			}
		}
	}
	else if (STR_EQUALN(tag_name, "DISCS_", 6)) {
		result.force_array(); 
		sub_tag_name = substr(tag_name, 6);
		load(p_status, p_abort); 
		for (size_t i = 0; i < discs.get_size(); i++) {
			result.append_item_val(discs[i]->get_data(sub_tag_name, p_status, p_abort));
		}
	}
	else if (STR_EQUALN(tag_name, "ARTISTS_", 8)) {
		result.force_array(); 
		sub_tag_name = substr(tag_name, 8);
		load(p_status, p_abort); 
		for (size_t i = 0; i < artists.get_size(); i++) {
			result.append_item_val(artists[i]->get_data(sub_tag_name, p_status, p_abort));
		}
	}
	else if (STR_EQUALN(tag_name, "IMAGES_", 7)) {
		result.force_array(); 
		sub_tag_name = substr(tag_name, 7);
		load(p_status, p_abort); 
		for (size_t i = 0; i < images.get_size(); i++) {
			result.append_item_val(images[i]->get_data(sub_tag_name, p_status, p_abort));
		}
	}
	else if (STR_EQUALN(tag_name, "MAIN_RELEASE_", 13)) {
		sub_tag_name = substr(tag_name, 13);
		load_preview(p_status, p_abort);
		if (main_release) {
			result = main_release->get_data(sub_tag_name, p_status, p_abort);
		}
	}
	else if (STR_EQUALN(tag_name, "RELEASES_", 9)) {
		result.force_array(); 
		sub_tag_name = substr(tag_name, 9);
		load_releases(p_status, p_abort);
		for (size_t i = 0; i < sub_releases.get_size(); i++) {
			result.append_item_val(sub_releases[i]->get_data(sub_tag_name, p_status, p_abort));
		}
	}
	else {
		return ExposedTags<MasterRelease>::get_sub_data(tag_name, p_status, p_abort);
	}
	result.encode();
	return result;
}


inline void Discogs::MasterRelease::set_main_release(Release_ptr main) {
	main_release = main;
	if (main) {
		main_release_id = main->id;
	}
}

inline void Discogs::Release::set_master_release(MasterRelease_ptr master) {
	master_release = master;
	if (master) {
		master_id = master->id;
	}
}


pfc::string8 Discogs::remove_number_suffix(const pfc::string8& src) {

	if (src.get_length() < 4) {

		return src.c_str();
	}

	pfc::string8 dst = src;
	unsigned src_size = src.get_length();
	std::regex regex_v;
	try {
		regex_v = std::regex("\\(\\d+?\\)");
	}
	catch (std::regex_error e) {
		// A failed suffix normalization must not discard the original title.
		return src;
	}
	try {
		dst.set_string(std::regex_replace(dst.get_ptr(), regex_v, "").c_str());
		while (dst.length() > 0 && dst.endsWith(' '))
			dst.truncate(dst.length() - 1);
	}
	catch (std::regex_error e) {
		// A failed suffix normalization must not discard the original title.
		return src;
	}
	return dst;
}

pfc::string8 Discogs::move_the_to_end(const pfc::string8 &src) {
	if (src.get_length() < 5) {
		return src;
	}
	pfc::string8 tmp = src;
	pfc::string8 start = substr(tmp, 0, 4);
	if (STR_EQUAL(start, "The ") || STR_EQUAL(start, "the ")) {
		tmp = substr(tmp, 4, src.get_length());
		tmp << ", The";
	}
	return tmp;
}

pfc::string8 Discogs::move_the_to_start(const pfc::string8 &src) {
	if (src.get_length() < 5) {
		return src;
	}
	pfc::string8 tmp = src;
	pfc::string8 end = substr(tmp, src.get_length() - 5, 5);
	if (STR_EQUAL(end, ", The") || STR_EQUAL(end, ", the")) {
		pfc::string8 ret("The ");
		ret << substr(tmp, 0, src.length() - 5);
		return ret;
	}
	return src;
}

pfc::string8 Discogs::strip_artist_name(const pfc::string8 str) {
	pfc::string8 result = lowercase(str);
	result = rtrim(result);
	if (result.length() > 0) {
		if (result[result.length() - 1] == ')') {
			result = substr(result, 0, result.find_last('('));
			result = rtrim(result);
		}
		if (result.length() > 5 && STR_EQUAL(substr(result, result.length() - 5, 5), ", the")) {
			result = substr(result, 0, result.length() - 5);
			result = rtrim(result);
		}
		if (result.length() > 4 && STR_EQUAL(substr(result, result.length() - 4, 4), "the ")) {
			result = substr(result, 4);
			result = rtrim(result);
		}
	}
	return result;
}

ReleaseArtist_ptr Discogs::parseReleaseArtist(json_t *element, bool preload) {
	assert_is_object(element);
	pfc::string8 artist_id = JSONAttributeString(element, "id");
	pfc::string8 name = JSONAttributeString(element, "name");
	pfc::string8 anv = JSONAttributeString(element, "anv");
	auto full_artist = discogs_interface->get_artist(artist_id);

	ReleaseArtist_ptr artist(new ReleaseArtist(artist_id, full_artist, preload));
	artist->name = name;
	artist->anv = anv;
	artist->raw_roles = JSONAttributeString(element, "role");
	tokenize_non_bracketed(artist->raw_roles, ",", artist->full_roles, true);
	for (size_t i = 0; i < artist->full_roles.get_size(); i++) {
		size_t pos = artist->full_roles[i].find_first('[');
		if (pos != pfc::infinite_size) {
			artist->roles.append_single(trim(substr(artist->full_roles[i], 0, pos)));
		}
		else {
			artist->roles.append_single(artist->full_roles[i]);
		}
	}
	artist->join = JSONAttributeString(element, "join");
	return std::move(artist);
}

void Discogs::parseReleaseArtists(json_t *element, pfc::array_t<ReleaseArtist_ptr> &artists, bool preload) {
	assert_is_array(element);
	for (size_t i = 0; i < json_array_size(element); i++) {
		json_t *artist = json_array_get(element, i);
		artists.append_single(parseReleaseArtist(artist, preload));
	}
}

pfc::array_t<pfc::string8> get_delims() {
	pfc::array_t<pfc::string8> v;
	v.append_single(pfc::string8(","));
	v.append_single(pfc::string8(";"));
	v.append_single(pfc::string8(" and "));
	v.append_single(pfc::string8("&"));
	//v.append_single(pfc::string8(" & "));
	return v;
}

const pfc::array_t<pfc::string8> cCREDIT_DELIMS = get_delims();

void Discogs::parseReleaseCredits(json_t* element, pfc::array_t<ReleaseCredit_ptr> &credits, Release *release) {
	assert_is_array(element);
	pfc::string8 last_role;
	std::regex regex_v_rcy_sani_commas;
	for (size_t i = 0; i < json_array_size(element); i++) {
		json_t *j = json_array_get(element, i);
		ReleaseArtist_ptr artist = parseReleaseArtist(j);
		const pfc::string8 &tracks = JSONAttributeString(j, "tracks");

		if (tracks.get_length() && !tracks.equals("All")) {

			ReleaseCredit_ptr credit(new ReleaseCredit());
			credit->raw_roles = artist->raw_roles;
			credit->roles = artist->roles;
			credit->full_roles = artist->full_roles;
			credit->artists.append_single(artist);

			try {
				pfc::array_t<pfc::string8> array_parts;

				pfc::string8 sani_to_tracks = sanitize_track_to(tracks);
				size_t count = tokenize_multi(sanitize_track_commas(sani_to_tracks, regex_v_rcy_sani_commas), cCREDIT_DELIMS, array_parts, true, true);

				DistReleaseTrackCredits(array_parts, credit, release);
			}
			catch (parser_exception& e) {
				pfc::string8 error;
				error << ("(skipped) ") << e.what() << "\n";
				popup_message::g_show(error.get_ptr(), "Error(s)", popup_message::icon_error);
			}
		}
		else {
			//1.0.22 beta search crash: artist= Various artists & format = cd
			if (!artist->roles.get_ptr() && !artist->raw_roles.get_length() && !credits.get_ptr()) {

				log_msg(PFC_string_formatter() << "Skipping empty/orphan extra artist credit.");
				continue;
			}

			try {
				if (!artist->raw_roles.get_length() || (credits.get_size() && STR_EQUAL(artist->raw_roles, credits[credits.get_size() - 1]->raw_roles))) {
					credits[credits.get_size() - 1]->artists.append_single(artist);
				}
				else {

					ReleaseCredit_ptr credit(new ReleaseCredit());
					credit->raw_roles = artist->raw_roles;
					credit->roles = artist->roles;
					credit->full_roles = artist->full_roles;
					credit->artists.append_single(artist);
					credits.append_single(credit);
				}
			}
			catch (parser_exception& e) {
				pfc::string8 error;
				error << ("(skipped) ") << e.what() << "\n";
				popup_message::g_show(error.get_ptr(), "Error(s)", popup_message::icon_error);
			}
		}
	}
}

//todo: cleanup/fix alphanumeric notation
unsigned long long encode_track_position(const pfc::string8 &pos) {
	// first 1-8 characters encoded
	unsigned long long result = 0;
	unsigned char temp = 0;
	bool use_temp = false;
	size_t count = 0;
	const size_t length = pos.get_length();
	for (size_t i = 0; count < 8 && i < length; i++) {
		if (pos[i] >= 48 && pos[i] <= 57) {
			temp = temp * 10 + (pos[i] - 48);
			use_temp = true;
		}
		else {
			if (use_temp) {
				result |= temp;
				use_temp = false;
				temp = 0;
				count++;
				if (count < 7) {
					result <<= 8;
				}
			}
			result |= pos[i];
			count++;
			if (count < 7) {
				result <<= 8;
			}
		}
	}
	if (use_temp) {
		result |= temp;
		count++;
		if (count < 7) {
			result <<= 8;
		}
	}
	if (count < 7) {
		result <<= (count * 8);
	}
	return result;
}

bool track_in_range(const pfc::string8 &pos, const pfc::string8 &start, const pfc::string8 &end) {
	unsigned long long dpos = encode_track_position(pos);
	unsigned long long dstart = encode_track_position(start);
	unsigned long long dend = encode_track_position(end);
	return dpos >= dstart && dpos <= dend;
}


size_t get_last_num(pfc::string8 str, size_t minlength, size_t minpos, pfc::string8& lastnum) {

	if (pfc::string_is_numeric(str)) return ~0;

	if (str.get_length() >= minlength) {
		std::regex regex_v("[\\d]+");
		std::string str_reg(str.c_str());
		std::sregex_iterator begin = std::sregex_iterator(str_reg.begin(), str_reg.end(), regex_v);
		for (std::sregex_iterator i = begin; i != std::sregex_iterator(); i++) {
			if (i->position() >= minpos) {
				lastnum = i->str().c_str();
				return i->position();
			}
		}
	}
	return ~0;
}

void Discogs::DistReleaseTrackCredits(const pfc::array_t<pfc::string8>& arrTracks, ReleaseCredit_ptr& credit, Release* release) {

	pfc::string8 alltracks_csv;
	std::vector<pfc::string8> v_media_desc;
	nota_info n_info;

	size_t cTracks = arrTracks.get_count();

	if (!cTracks) {
		//break recursion
		return;
	}


	pfc::string8 dc_credit_pos = arrTracks[0];

	// check credit movement

	pfc::string8 mov_credit_pos;
	pfc::string8 dc_credit_big_pos;
	get_last_num(dc_credit_pos, 3, 0, dc_credit_big_pos);
	if (release->indexes.get_count() && dc_credit_big_pos.get_length() > 2) {

		std::vector<pfc::string8> vindex_mov_nums;
		for (const auto& var : release->indexes) {
			auto bignum = extract_max_number(var->title, 'z', false);
			if (bignum.get_length()) {
				vindex_mov_nums.emplace_back(bignum);
			}
		}

		if (std::find(std::begin(vindex_mov_nums), std::end(vindex_mov_nums), dc_credit_big_pos.c_str()) != std::end(vindex_mov_nums)) {

			std::vector<pfc::string8> vsplit;
			split(dc_credit_pos, "-", 0, vsplit);
			for (pfc::string8 wmovement : vsplit) {
				if (!trim(wmovement).get_length()) {
					continue;
				}

				auto relh = release->indexes;
				for (const auto& var : relh) {
					std::vector<std::string> mylist = { "Concerto", "Sonata", "Suite" };
					auto found_it = std::find_if(mylist.begin(), mylist.end(), [=](const std::string& w) {
						return var->title.contains(w.c_str());
						});
					if (found_it != mylist.end()) {
						if (var->title.contains(wmovement)) {
							if (mov_credit_pos.get_length()) {
								mov_credit_pos << ",";
							}
							mov_credit_pos = var->dc_track_first;

							if (!var->dc_track_first.equals(var->dc_track_last)) {
								mov_credit_pos << " to " << var->dc_track_last;
							}
							break;
						}
					}
				}
			}
		}

	}
	dc_credit_pos = mov_credit_pos.get_length() ? mov_credit_pos : dc_credit_pos;
	//.. end check credit movement

	pfc::array_t<pfc::string8> inner_parts;
	size_t inner_count = tokenize(dc_credit_pos, " to ", inner_parts, true);

	if (!inner_count || inner_count > 2) {
		parser_exception ex;
		ex << "Error parsing release credits.";
		throw ex;
	}
	else if (inner_count < 2) {
		int tmp_disc, tmp_track;
		replace_track_volume_desc(inner_parts[0], n_info, v_media_desc);

		bool bres = release->find_std_disc_track(inner_parts[0], &tmp_disc, &tmp_track);
		if (bres) {
			if (tmp_disc < release->discs.get_size() && tmp_track < release->discs[tmp_disc]->tracks.get_size())
				release->discs[tmp_disc]->tracks[tmp_track]->credits.append_single(credit);
		}
	}
	else if (inner_count == 2) {

		std::vector<std::pair<size_t, size_t>> inner_std_parts;
		int tmp_disc, tmp_track;

		// ->o to p
		replace_track_volume_desc(inner_parts[0], n_info, v_media_desc);
		{
			bool bresleft = release->find_std_disc_track(inner_parts[0], &tmp_disc, &tmp_track);
			if (bresleft) {
				inner_std_parts.emplace_back(tmp_disc, tmp_track);
			}
		}
		// o to ->p
		replace_track_volume_desc(inner_parts[1], n_info, v_media_desc);
		{
			bool bresright = release->find_std_disc_track(inner_parts[1], &tmp_disc, &tmp_track);
			if (bresright) {
				inner_std_parts.emplace_back(tmp_disc, tmp_track);
			}
		}
		if (inner_std_parts.size() == 2) {

			pfc::string8 range_botton = PFC_string_formatter() << inner_std_parts[0].first << "." << inner_std_parts[0].second;
			pfc::string8 range_top = PFC_string_formatter() << inner_std_parts[1].first << "." << inner_std_parts[1].second;

			//check bounds
			if (inner_std_parts[0].first < release->discs.get_size() &&
				inner_std_parts[1].first < release->discs.get_size() &&
				inner_std_parts[0].second < release->discs[inner_std_parts[0].first]->tracks.get_size() &&
				inner_std_parts[1].second < release->discs[inner_std_parts[1].first]->tracks.get_size()) {

				for (size_t d = inner_std_parts[0].first; d <= inner_std_parts[1].first; d++) {
					for (size_t t = 0; t < release->discs[d]->tracks.get_size(); t++) {

						pfc::string8 pos = release->discs[d]->tracks[t]->discogs_track_number;
						if (release->find_std_disc_track(pos, &tmp_disc, &tmp_track)) {
							pos = PFC_string_formatter() << tmp_disc << "." << tmp_track;
							//todo: fix/cleanup to support release track notation
							if (track_in_range(pos, range_botton, range_top)) {
								release->discs[d]->tracks[t]->credits.append_single(credit);
							}
						}
					}
				}
			}
		}
	}

	if (cTracks > 1) {
		//tail recursion
		//todo: pass 'from' position instead 
		pfc::array_t<pfc::string8> arrTracksTail; arrTracksTail.resize(arrTracks.get_count() - 1);
		int c = 1;
		for (auto& ti : arrTracksTail) {
			ti = arrTracks[c++];
		}
		DistReleaseTrackCredits(arrTracksTail, credit, release);
	}
}

ReleaseLabel_ptr Discogs::parseReleaseLabel(json_t *element) {
	ReleaseLabel_ptr label(new ReleaseLabel());
	label->name = JSONAttributeString(element, "name");
	label->id = JSONAttributeString(element, "id");
	label->catalog = JSONAttributeString(element, "catno");
	return std::move(label);
}

void Discogs::parseReleaseLabels(json_t *element, pfc::array_t<ReleaseLabel_ptr> &release_labels) {
	if (json_is_array(element)) {
		for (size_t i = 0; i < json_array_size(element); i++) {
			release_labels.append_single(parseReleaseLabel(json_array_get(element, i)));
		}
	}
}

ReleaseCompany_ptr Discogs::parseReleaseCompany(json_t *element) {
	ReleaseCompany_ptr company(new ReleaseCompany());
	company->name = JSONAttributeString(element, "name");
	company->id = JSONAttributeString(element, "id");
	company->catalog = JSONAttributeString(element, "catno");
	company->entity_type_name = JSONAttributeString(element, "entity_type_name");
	return std::move(company);
}

void Discogs::parseReleaseCompanies(json_t *element, pfc::array_t<ReleaseCompany_ptr> &release_companies) {
	if (json_is_array(element)) {
		for (size_t i = 0; i < json_array_size(element); i++) {
			release_companies.append_single(parseReleaseCompany(json_array_get(element, i)));
		}
	}
}

ReleaseSeries_ptr Discogs::parseReleaseSeries(json_t *element) {
	ReleaseSeries_ptr series(new ReleaseSeries());
	series->name = JSONAttributeString(element, "name");
	series->id = JSONAttributeString(element, "id");
	series->api_url = JSONAttributeString(element, "resource_url");
	series->number = JSONAttributeString(element, "catno");
	return std::move(series);
}

void Discogs::parseReleaseSeries(json_t *element, pfc::array_t<ReleaseSeries_ptr> &release_series) {
	if (json_is_array(element)) {
		for (size_t i = 0; i < json_array_size(element); i++) {
			release_series.append_single(parseReleaseSeries(json_array_get(element, i)));
		}
	}
}

Image_ptr Discogs::parseImage(json_t *element) {
	Image_ptr image(new ExpTagsImage());
	// Should we use thumb instead of url150?
	image->type = JSONAttributeString(element, "type");
	image->url = JSONAttributeString(element, "resource_url");
	image->url150 = JSONAttributeString(element, "uri150");
	image->width = JSONAttributeString(element, "width");
	image->height = JSONAttributeString(element, "height");
	return std::move(image);
}

void Discogs::parseImages(json_t *array, pfc::array_t<Image_ptr> &images) {
	const size_t param_image_count = images.size();
	pfc::array_t<Image_ptr> sec_images;
	if (json_is_array(array)) {
		for (size_t i = 0; i < json_array_size(array); i++) {
			json_t *img = json_array_get(array, i);
			if (json_is_object(img)) {
				Image_ptr image = parseImage(img);
				if (STR_EQUAL(image->type, "primary")) {
					images.append_single(std::move(image));
				}
				else {
					sec_images.append_single(std::move(image));
				}
			}
		}
	}
	if (images.size()) {
		images.append(sec_images);
	}
	else {
		//todo: rev
		images.append(sec_images);
	}
	if (images.size() - param_image_count != json_array_size(array)) {
		pfc::string8 msg("Error parsing ");
		msg << PFC_string_formatter() << (json_array_size(array) - sec_images.size());
		msg << PFC_string_formatter() << " out of " << json_array_size(array) << " images";
		log_msg(msg);
	}
}

ReleaseFormat_ptr Discogs::parseReleaseFormat(json_t *element) {
	ReleaseFormat_ptr format(new ReleaseFormat());
	format->set_name(JSONAttributeString(element, "name"));
	format->qty = JSONAttributeString(element, "qty");
	format->descriptions = JSONAttributeStringArray(element, "descriptions");
	format->text = JSONAttributeString(element, "text");
	return format;
}

void Discogs::parseReleaseFormats(json_t *element, pfc::array_t<ReleaseFormat_ptr> &formats) {
	if (json_is_array(element)) {
		for (size_t i = 0; i < json_array_size(element); i++) {
			json_t *format = json_array_get(element, i);
			formats.append_single(parseReleaseFormat(format));
		}
	}
}

bool remove_chars(char c) {
	return !((c >= '0' && c <= '9') || c == '-');
}

struct ptp_nfo {

	pfc::string8 dctn;
	pfc::string8 vol_preffix;
	pfc::string8 trk_postfix;
	pfc::string8 subtrk_postfix;
	pfc::string8 hidden_postfix;

	bool bvol_alpha;
	bool btrk_alpha;
	bool bmod_subtrack_track_is_disc;

	pfc::string8 format;
	pfc::string8 altformat;
	size_t format_qty;
	size_t ql_format_qty;

	bool bformat_incl_vol;
	bool bformat_incl_sideb;
	pfc::string8 side_trk_postfix;

	ReleaseFormat_ptr p_rf;

	ptp_nfo() {

		bvol_alpha = false;
		btrk_alpha = false;

		bmod_subtrack_track_is_disc = false;

		format_qty = 0;
		ql_format_qty = ~0;

		bformat_incl_vol = false;
		bformat_incl_sideb = false;

	}
};

struct trk_name_nfo {

	/*const*/ pfc::string8 dctn;

	pfc::string8 tn_nosub;
	pfc::string8 tn_nosub_chopped;
	pfc::string8 tn_std;

	bool isIndex;

	trk_name_nfo(const pfc::string8 dctn, bool isIndex): dctn(dctn), isIndex(isIndex) {
		//..
	}

};

// 1..N in/out params
// second are start/end release format positions

std::pair<ReleaseFormat_ptr, LPARAM> get_format_name(pfc::array_t<ReleaseFormat_ptr>* formats, size_t clay_pos) {

	std::pair<ReleaseFormat_ptr, size_t> no_res(NULL, NULL);

	if (clay_pos == ~0) return no_res;

	//todo: sec boxes
	const ReleaseFormat* wf = (*formats)[0].get();

	size_t boxset_offset = static_cast<size_t>(wf->is_box());
	size_t cDisc = 0;

	for (size_t w = 0; w < formats->get_count(); w++) {

		const ReleaseFormat_ptr wf = (*formats)[w];
		auto qty = atoi(wf->get_quantity().print().c_str());

		cDisc += qty;

		if (clay_pos + boxset_offset <= cDisc) {
			cDisc -= boxset_offset;
			LPARAM range = MAKELPARAM(cDisc - qty + 1, cDisc);
			return std::pair((*formats)[w], range);
		}
	}
	return no_res;
}

//todo: depricate
bool get_format_info(const pfc::string8 dc_track, const std::pair<ReleaseFormat_ptr, size_t> format_nfo_curr, const std::pair<ReleaseFormat_ptr, size_t> format_nfo_next,
	int curr_disc_number, ptp_nfo& ptpos, pfc::string8& chop, bool& bjump, size_t& jump_disc) {

	bjump = false;
	jump_disc = ~0;

	if (format_nfo_next.first) {

		pfc::string8 name = format_nfo_curr.first->get_name().get_value();
		ptpos.altformat = name;
		bool bname = dc_track.has_prefix(name);

		if (bname) {
			if (format_nfo_curr.first == ptpos.p_rf) {
				if (ptpos.ql_format_qty != ~0) {
					chop = name;
				}
			}
			else {
				chop = name;
				ptpos.format = chop;
				ptpos.p_rf = format_nfo_curr.first;
				ptpos.ql_format_qty = atoi(ptpos.p_rf->qty);
				ptpos.bformat_incl_vol = format_nfo_curr.first->format_incl_vol;
			}
		}
		else {

			for (size_t wdesc = 0; wdesc < format_nfo_curr.first->descriptions.get_count(); wdesc++) {

				pfc::string8 desc = format_nfo_curr.first->descriptions[wdesc];

				if (dc_track.has_prefix(desc)) {
					if (format_nfo_curr.first == ptpos.p_rf) {
						if (ptpos.ql_format_qty != ~0) {
							chop = desc;
							break;
						}
					}
					else {
						chop = desc;
						ptpos.format = chop;
						ptpos.p_rf = format_nfo_curr.first;
						ptpos.ql_format_qty = atoi(ptpos.p_rf->qty);
						ptpos.bformat_incl_vol = format_nfo_curr.first->format_incl_vol;
						break;
					}
				}
			}
		}
		//todo: trace
		jump_disc = curr_disc_number;
	}

	if (!chop.get_length()) {

		//TRY NEXT FORMAT...

		if (format_nfo_next.first) {

			pfc::string8 name = format_nfo_next.first->get_name().get_value();
			ptpos.altformat = name;
			bool bname = dc_track.has_prefix(name);

			if (bname) {
				if (format_nfo_next.first == ptpos.p_rf) {
					if (ptpos.ql_format_qty != ~0) {
						chop = name;
					}
				}
				else {
					chop = name;
					ptpos.format = chop;
					ptpos.p_rf = format_nfo_next.first;
					ptpos.ql_format_qty = atoi(ptpos.p_rf->qty);
					ptpos.bformat_incl_vol = format_nfo_next.first->format_incl_vol;
				}
			}
			else {

				for (size_t wdesc = 0; wdesc < format_nfo_next.first->descriptions.get_count(); wdesc++) {

					pfc::string8 desc = format_nfo_next.first->descriptions[wdesc];

					if (dc_track.has_prefix(desc)) {
						if (format_nfo_next.first == ptpos.p_rf) {
							if (ptpos.ql_format_qty != ~0) {
								{
									chop = desc;
									break;
								}
							}
							else {

								chop = desc;
								ptpos.format = chop;
								ptpos.p_rf = format_nfo_next.first;
								ptpos.ql_format_qty = atoi(ptpos.p_rf->qty);
								ptpos.bformat_incl_vol = format_nfo_next.first->format_incl_vol;
								break;
							}
						}
					}
				}
			}

			if (chop.get_length()) {
				bjump = true;
				jump_disc = LOWORD(format_nfo_next.second);
			}
		}
	}

	return chop.get_length();
}

// todo: depricate
// 1..N in/out params
// second are start/end release format positions

ReleaseFormat_ptr get_matching_format_info(const pfc::array_t<ReleaseFormat_ptr>* formats, pfc::string8 dc_track, ptp_nfo& ptpos, pfc::string8& chop) {

	if (is_number(dc_track.c_str())) return nullptr;

	const ReleaseFormat* wf = (*formats)[0].get();

	size_t boxset_offset = static_cast<size_t>(wf->is_box());
	size_t cDisc = 0;

	for (size_t w = 0; w < formats->get_count(); w++) {

		const ReleaseFormat_ptr wf = (*formats)[w];
		auto qty = atoi(wf->get_quantity().print().c_str());

		if (dc_track.has_prefix(wf->get_name().get_cvalue())) {
			ptpos.format = chop = wf->get_name();
			ptpos.p_rf = wf;
			ptpos.ql_format_qty = atoi(ptpos.p_rf->qty);
			ptpos.bformat_incl_vol = wf->format_incl_vol;
			return wf;
		}
	}
	return nullptr;
}

void parseTrackPosition(ReleaseTrack_ptr& track, const pfc::array_t<ReleaseFormat_ptr>* formats, bool& bformats_unsorted,
	const std::pair<ReleaseFormat_ptr, size_t> format_nfo_curr, const std::pair<ReleaseFormat_ptr, size_t> format_nfo_next,
	ptp_nfo & ptpos, bool multi_format,int curr_vol_number, int next_track_number) {

	if (!(bool)curr_vol_number) {
		//first run
		curr_vol_number = 1;
	}

	//rewind-hidden track 0 not supported
	if (track->discogs_track_number.equals("-1")) {
		track->discogs_track_number = "1a";
	}
	else if (track->discogs_track_number.startsWith("0.")) {
		auto hidden_num = track->discogs_track_number.subString(2, 1);
		pfc::string8 hidden_alpha = "1";
		hidden_alpha.add_char(96 + atoi(hidden_num));
		track->discogs_track_number = hidden_alpha;
	}

	trk_name_nfo trk_name(track->discogs_track_number, track->title_subtrack.get_length());

	ptpos.dctn = trk_name.dctn;
	ptpos.bvol_alpha = false;
	ptpos.btrk_alpha = false;
	ptpos.hidden_postfix = "";
	ptpos.vol_preffix = "";
	ptpos.trk_postfix = "";
	ptpos.subtrk_postfix = "";

	ptpos.bformat_incl_vol = false;
	ptpos.bformat_incl_sideb = false;
	ptpos.side_trk_postfix = "";

	bool bjump = false;
	size_t jump_disc = ~0;
	pfc::string8 chop;

	//todo: unify next extract subtract
	// EXTRACT SUBTRACK 1
	size_t dotpos;
	if ((dotpos = trk_name.dctn.find_last('.')) < trk_name.dctn.get_length()) {

	bool dotpos_is_len = dotpos == trk_name.dctn.get_length() - 1;
	if (dotpos_is_len) {
		trk_name.dctn << std::to_string(next_track_number).c_str();
	}

	ptpos.subtrk_postfix = substr(trk_name.dctn, dotpos + 1);
	int dec = RomanToDecimal(ptpos.subtrk_postfix);
	if (dec) {
		ptpos.subtrk_postfix = std::to_string(dec).c_str();
	}

		trk_name.tn_nosub = substr(trk_name.dctn, 0, dotpos);
	}
	else {

		trk_name.tn_nosub = trk_name.dctn;
	}

	// QUERY FORMATS / DISC RANGES

	//todo: repricate
	if (!bformats_unsorted) {
		get_format_info(trk_name.dctn, format_nfo_curr, format_nfo_next, curr_vol_number,
			ptpos, chop, bjump, jump_disc);
		if (!chop.get_length()) {
			//format will not help match
			bformats_unsorted = true;
		}
	}
	//todo: repricate
	if (bformats_unsorted) {
		auto f = get_matching_format_info(formats, trk_name.dctn, ptpos, chop);
	}

	// PREPLACE DISC NUMBERS & FORMAT PREFIXES
	size_t choplen = chop.get_length();

	pfc::string8 mod_dc_tn = trk_name.dctn;

	if (choplen) {

		ptpos.format = chop;

		//remove media format preffix
		mod_dc_tn = substr(mod_dc_tn, choplen);

		size_t min_pos = mod_dc_tn.find_first('-');
		if (min_pos != SIZE_MAX) {
			if (min_pos == 0) {
				jump_disc = curr_vol_number;
				pfc::string8 tmp_trk(mod_dc_tn.subString(1));
				if (is_number(tmp_trk.c_str())) {
					if (atoi(tmp_trk) < next_track_number) {
						jump_disc = curr_vol_number + 1;
					}

					ptpos.vol_preffix = PFC_string_formatter() << jump_disc;

				}
				bjump = false;
			}
			else {
				pfc::string8 tmp(mod_dc_tn.subString(0, min_pos));
				if (is_number(tmp.c_str())) {
					jump_disc = atoi(tmp);
					ptpos.vol_preffix = PFC_string_formatter() << jump_disc;
					bjump = false;
				}
				else {
					//..
				}
			}
			mod_dc_tn = PFC_string_formatter() <<  jump_disc << "-" << substr(trk_name.dctn, choplen + min_pos + 1);
		}
	}

	// replace disc numbers

	if (bjump) {

		jump_disc = LOWORD(format_nfo_next.second);

		if (!ptpos.bformat_incl_vol) {

			size_t track_minus = mod_dc_tn.find_first('-');
			if (track_minus  < mod_dc_tn.get_length()) {

				mod_dc_tn = PFC_string_formatter() << jump_disc << substr(mod_dc_tn, track_minus);
			}
			else {

				mod_dc_tn = PFC_string_formatter() << jump_disc << "-" << mod_dc_tn;
			}
		}
	}

	trk_name.tn_nosub_chopped = mod_dc_tn;

	// TODO: remove prev extract !!!
	// EXTRACT SUBTRACK 2
	size_t subdotpos;

	if ((subdotpos = mod_dc_tn.find_last('.')) < mod_dc_tn.get_length() -1) {

		ptpos.subtrk_postfix = substr(mod_dc_tn, subdotpos + 1);
		int dec = RomanToDecimal(ptpos.subtrk_postfix);
		if (dec) {
			ptpos.subtrk_postfix = std::to_string(dec).c_str();
		}
		ptpos.trk_postfix = substr(mod_dc_tn, 0, subdotpos);

		if (!trk_name.isIndex) {

			trk_name.tn_nosub_chopped = ptpos.trk_postfix;
		}
		else {

			trk_name.tn_nosub_chopped = trk_name.tn_nosub;
		}
	}
	else {

		if (!trk_name.tn_nosub_chopped.get_length()) {
			//rev: trace
			trk_name.tn_nosub_chopped = trk_name.dctn;
		}
	}

	// fix v1.0.22.2.beta.1 r:364554 first disc track '1-1', 2nd disc '2.1' (disc dot track)
	// todo: will not work for intermediate 'disc.track' if the last disc has 'disk-track'
	bool bminuspos = mod_dc_tn.find_last('-') < mod_dc_tn.get_length() - 1;
	if (ptpos.bmod_subtrack_track_is_disc && !bminuspos) {
		//fix entry typos 30,3 -> 30.3
		if (subdotpos > mod_dc_tn.get_length() && mod_dc_tn.get_length() > 2 && !pfc::string_is_numeric(mod_dc_tn)) {
			std::string s = mod_dc_tn.get_ptr();
			std::replace_if(s.begin(), s.end(), [](unsigned char c)
				{
					return !std::isdigit(c);
				}, '.');
			mod_dc_tn = s.c_str();
		}
		ptpos.subtrk_postfix = substr(mod_dc_tn, subdotpos + 1);
		ptpos.trk_postfix = substr(mod_dc_tn, 0, subdotpos);

		size_t trk_subdotpos = ptpos.trk_postfix.find_last('.');

		if (subdotpos != trk_subdotpos && trk_subdotpos < ptpos.trk_postfix.get_length() - 1) {
			ptpos.vol_preffix = trim(substr(ptpos.trk_postfix, 0, trk_subdotpos));
			ptpos.trk_postfix = trim(substr(ptpos.trk_postfix, trk_subdotpos+1));
		}
		else {
		
			ptpos.vol_preffix = ptpos.trk_postfix;
			ptpos.trk_postfix = ptpos.subtrk_postfix;
			ptpos.subtrk_postfix = "0";
		}

		std::string tmpPre = ptpos.vol_preffix.get_ptr();
		std::string tmpPost = ptpos.trk_postfix.get_ptr();
		std::string tmpSub = ptpos.subtrk_postfix.get_ptr();
		tmpPre.erase(0, tmpPre.find_first_not_of('0'));
		tmpPost.erase(0, tmpPost.find_first_not_of('0'));
		tmpSub.erase(0, tmpSub.find_first_not_of('0'));
		ptpos.vol_preffix = tmpPre.c_str();
		ptpos.trk_postfix = tmpPost.c_str();
		ptpos.subtrk_postfix = tmpSub.c_str();

		if (ptpos.vol_preffix.get_length())
			ptpos.bvol_alpha = !pfc::string_is_numeric(ptpos.vol_preffix);
		if (ptpos.trk_postfix.get_length())
			ptpos.btrk_alpha = !pfc::string_is_numeric(ptpos.trk_postfix);
		//..
		return;
		//..
	}


	// FINISH PARSE TRACK...

	trk_name.tn_std = "";

	size_t _trk_div_pos = ~0;
	pfc::string8 tmp_pre = "";
	pfc::string8 tmp_post = "";

	_trk_div_pos = trk_name.tn_nosub_chopped.find_last("-");

	if (_trk_div_pos == ~0) {

		if (pfc::string_is_numeric(trk_name.tn_nosub) && (pfc::string_is_numeric(trk_name.tn_nosub_chopped))) {

			tmp_pre << curr_vol_number;

			// std
			trk_name.tn_std = PFC_string_formatter() << curr_vol_number << "-" << trk_name.tn_nosub;
			_trk_div_pos = trk_name.tn_nosub_chopped.find_last("-");
		}
		else {

			// A, B, C... -> A1, B1, C1...
			if (trk_name.tn_nosub_chopped.get_length() == 1) {
				tmp_pre = trk_name.tn_nosub_chopped;
				tmp_post = "1";
				trk_name.tn_nosub_chopped = PFC_string_formatter() << tmp_pre << tmp_post;
				_trk_div_pos = ~0;
			}
		}
	}

	if (_trk_div_pos == 0) {
		//missing vol ej -2
		trk_name.tn_nosub_chopped = PFC_string_formatter() << "0-" << trk_name.tn_nosub_chopped;
		_trk_div_pos = trk_name.tn_nosub_chopped.find_last("-");
	}

	if (_trk_div_pos == trk_name.tn_nosub_chopped.get_length() - 1) {
		//missing track ej. 2-
		trk_name.tn_nosub_chopped << next_track_number;
		_trk_div_pos = trk_name.tn_nosub_chopped.find_last("-");
	}

	if (_trk_div_pos != ~0) {
		tmp_pre = trim(substr(trk_name.tn_nosub_chopped, 0, _trk_div_pos));
		tmp_post = trim(substr(trk_name.tn_nosub_chopped, _trk_div_pos + 1));

		int dec = RomanToDecimal(tmp_post);
		if (dec) {
			ptpos.subtrk_postfix = std::to_string(dec).c_str();
			tmp_post = ptpos.subtrk_postfix;
		}

		// VOL_ALPHA AND TRK_ALPHA 

		ptpos.bvol_alpha = !pfc::string_is_numeric(tmp_pre);
		ptpos.btrk_alpha = !pfc::string_is_numeric(tmp_post);

		trk_name.tn_std = PFC_string_formatter() << tmp_pre << "-" << tmp_post;
	}
	else {

		// CAS/LP TO DISC AND SIDES
		ptpos.bformat_incl_vol = true;

		pfc::string8 strDummy;
		//min 2 chars, pos >= 1
		size_t last_num_pos = get_last_num(trk_name.tn_nosub_chopped, 2, 1, strDummy);

		if (last_num_pos != ~0) {

			tmp_pre = substr(trk_name.tn_nosub_chopped, 0, last_num_pos);
			tmp_post = substr(trk_name.tn_nosub_chopped, last_num_pos);

			// A1 B1a C3 or x, y
			auto ndx = (int)tmp_pre.toLower()[0] - 97;
			if (tmp_pre.get_length() == 1) {
				//fix of fix xy
				if (((ndx + 1) / (double)2) - curr_vol_number > 1) {
					//x, y
					tmp_post = PFC_string_formatter() << next_track_number;
					tmp_pre = PFC_string_formatter() << curr_vol_number;
				}
				else {
					if (ndx >= 0 && ndx < 26) {

						ptpos.side_trk_postfix = tmp_post;

						if (ndx % 2) {
							//odd
							auto album = (ndx + 1) / (double)2 /*+ 0.5*/;
							tmp_pre = std::to_string((int)album).c_str();
							//SIDE B
							ptpos.bformat_incl_sideb = true;
						}
						else {
							auto album = (ndx + 1) / (double)2 + 0.5;
							//even
							tmp_pre = std::to_string((int)album).c_str();
						}
						tmp_post = std::to_string(next_track_number).c_str();
					} // end album letters
					else {
						// other chars
						tmp_pre = std::to_string(curr_vol_number).c_str();
					}
				}
			}
		}
		else {
			//'1a' instead of A1
			tmp_pre = std::to_string(curr_vol_number).c_str();
			tmp_post = trk_name.tn_nosub_chopped;
		}

		// std
		trk_name.tn_std = PFC_string_formatter() << tmp_pre << "-" << tmp_post;
	}

	if (!trk_name.tn_std.get_length()) {
			// std
			trk_name.tn_std = PFC_string_formatter() << tmp_pre << "-" << tmp_post;
	}

	if (!pfc::string_is_numeric(tmp_pre)) {

		pfc::string8 last_num;
		size_t last_num_pos = get_last_num(tmp_pre, 2, 0, last_num);
		if (last_num_pos != ~0) {
			tmp_pre = last_num;
		}
		else {
			tmp_pre = std::to_string(curr_vol_number).c_str();
		}
	}

	// CHECK TRACK ALPHA/HIDDEN / MOD TMP_POST

	ptpos.btrk_alpha = !pfc::string_is_numeric(tmp_post);

	if (!pfc::string_is_numeric(tmp_post)) {

		pfc::string8 last_num;
		size_t last_num_position = get_last_num(tmp_post, 2, 0, last_num); //could be 1a or A1 ?

		if (last_num_position != ~0) {

			pfc::string8 halpha = substr(tmp_post, last_num.get_length());

			// mod TEMP_POST

			tmp_post = last_num;
			trk_name.tn_std = PFC_string_formatter() << tmp_pre << "-" << tmp_post;

			ptpos.btrk_alpha = (bool)halpha.get_length();

			if (ptpos.btrk_alpha) {

				ptpos.hidden_postfix = halpha;
			}
		}

	}


	// check leftovers

	if (pfc::string_is_numeric(tmp_pre) && pfc::string_is_numeric(tmp_post)) {

		if (std::stoi(tmp_pre.get_ptr()) < curr_vol_number) {
			ptpos.vol_preffix = std::to_string(curr_vol_number).c_str();
			ptpos.trk_postfix = tmp_post;
		}

		if (std::stoi(tmp_post.get_ptr()) >= next_track_number || trk_name.isIndex) {
			ptpos.vol_preffix = tmp_pre;
			ptpos.trk_postfix = tmp_post;
		}
		else {
			if (!ptpos.btrk_alpha && !bjump) {
				ptpos.vol_preffix = tmp_pre;
				ptpos.trk_postfix = tmp_post;
			}
			else {
				if (ptpos.btrk_alpha) {
					ptpos.trk_postfix = tmp_post;
					ptpos.vol_preffix = tmp_pre;
				}
				else {
					ptpos.vol_preffix = tmp_pre;
					ptpos.trk_postfix = tmp_post;
				}
			}
		}
	}

	else {

		// ALPHA tmp_pre or tmp_post leftovers

		pfc::string8 mytrack = trk_name.tn_std;

		if (mytrack.get_length()) {

				ptpos.btrk_alpha = mytrack.get_length() && (mytrack[0] < -1 || mytrack[0] > 255 || isalpha(mytrack[0])); // unicode...

			if (!pfc::string_is_numeric(mytrack)) {

				std::string lastnum;
				std::string str_mod = mytrack.c_str();

				std::regex regex_v("[\\d]+");
				std::sregex_iterator begin = std::sregex_iterator(str_mod.begin(), str_mod.end(), regex_v);
				for (std::sregex_iterator i = begin; i != std::sregex_iterator(); i++) {
					lastnum = i->str();
				}

				if (lastnum.size()) {
					ptpos.vol_preffix = tmp_pre;
					ptpos.trk_postfix = lastnum.c_str();
				}
				else {
					ptpos.vol_preffix = tmp_pre;
					ptpos.trk_postfix = tmp_post;
				}
			}
			else {
				// 1,2,3... or whatever
				ptpos.trk_postfix = mytrack;
			}
		}
	}
}

	bool test_track_is_vol(int total_discs, pfc::string8 last_track_discogs_position) {
		bool bres = false;
		size_t dotpos;
		if ((dotpos = last_track_discogs_position.find_first('.')) < last_track_discogs_position.get_length()) {
			bres = substr(last_track_discogs_position, 0, dotpos).equals(std::to_string(total_discs).c_str());
		}
		return bres;
	}

// PARSE ALL TRACK POSITIONS

bool parseAllTrackPositions(pfc::array_t<ReleaseTrack_ptr>& intermediate_tracks, HasTracklist* release) {

	if (!intermediate_tracks.get_count() || !release) {
		return false;
	}

	//counters
	int ctrack_number = 1;
	int cdisc_track_number = 1;
	int cdisc_number = 0;

	bool bsubtrack = false;
	bool bfake_subtrack_header_changed = false;

	bool bmerge_subtracks = CONF.cache_offline_cache_flag & ol::CacheFlags::MERGE_SUBTRACKS;
	bool bmerge_tracks_to_header = CONF.cache_offline_cache_flag & ol::CacheFlags::MERGE_TRACKS_TO_HEADER;
	bool bmerge_hidden = !CONF.parse_hidden_as_regular;

	bool last_hidden = false;
	bool last_ssubtrack = false;
	bool last_fake_subtrack = false;

	pfc::string8 merging_track = "";
	pfc::string8 last_heading = "";

	bool last_vol_alpha = false;

	pfc::string8 last_vol_preffix = "0";
	pfc::string8 last_trk_postfix = "0";

	ReleaseDisc_ptr disc;
	ptp_nfo parse_nfo;

	Release* ptmp = static_cast<Release*>(release);
	pfc::array_t<ReleaseFormat_ptr>* formats = &ptmp->formats;

	std::pair<ReleaseFormat_ptr, LPARAM> format_nfo_current = std::pair(nullptr, 0);
	std::pair<ReleaseFormat_ptr, LPARAM> format_nfo_next = std::pair(nullptr, 0);


	bool formats_unsorted = false;
	if (intermediate_tracks.get_size() > 1) {
		parse_nfo.bmod_subtrack_track_is_disc =
				test_track_is_vol(release->discogs_total_discs, intermediate_tracks[intermediate_tracks.get_size() -1]->discogs_track_number);
	}

	try {

	// WALK TRACKS
	for (size_t i = 0; i < intermediate_tracks.get_size(); i++) {

		auto current_disc = (std::max)((int)release->discs.get_count(), 1);

		if (current_disc > release->discogs_total_discs) {
			current_disc = release->discogs_total_discs;
		}

		// query format refs

		format_nfo_current = get_format_name(formats, (std::min)(release->discogs_total_discs, (int)current_disc));
		if (format_nfo_current.first) {
			format_nfo_next = get_format_name(formats, (std::min)(release->discogs_total_discs, (int)HIWORD(format_nfo_current.second) + 1));
		}

		ReleaseTrack_ptr& track = intermediate_tracks[i];

		//
		// PARSE TRACK POSITION
		//

		parseTrackPosition(track, formats, formats_unsorted,
				format_nfo_current,  format_nfo_next,
				parse_nfo, ptmp->formats.get_count(),
				release->discs.get_count(),
			cdisc_track_number);

		//

		bool bdiscogs_track_number_std = (atoi(parse_nfo.vol_preffix) > 0 && atoi(parse_nfo.trk_postfix) > 0);
		bool bhidden = (bool)parse_nfo.hidden_postfix.get_length();
		bhidden |= parse_nfo.subtrk_postfix.get_length() && !(track->/*title_index*/title_subtrack.get_length() || track->title_index.get_length() || track->title_heading.get_length());
		if (bhidden && !parse_nfo.hidden_postfix.get_length()) parse_nfo.hidden_postfix = parse_nfo.subtrk_postfix;
		bhidden |= parse_nfo.subtrk_postfix.get_length() && !pfc::string_is_numeric(parse_nfo.subtrk_postfix);
		if (bhidden && !parse_nfo.hidden_postfix.get_length()) parse_nfo.hidden_postfix = parse_nfo.subtrk_postfix;

		// SUBTRACK STATUS

		//todo: rev hidden subtracks?

		pfc::string8 curr_heading = track->title_heading;
		curr_heading << track->title_index;

		bsubtrack = !bhidden;
		bsubtrack &= (bool)track->title_subtrack.get_length();
		bool bheading_changed = last_heading.get_length() && !curr_heading.equals(last_heading);

		//todo: headed hidden track, subtrack within track header,
		//      subtrack index right after track header,...
		bool bfake_subtrack = !bhidden && !bsubtrack;

		bfake_subtrack &= (bool)track->title_heading.get_length() && (!track->discogs_duration_raw.get_length() && parse_nfo.subtrk_postfix.get_length());
		if (bfake_subtrack) {

			bfake_subtrack_header_changed = last_heading.get_length() && !last_heading.equals(curr_heading);

		}
		else {
			//..
		}

		if (!bsubtrack && !bfake_subtrack) {
			if (track->discogs_duration_raw.get_length()) {

				if (parse_nfo.subtrk_postfix.get_length()) {
					//..
				}
			}
		}

		bool last_merge_off = last_ssubtrack && !bmerge_subtracks;
		last_merge_off |= last_fake_subtrack && !bmerge_tracks_to_header;
		last_merge_off |= last_hidden && !bmerge_hidden;

		bool last_header_changed = last_ssubtrack && bheading_changed;
		last_header_changed |= last_fake_subtrack && bfake_subtrack_header_changed;

		pfc::string8 without_hidd_postfix = substr(track->discogs_track_number, 0, track->discogs_track_number.get_length() - parse_nfo.hidden_postfix.get_length());
		bool end_merging = false;
		if (merging_track.get_length()) {
			end_merging = !without_hidd_postfix.equals(substr(merging_track, 0, without_hidd_postfix.get_length()));
		}

		if (end_merging || (((bhidden && bmerge_hidden) || (bsubtrack && bmerge_subtracks) || (bfake_subtrack && bmerge_tracks_to_header)) && last_header_changed)
			|| (!last_merge_off && (!bhidden && !bsubtrack && !bfake_subtrack) && (last_header_changed))) {
			//resume disc track counter (1/2)
			++cdisc_track_number;
		}

		merging_track = "";

		bool bAddDisk = last_vol_preffix != parse_nfo.vol_preffix || last_vol_alpha != parse_nfo.bvol_alpha;

		if (bAddDisk) {
			// + NEW DISC
			disc = std::make_shared<ReleaseDisc>();
			release->discs.append_single(disc);

			// consume qty
			--parse_nfo.ql_format_qty;

			// get format
			pfc::string8 strFormat;
			if (parse_nfo.p_rf) {
				strFormat = parse_nfo.p_rf->get_name();
			}
			else {
				strFormat = parse_nfo.altformat.get_length() ? parse_nfo.altformat : parse_nfo.format;
			}

			strFormat = strFormat.toUpper();

			if (strFormat.get_length() > 4) {
				strFormat = substr(strFormat, 0, 3);
			}

			// reset disc track number
			// increment disc counter
			// set disc attributes

			cdisc_track_number = 1;
			disc->disc_number = ++cdisc_number;
			disc->format = strFormat/*parse_nfo.format.get_length() ? parse_nfo.format : parse_nfo.altformat.c_str()*/;

			parse_nfo.trk_postfix = "1";

			// reset last subtrack, hidden & vol alpha

			last_ssubtrack = last_fake_subtrack = last_hidden = false;
			last_vol_alpha = parse_nfo.bvol_alpha;

		} //new disc

		// BUILD TRACK

		if (bdiscogs_track_number_std && !track->title_subtrack.get_length() && !track->title_heading) {

			//may overwrite counters
			disc->disc_number = atoi(parse_nfo.vol_preffix);

			track->disc_track_number = atoi(parse_nfo.trk_postfix);
			track->disc_track_two_sided = parse_nfo.bformat_incl_sideb;
			track->disc_track_side = atoi(parse_nfo.side_trk_postfix);

				track->track_number = ctrack_number;

				cdisc_number = disc->disc_number;
				cdisc_track_number = track->disc_track_number;
			}

			else {

			track->disc_track_number = cdisc_track_number;
			track->disc_track_two_sided = parse_nfo.bformat_incl_sideb;
			track->disc_track_side = atoi(parse_nfo.side_trk_postfix);

				track->track_number = ctrack_number;
			}


		// ROUTE TRACK

		bool bdisc_empty = !(bool)disc->tracks.get_size();

		bool skip_first_alpha_hidden = (bhidden && last_trk_postfix == parse_nfo.trk_postfix) && bmerge_hidden;

		if (!skip_first_alpha_hidden && (bdisc_empty || !bhidden || !last_hidden || (bhidden && !bmerge_hidden) || last_trk_postfix != parse_nfo.trk_postfix)) {

			// new track precondition 2
			bool merge_off = bsubtrack && !bmerge_subtracks;
			merge_off |= bfake_subtrack && !bmerge_tracks_to_header;
			merge_off |= bhidden && !bmerge_hidden;
			bool last_changed = bsubtrack && !(last_ssubtrack && bmerge_subtracks);
			last_changed |= bfake_subtrack && !(last_fake_subtrack && bmerge_tracks_to_header);
			last_changed |= bhidden && !(last_hidden && bmerge_hidden);
			bool headed_changed = bsubtrack && bheading_changed;
			headed_changed |= bfake_subtrack && bfake_subtrack_header_changed;
			headed_changed |= bhidden && (last_heading.get_length() && !last_heading.equals(track->title_heading));

			if ((!bhidden && !bsubtrack && !bfake_subtrack) || (merge_off || last_changed || headed_changed)) {

				// ADD NEW TRACK

				disc->tracks.append_single(track);

				ctrack_number++;

				bool can_fake_duration = bfake_subtrack && bfake_subtrack_header_changed;
				bool can_advance_subtrack = bsubtrack && !bmerge_subtracks;
				bool can_advance_continuous_subtracks = (bsubtrack || last_ssubtrack) && !bmerge_subtracks;
				bool can_advance_continuous_fake_subtracks = (bfake_subtrack || last_fake_subtrack) && !bmerge_tracks_to_header; 
				bool can_advance_continuous_hidden = (bhidden || last_hidden) && !bmerge_hidden;
				bool bdisc_track_ptnfo_match = atoi(parse_nfo.vol_preffix) == cdisc_number && atoi(parse_nfo.trk_postfix) == cdisc_track_number;

				if (!merge_off && ((bsubtrack || bfake_subtrack) || can_fake_duration)) {

					if (last_vol_preffix != parse_nfo.vol_preffix || last_trk_postfix != parse_nfo.trk_postfix) {

						// assign subtrack discogs durations to first subtrack
						bool not_both = (bool)track->discogs_indextrack_duration_raw.get_length() != (bool)track->title_heading_duration.get_length();
						if (!track->discogs_duration_raw.get_length() || not_both) {
							track->discogs_duration_seconds = track->discogs_indextrack_duration_seconds;
							track->discogs_duration_raw = track->discogs_indextrack_duration_raw;

							//nested heading->subtrack not supported
							if (track->title_heading_duration.get_length() && !track->discogs_indextrack_duration_raw.get_length()) {
								track->discogs_duration_raw = track->title_heading_duration;
							}
						}
					}
				}

				//resume disc track counter (2/2)
				if (!bhidden && !bsubtrack && !bfake_subtrack) {
					// +++
					cdisc_track_number++;
					// +++
				}
				else if (!parse_nfo.btrk_alpha && can_advance_subtrack) {

					// +++
					cdisc_track_number++;
					// +++

					}
				else {

					if ((can_advance_continuous_subtracks)
						|| (can_advance_continuous_fake_subtracks)
						|| can_advance_continuous_hidden) {


						// +++
						cdisc_track_number++;
						// +++
					}
				}
			}
		else {

			// ROUTE MERGE TITLES

			// merge subrack precond 1
			if ((last_ssubtrack && bmerge_subtracks) || (last_fake_subtrack && bmerge_tracks_to_header)) {
				//other cases not needing insertion (merging bsubtrack titles into container title)
					// merge subrack precond 2
					if (disc->tracks.get_count()) {

						// APPEND TITLES: SUBTRACK

						if (bfake_subtrack) {

							disc->tracks[disc->tracks.get_count() - 1]->title << " + " << track->title;
						}
						else {
							disc->tracks[disc->tracks.get_count() - 1]->title << " + " << track->title_subtrack;
	
							//was empty
							if (!disc->tracks[disc->tracks.get_count() - 1]->subtrack_durations.get_count()) {
								pfc::string8 first_duration = disc->tracks[disc->tracks.get_count() - 1]->discogs_duration_raw;
								disc->tracks[disc->tracks.get_count() - 1]->subtrack_durations.append_single(first_duration);
							}
							//current
							if (track->discogs_duration_raw.get_length()) {
								disc->tracks[disc->tracks.get_count() - 1]->subtrack_durations.append_single(track->discogs_duration_raw/*pfc::string8("99:99")*/);
							}
							//later remove last
						}
					}
				}
			}
		}
		else {

			// discard track...

			if ((bhidden && last_hidden) || skip_first_alpha_hidden) {

				// last_hidden (n > 1)
				// accumulate hidden duration
				disc->tracks[disc->tracks.get_size() - 1]->discogs_hidden_duration_seconds += track->discogs_duration_seconds;

				//ignore silence, otherwise move to track's hiddens
				if (!track->title.equals("(silence)")) {

					disc->tracks[disc->tracks.get_size() - 1]->hidden_tracks.append_single(track);

					merging_track = track->discogs_track_number;

					if (!CONF.parse_hidden_as_regular && CONF.parse_hidden_merge_titles) {

						// APPEND TITLES: HIDDEN

						disc->tracks[disc->tracks.get_size() - 1]->title << " + " << trim(track->title);
					}
				}
			}
		} // end discard track

		last_vol_preffix = parse_nfo.vol_preffix;
		last_trk_postfix = parse_nfo.trk_postfix;

		if (last_ssubtrack && !bsubtrack) {
			//remove last index
			size_t prev_size = disc->tracks[disc->tracks.get_count() - 1]->subtrack_durations.get_count();
			if (prev_size) {
				disc->tracks[disc->tracks.get_count() - 1]->subtrack_durations.resize(prev_size - 1);
			}
		}
	
		last_ssubtrack = bsubtrack;
	
		last_heading = track->title_heading;
		last_heading << track->title_index;
		last_hidden = bhidden;

	} // track loop
}
catch (...) {
		foo_discogs_exception ex;
		ex << "Unknown error parsing track positions.";
		throw ex;
}

	return true;
}

void Discogs::parseReleaseTrack(json_t* element, pfc::array_t<ReleaseTrack_ptr>& tracks, 
		const pfc::string8 heading, const pfc::string8 heading_duration,
		ReleaseTrack_ptr* index,HasArtists* has_artists, HasIndexes* has_indexes,
		size_t& last_disc_number, size_t& last_track_number) {

	assert_is_object(element);

	ReleaseTrack_ptr track(new ReleaseTrack());

	track->title_heading = heading;
	track->title_heading_duration = heading_duration;

	track->title = JSONAttributeString(element, "title");
	pfc::string8 duration = JSONAttributeString(element, "duration");

	track->discogs_track_number = JSONAttributeString(element, "position");

	if (index != nullptr) {

		// BUILD INDEX SUBTRACK CONTAINER

		track->title_index = (*index)->title;
		track->title_subtrack = track->title;

		//USE title format in tag mapping instead
		//pfc::string8 temp = (*index)->title;
		//temp << " - " << track->title;
		//track->title = temp;

		track->credits = (*index)->credits;
		track->catcredits_gxc = (*index)->catcredits_gxc;
		track->catcredits_gxp = (*index)->catcredits_gxp;

		auto& p_ndx = has_indexes->indexes[has_indexes->indexes.get_count() - 1];
		pfc::string8 std_trk_pos = track->discogs_track_number;

		if (!p_ndx->dc_track_first.length()) {
			p_ndx->dc_track_first = std_trk_pos;
		}
		p_ndx->dc_track_last = std_trk_pos;

		track->index_ptr = p_ndx;

		track->discogs_indextrack_duration_raw = (*index)->discogs_duration_raw;
		track->discogs_indextrack_duration_seconds = (*index)->discogs_duration_seconds;

		//tracks[tracks.get_count()-1]->subtrack_durations.append_single(pfc::string8("99:99"));
	}
	else {

		track->title_index = "";
		track->title_subtrack = "";
	}

	if (CONF.skip_video_tracks) {
		pfc::string8 lower_position = lowercase(track->discogs_track_number);
		if (lower_position.find_first("video") != pfc::infinite_size || lower_position.find_first("dvd") != pfc::infinite_size) {
			return;
		}
	}

	json_t* artists = json_object_get(element, "artists");
	if (artists) {
		parseReleaseArtists(artists, track->artists);
	}
	else if (has_artists) {
		for (size_t i = 0; i < has_artists->artists.get_count(); i++) {
			track->artists.append_single_val(has_artists->artists[i]);
		}
	}

	json_t* extraartists = json_object_get(element, "extraartists");
	if (extraartists) {
		parseReleaseCredits(extraartists, track->credits, nullptr);
	}

	int duration_seconds;
	if (duration.get_length() != 0) {
		duration_seconds = duration_in_seconds(duration);
		if (!duration_seconds) {
			log_msg(PFC_string_formatter() << "Error parsing track duration: " << duration);
		}
		track->discogs_duration_raw = duration;
		track->discogs_duration_seconds = duration_seconds;
	}

	pfc::string8 type = JSONAttributeString(element, "type_");

	if (STR_EQUAL(type, "track")) {

		if (track->discogs_track_number.get_length() || track->title.get_length()) {

			//APPEND TRACK
			tracks.append_single(move(track));
			//first track could be a subtrack?
			if (!index || !last_track_number) {
				++last_track_number;
			}
		}
	}
	else if (STR_EQUAL(type, "index")) {

		// may contain subtracks (bsubtrack parse loop)

		ReleaseIndexes_ptr index_p = std::make_shared<ReleaseIndexes>();

		index_p->title = JSONAttributeString(element, "title");
		index_p->duration = JSONAttributeString(element, "duration");
		pfc::string8 str_pos = JSONAttributeString(element, "position");

		has_indexes->indexes.add_item(std::move(index_p));
		track->title = JSONAttributeString(element, "title");

		json_t* sub_tracks = json_object_get(element, "sub_tracks");

		// PARSE SUBTRACKS

		if (json_is_array(sub_tracks)) {
			for (size_t i = 0; i < json_array_size(sub_tracks); i++) {
				json_t* js_sub_track = json_array_get(sub_tracks, i);
				//recursion
				parseReleaseTrack(js_sub_track, tracks, track->title_heading, track->title_heading_duration,
						/*index*/&track, has_artists, has_indexes, last_disc_number, last_track_number);
			}
		}
	}
}

void Discogs::parseAllReleaseTracks(json_t* jsTracklist, bool isRelease, const pfc::array_t<ReleaseFormat_ptr>& formats, HasTracklist* has_tracklist, HasArtists* has_artists, HasIndexes* has_indexes) {

	assert_is_array(jsTracklist);

	pfc::array_t<ReleaseTrack_ptr> intermediate_tracks;

	pfc::string8 heading = "";
	pfc::string8 heading_duration = "";
	has_tracklist->total_headings = 0;

	size_t last_track_number = 0;
	size_t last_disc_number = 0;

	for (size_t w = 0; w < json_array_size(jsTracklist); w++) {

		// walk jstrack
		json_t* jstrack = json_array_get(jsTracklist, w);

		pfc::string8 type = JSONAttributeString(jstrack, "type_");

		if (STR_EQUAL(type, "heading")) {

			heading = JSONAttributeString(jstrack, "title");
			heading_duration = JSONAttributeString(jstrack, "duration");
			has_tracklist->total_headings++;
		}
		else {

			// parse track fields

			parseReleaseTrack(jstrack, intermediate_tracks, heading, heading_duration,
				/*index*/nullptr, has_artists, has_indexes, last_disc_number, last_track_number);

		}
	}

	has_tracklist->discogs_tracklist_count = intermediate_tracks.get_count()/*discogs_ori_track_count*/;
	has_tracklist->discs.force_reset();

	// roman, english notation preprocessor

	std::vector<std::string>vbk_romans_alphas; vbk_romans_alphas.resize(intermediate_tracks.get_count());
	std::vector<std::string>vbk_romans_nums; vbk_romans_nums.resize(intermediate_tracks.get_count());
	size_t ndx = 0;
	bool no_separator = true;
	bool invalid_separator = false;
	std::string sep;
	for (auto wi : intermediate_tracks) {
		std::string outputAlpha;
		std::string outputNum;
		const std::string tmp = wi->discogs_track_number.toUpper().trim('.').get_ptr();
		copy_if(tmp.begin(), tmp.end(), back_inserter(outputAlpha), [](char c) {
			return isalpha(c);
			});
		copy_if(tmp.begin(), tmp.end(), back_inserter(outputNum), [](char c) {
			return !isalpha(c);
			});


		if (!outputAlpha.size() || !outputNum.size()) {
			vbk_romans_alphas.clear();
			vbk_romans_nums.clear();
			break;
		}

		vbk_romans_alphas[ndx] = outputAlpha;
		vbk_romans_nums[ndx] = outputNum;

		no_separator &= outputAlpha.size() + outputNum.size() == tmp.size();
		if (!no_separator) {
			std::string old_sep;
			copy_if(tmp.begin(), tmp.end(), back_inserter(sep), [](char c) {
				return (c == '-' || c== '.');
				});
			if (ndx == 0) old_sep = sep;
			invalid_separator |= ((sep.size() > 1) | ((bool)sep.compare(old_sep)));
		}
		++ndx;
	}

	// check all are romans

	bool all_subroman = vbk_romans_alphas.size();
	size_t it_wi = 0;
	for (auto wi : vbk_romans_alphas) {
		if (!it_wi) {
			//check roman = CD, DVD
			for (ReleaseFormat_ptr wrf : formats) {
				if (wrf->get_name().get_cvalue().equals(wi.c_str())) {
					all_subroman = false;
					break;
				}
			}
		}
		all_subroman &= validation_of_roman_number(wi);
		if (!all_subroman) break;
		++it_wi;
	}

	bool removed_parenthesized = false;

	for (size_t i = 0; i < intermediate_tracks.get_size(); i++) {
		ReleaseTrack_ptr& track = intermediate_tracks[i];
		removed_parenthesized |= remove_parenthesized(track->discogs_track_number);
	}


	// check all english
	bool all_subenglish = vbk_romans_alphas.size();
	it_wi = 0;
	for (auto wi : vbk_romans_alphas) {
		if (!it_wi) {
			//..
		}
		all_subenglish &= (EnglishToDecimal(wi) != SIZE_MAX);
		if (!all_subenglish) break;
		++it_wi;
	}

	// if they are backup original romans and susbtitute by digits (1-1,1-2... 2-1, 2-2)

	std::vector<pfc::string8>vbk_romans;
	bool roman_done = false;
	if (all_subroman && sep.size() <= 1 && !invalid_separator) {
		roman_done = true;
		vbk_romans.resize(intermediate_tracks.get_count());
		size_t ndx = 0;
		for (auto wi : intermediate_tracks) {
			vbk_romans[ndx] = wi->discogs_track_number;
			wi->discogs_track_number = PFC_string_formatter() << RomanToDecimal(vbk_romans_alphas[ndx].c_str());
			wi->discogs_track_number << (no_separator ? "-" : sep.c_str());
			wi->discogs_track_number << vbk_romans_nums[ndx].c_str();
			++ndx;
		}
	}

	bool english_done = false;
	if (all_subenglish && sep.size() <= 1 && !invalid_separator) {
		english_done = true;
		vbk_romans.resize(intermediate_tracks.get_count());
		size_t ndx = 0;
		for (auto wi : intermediate_tracks) {
			vbk_romans[ndx] = wi->discogs_track_number;
			wi->discogs_track_number = vbk_romans_nums[ndx].c_str();
			wi->discogs_track_number << PFC_string_formatter() << EnglishToDecimal(vbk_romans_alphas[ndx].c_str());
			++ndx;
		}
	}

	// if not a combination, try pure romans (I, II, III, IV,...)

	if (!roman_done && !english_done) {

		//test only roman https://www.discogs.com/release/29273164-Dvne-Asheran?redirected=true

		//roman numbers (all)
		bool allroman = true;
		size_t it_wi = 0;
		for (auto wi : intermediate_tracks) {
			if (!it_wi) {
				//check roman = CD, DVD
				for (ReleaseFormat_ptr wrf : formats) {
					if (wrf->get_name().get_cvalue().equals(wi->discogs_track_number.trim('.').toUpper().c_str())) {
						all_subroman = false;
						break;
					}
				}
			}
			allroman &= validation_of_roman_number(wi->discogs_track_number.trim('.').toUpper().c_str());
			if (!allroman) break;
			++it_wi;
		}

		if (allroman) {
			//backup discogs_track_numbers
			roman_done = true;
			vbk_romans.resize(intermediate_tracks.get_count());
			size_t ndx = 0;
			for (auto& wi : intermediate_tracks) {
				vbk_romans[ndx] = wi->discogs_track_number;
				wi->discogs_track_number = PFC_string_formatter() << RomanToDecimal(vbk_romans[ndx].toUpper().trim('.'));
				++ndx;
			}
		}
	}

	// parse release track positions

	bool res = false;
	if (isRelease) {
		try {
			res = parseAllTrackPositions(intermediate_tracks, has_tracklist);
		}
		catch (...) {
			log_msg("Error parsing track posisitons");
		}
	}

	// end release track positions

	// roman, english notation postprocessor

	if (roman_done || english_done) {
		size_t ndx = 0;
		for (auto& wi : intermediate_tracks) {
			wi->discogs_track_number = vbk_romans[ndx];
			++ndx;
		}
	}

}

pfc::string8 getYearFromReleased(pfc::string8 released) {
	int dash = released.find_first("-");
	if (dash != pfc::infinite_size) {
		released = substr(released, 0, dash);
	}
	if (released.get_length() == 4) {
		return released;
	}
	return "";
}

pfc::string8 remove_dot_2spaces(pfc::string8 in) {
	pfc::string8_fastalloc buffer;
	uReplaceString(buffer, in, pfc_infinite, ".  ", 3, ". ", 2, false);
	return buffer;
}

// PARSE RELEASE

void Discogs::parseRelease(Release *release, json_t *root) {

	assert_is_object(root);

	release->id = JSONAttributeString(root, "id");


	auto master_id = JSONAttributeString(root, "master_id");
	if (!atoi(master_id)) master_id = "";
	if (master_id.get_length()) {
		MasterRelease_ptr master = discogs_interface->get_master_release(master_id);
		release->set_master_release(std::move(master));
	}

	release->title = JSONAttributeString(root, "title");
	release->country = JSONAttributeString(root, "country");
	release->genres = JSONAttributeStringArray(root, "genres");
	release->styles = JSONAttributeStringArray(root, "styles");

	release->barcode = JSONAttributeObjectArrayAttributeStringWhere(root, "identifiers", "value", "type", "Barcode");

	pfc::array_t<pfc::string8> tokens;
	release->release_date_raw = JSONAttributeString(root, "released");
	int num_tokens = tokenize(release->release_date_raw, "-", tokens, false);

	//release date

	release->release_date = "";
	try {
		if (num_tokens) {
			if (num_tokens == 1) {
				release->release_year = tokens[0];
			}
			else if (num_tokens == 3) {
				release->release_year = tokens[0];
				if (!(STR_EQUAL(tokens[1], "00"))) {
					release->release_month = tokens[1];
					release->release_date << MONTH_NAMES.at(tokens[1].get_ptr()) << " ";
				}
				if (!(STR_EQUAL(tokens[2], "00"))) {
					release->release_day = tokens[2];
					release->release_date << tokens[2] << " ";
				}
			}
			release->release_date << tokens[0];
		}
	}
	catch (std::out_of_range) {
		throw foo_discogs_exception("Error parsing release date.");
	}

	//

	json_t *labels = json_object_get(root, "labels");
	parseReleaseLabels(labels, release->labels);

	//All Media, Box Set, File, ..., CD, DVD, ... + quantity
	json_t *formats = json_object_get(root, "formats");
	parseReleaseFormats(formats, release->formats);

	size_t volume_count = 0;
	size_t volume_discarded_count = 0;

	for (size_t w = 0; w < release->formats.get_count(); w++) {
		if (release->formats[w]->format_incl_vol) {
			volume_count += atoi(release->formats[w]->get_quantity().get_value());
		}
		else {
			if (release->formats[w]->get_name().get_value().equals("Box Set") ||
					release->formats[w]->get_name().get_value().equals("All Media")) {
				//..
				++volume_discarded_count;
			}
			else if (release->formats[w]->get_name().get_value().equals("File")) {
				//put files into a volume (disk)
				++volume_count;
			}
			else {
				volume_count += atoi(release->formats[w]->get_quantity().get_value());
			}
		}
	}

	//total number of volumes in release + 1 for each Box Sets
	pfc::string8 rel_fq(JSONAttributeString(root, "format_quantity").get_ptr());

	if (atoi(rel_fq) != (volume_count + volume_discarded_count)) {
		log_msg(PFC_string_formatter() << "Release format mismatch (" << release->id <<"");
	}

	release->discogs_total_discs = volume_count;

	json_t *artists = json_object_get(root, "artists");
	parseReleaseArtists(artists, release->artists, true);

	if (!release->artists.get_size()) {
		parser_exception ex;
		ex << " Release has no artists!";
		throw ex;
	}

	json_t *jsTrackList = json_object_get(root, "tracklist");

	// PARSE ALL RELEASE TRACKS

	parseAllReleaseTracks(jsTrackList, true, release->formats, /*has track list*/release,/*has artists*/ release, /*has indexes*/ release);

	//

	json_t *extraartists = json_object_get(root, "extraartists");
	if (extraartists) {
		parseReleaseCredits(extraartists, release->credits, release);
	}

	pfc::string8 tmp = remove_dot_2spaces(JSONAttributeString(root, "notes"));
	tmp.replace_char('\x01', ' ');
	tmp.replace_char('\x02', ' ');
	tmp.replace_char('\x03', ' ');
	release->notes = tmp;

	json_t* images = json_object_get(root, "images");
	parseImages(images, release->images);

	release->videos = JSONAttributeObjectArrayAttributeString(root, "videos", "uri");
	json_t* companies = json_object_get(root, "companies");
	parseReleaseCompanies(companies, release->companies);
	json_t* series = json_object_get(root, "series");
	parseReleaseSeries(series, release->series);
	release->weight = JSONAttributeString(root, "estimated_weight");

	json_t* community = json_object_get(root, "community");
	if (json_is_object(community)) {
		release->discogs_status = JSONAttributeString(community, "status");
		release->members_have = JSONAttributeString(community, "have");
		release->members_want = JSONAttributeString(community, "want");
		json_t* rating = json_object_get(community, "rating");
		if (json_is_object(rating)) {
			release->discogs_avg_rating = JSONAttributeString(rating, "average");
			release->discogs_rating_votes = JSONAttributeString(rating, "count");
		}
		json_t* submitter = json_object_get(community, "submitter");
		if (json_is_object(submitter)) {
			release->submitted_by = JSONAttributeString(submitter, "username");
		}
		release->discogs_data_quality = JSONAttributeString(community, "data_quality");
		release->date_added = JSONAttributeString(root, "date_added");
		release->date_changed = JSONAttributeString(root, "date_changed");
	}
}

void Discogs::parseReleaseRating(Release *release, json_t *root) {
	release->discogs_my_rating = JSONAttributeString(root, "rating");
}

void Discogs::parseMasterRelease(MasterRelease *master_release, json_t *root) {
	assert_is_object(root);

	master_release->id = JSONAttributeString(root, "id");
	master_release->main_release_id = JSONAttributeString(root, "main_release");
	Release_ptr main_release = discogs_interface->get_release(encode_mr(0, master_release->main_release_id/*->id*/)/*,JSONAttributeString(root, "main_release")*/);
	master_release->set_main_release(main_release);
	master_release->main_release_api_url = JSONAttributeString(root, "main_release_url");
	master_release->main_release_url << "https://www.discogs.com/release/" << master_release->main_release_id;

	master_release->most_recent_release_id = JSONAttributeString(root, "most_recent_release");
	master_release->most_recent_release_url << "https://www.discogs.com/release/" << master_release->most_recent_release_id;

	/*= JSONAttributeString(root, "uri")*/;
	master_release->release_year = JSONAttributeString(root, "year");
	if (!atoi(master_release->release_year)) master_release->release_year = ""; //remove bad entry "0" years

	master_release->versions_api_url = JSONAttributeString(root, "versions_url");
	master_release->title = JSONAttributeString(root, "title");
	master_release->discogs_data_quality = JSONAttributeString(root, "data_quality");
	master_release->genres = JSONAttributeStringArray(root, "genres");
	master_release->styles = JSONAttributeStringArray(root, "styles");
	master_release->videos = JSONAttributeObjectArrayAttributeString(root, "videos", "uri");

	master_release->search_role = JSONAttributeString(root, "role");
	master_release->search_roles.add_item(JSONAttributeString(root, "role"));

	json_t *images = json_object_get(root, "images");
	parseImages(images, master_release->images);

	json_t *artists = json_object_get(root, "artists");
	parseReleaseArtists(artists, master_release->artists);

	if (!master_release->artists.get_size()) {
		parser_exception ex;
		ex << " Master release has no artists!";
		throw ex;
	}

	json_t *jsTracklist = json_object_get(root, "tracklist");

	parseAllReleaseTracks(jsTracklist, false, pfc::array_t<ReleaseFormat_ptr>(), master_release, master_release, master_release);

}

// Discogs can return the same release/master ID with different artist roles
// across pages. Preserve Main once observed and keep each role only once;
// a later Other result must not silently hide a Main release in the tree.
template <typename T>
static void merge_search_result_role(T& target, const pfc::string8& incoming) {
    if (!incoming.get_length()) return;
    bool seen = false;
    for (const auto& role : target.search_roles) {
        if (role.equals(incoming)) { seen = true; break; }
    }
    if (!seen) target.search_roles.add_item(incoming);
    if (!target.search_role.get_length() ||
        (incoming.equals("Main") && !target.search_role.equals("Main")))
        target.search_role = incoming;
}

void parse_release_search_fields(json_t* rel, Release_ptr release) {

	if (!release->artists.get_count()) {
		//non-cached release

		//ID
		release->id = JSONAttributeString(rel, "id");
		//MASTER_ID
		release->master_id = JSONAttributeString(rel, "master_id");
		if (!atoi(release->master_id)) release->master_id = "";
		//TITLE
		release->title = JSONAttributeString(rel, "title");
		//COUNTRY
		release->country = JSONAttributeString(rel, "country");
		//YEAR
		release->release_year = JSONAttributeString(rel, "year");
		if (!release->release_year.get_length()) {
			//use released instead
			release->release_year = getYearFromReleased(JSONAttributeString(rel, "released"));
		}
		//GENRE
		release->genres = JSONAttributeStringArray(rel, "genre");
		//STYLE
		release->styles = JSONAttributeStringArray(rel, "style");
		//CATNO
		release->search_catno = JSONAttributeString(rel, "catno");
		//BARCODE
		release->barcode = JSONAttributeString(rel, "barcode");
	}

	auto jobj_label = json_object_get(rel, "label");

	if (jobj_label) {

		pfc::string8 all_labels;
		pfc::array_t<pfc::string8> flist;

		bool is_array = json_is_array(jobj_label);
		if (is_array/*as in query_mode*/) {
			flist = JSONAttributeStringArray(rel, "label");
			//remove duplicates
			std::unordered_set<std::string> s;
			for (auto w : flist) {
				s.insert(w.c_str());
			}

			if (flist.get_count()) {
				all_labels = flist[0];
			}
		}
		else {
			all_labels = JSONAttributeString(rel, "label");
		}
		release->search_labels = std::move(all_labels);
	}

	auto jobj = json_object_get(rel, "format");
	if (jobj) {

		pfc::string8 all_formats;
		pfc::array_t<pfc::string8> flist;

		bool is_array = json_is_array(jobj);
		if (is_array/*as in query_mode*/) {
			flist = JSONAttributeStringArray(rel, "format");
			all_formats = pfc::format_array(flist);
		}
		else {
			all_formats = JSONAttributeString(rel, "format");
			tokenize(all_formats, ",", flist, false);
		}
		if (flist.get_count()) {
			tokenize(flist[0], ",", release->search_major_formats, false);
			t_size formats_cpos = flist[0].get_length();
			release->search_formats = ltrim(substr(all_formats, formats_cpos + (int)(flist.get_count() > 1)));
		}
	}

	//only query: query_major_formats_qty
	json_t* s = json_object_get(rel, "format_quantity");

	if (json_is_integer(s) && (json_integer_value(s) > 1)) {
		release->query_major_formats_qty = JSONAttributeString(rel, "format_quantity");// json_integer_value(/*s*/"format_quantity");
	}

	json_t* formats = json_object_get(rel, "formats");
	if (json_is_array(formats)) {
		pfc::array_t<pfc::string8> mf;
		std::unordered_set<std::string> seen_minors;
		std::vector<std::string> ordered_minors;
		for (size_t i = 0; i < json_array_size(formats); i++) {
			json_t* format = json_array_get(formats, i);
			if (!json_is_object(format)) continue;
			json_t* name = json_object_get(format, "name");
			// Discogs' formats array is external data. Missing/null/non-string
			// names or descriptions must not become std::string(nullptr).
			if (!json_is_string(name)) continue;
			const char* format_name = json_string_value(name);
			if (!format_name || !*format_name) continue;
			std::string wmajor;
			json_t* qty = json_object_get(format, "qty");
			if (json_is_string(qty) && atoi(json_string_value(qty)) > 1)
				wmajor.append(json_string_value(qty)).append("x");
			wmajor.append(format_name);
			mf.add_item(wmajor.c_str());
			json_t* descs = json_object_get(format, "descriptions");
			if (!json_is_array(descs)) continue;
			for (size_t j = 0; j < json_array_size(descs); j++) {
				json_t* desc = json_array_get(descs, j);
				if (!json_is_string(desc)) continue;
				const char* value = json_string_value(desc);
				if (value && *value && seen_minors.emplace(value).second)
					ordered_minors.emplace_back(value);
			}
		}
		release->search_major_formats.force_reset();
		release->search_major_formats.move_from(mf);
		if (!ordered_minors.empty()) {
			std::string joined;
			for (const auto& minor : ordered_minors) {
				if (!joined.empty()) joined += ", ";
				joined += minor;
			}
			release->search_formats = joined.c_str();
		}
	}
	merge_search_result_role(*release, JSONAttributeString(rel, "role"));
}

MasterRelease_ptr check_in_master(pfc::string8 master_id, std::map<std::string, MasterRelease_ptr>& mva_masters, Artist* artist, size_t query_mode) {

	MasterRelease_ptr release;

	std::map<std::string, MasterRelease_ptr>::iterator it;
	it = mva_masters.find(master_id.get_ptr());

	if (it != mva_masters.end()) {
		release = it->second;
	}
	else {
		if (query_mode & SearchMode::AT) {
			release = discogs_interface->get_master_release(master_id, true);
		}
		else {
			auto enc = encode_mr(atoi(artist->id), master_id);
			release = discogs_interface->get_master_release(enc, false);

			if (!release->loaded_releases && release->sub_releases.get_count()) {
				release = discogs_interface->get_master_release(enc, true);
			}
		}
		//..
		mva_masters.emplace(master_id, release);
		//..
	}
	return release;
}

// both from artist search and search query
void Discogs::parseArtistReleases(json_t* root, Artist* artist, int query_mode, std::map<std::string, MasterRelease_ptr>& mva_masters, std::pair<size_t, size_t>& qry_mr_found) {
	try {
		json_t* releases;
		pfc::array_t<pfc::string8> results_artists;

		if (query_mode) {
			qry_mr_found = { 0, 0};
			releases = json_object_get(root, "results");
		}
		else {
			releases = json_object_get(root, "releases");
		}
		if (json_is_array(releases)) {
			auto c = json_array_size(releases);
			// LOOP RELEASE IN RESULTS/RELEASES
			for (size_t i = 0; i < json_array_size(releases); i++) {
				json_t* rel = json_array_get(releases, i);

				if (json_is_object(rel)) {
					// TYPE
					pfc::string8 type = JSONAttributeString(rel, "type");
					//..
					if (STR_EQUAL(type, "master")) {
						// MASTER TYPE
						++qry_mr_found.first;
						// CHECK-IN MASTER
						pfc::string8 master_id = trim(JSONAttributeString(rel, "id"), " ");

						MasterRelease_ptr release = check_in_master(master_id, mva_masters, artist, query_mode);

						pfc::string8 uri = JSONAttributeString(rel, "uri");
						pfc::string8 main_release = JSONAttributeString(rel, "main_release");

						if (!release->loaded_preview) {

							release->title = JSONAttributeString(rel, "title");
							release->release_year = JSONAttributeString(rel, "year");
							if (!release->release_year.get_length()) {
								release->release_year = getYearFromReleased(JSONAttributeString(rel, "released"));
							}
							release->genres = JSONAttributeStringArray(rel, "genre");
							release->styles = JSONAttributeStringArray(rel, "style");
							size_t lkey = encode_mr(artist->search_role_list_pos, main_release);

							if (lkey) {
								//..
								Release_ptr main_release = discogs_interface->get_release(lkey, true, query_mode & SearchMode::AT);
								//..
								release->set_main_release(main_release);
							}
							merge_search_result_role(*release, JSONAttributeString(rel, "role"));

						}

						bool duplicate = false;
						size_t va_duplicated_master_ndx = 0;

						for (MasterRelease_ptr walk_master_release : artist->master_releases) {

							if (release->id == walk_master_release->id) {
								merge_search_result_role(*walk_master_release,
									JSONAttributeString(rel, "role"));

								duplicate = true;

								break;
							}
							++va_duplicated_master_ndx;
						}
						if (!duplicate) {
							// ADD MASTER AND SEARCH ORDER TO ARTIST 
							release->loaded_preview = true;
							check_in_master(release->id, mva_masters, artist, query_mode);
							artist->master_releases.append_single(std::move(release));
							artist->search_order_master.append_single(true);
						}
						else {
							if (query_mode & SearchMode::AT) {
								auto subrels = artist->master_releases[va_duplicated_master_ndx]->sub_releases;
								if (!artist->master_releases[va_duplicated_master_ndx]->title.get_length()) {
									artist->master_releases[va_duplicated_master_ndx] = std::move(release);
									artist->master_releases[va_duplicated_master_ndx]->sub_releases = std::move(subrels);
								}
							}
						}
					}
					else if (STR_EQUAL(type, "release")) {

						// NON-MASTER RELASE & RELEASES
						pfc::string8 dbid = JSONAttributeString(rel, "id");

						size_t lkey = encode_mr(artist->search_role_list_pos, dbid);

						//..
						Release_ptr release = discogs_interface->get_release(lkey, true, query_mode & SearchMode::AT);
						parse_release_search_fields(rel, release);
						//..

						if (query_mode ||(!release->loaded_preview && !release->loaded)) {
							release->master_id = JSONAttributeString(rel, "master_id");
							if (!atoi(release->master_id)) release->master_id = "";
							if (!atoi(release->master_id)) {
								bool duplicate = false;
								// NON-MASTER RELEASE
								for (Release_ptr walk_artist_release : artist->releases) {
									if (release->id == walk_artist_release->id) {
										duplicate = true;
										break;
									}
								}

								if (!duplicate) {
									++qry_mr_found.second;
									artist->releases.add_item(std::move(release));
									artist->search_order_master.append_single(false);
								}
								//..
								continue;
								//..
							}

							if (query_mode & SearchMode::AT) {

								MasterRelease_ptr m = check_in_master(release->master_id, mva_masters, artist, query_mode);
								bool done = false;

								for (MasterRelease_ptr wamr : artist->master_releases) {

									if (done = wamr->id.equals(release->master_id)) {
										// ADD SUBRELEASE TO ANOHTER MASTER
                                        bool present = false;
                                        for (const Release_ptr& prior : wamr->sub_releases) {
                                            if (prior->id == release->id) { present = true; break; }
                                        }
                                        if (!present) wamr->sub_releases.add_item(release);
										break;
									}
								}

								if (!done) {
									// ADD SUBRELEASE TO NEW MASTER
									if (!m->title.get_length()) {
										m->title = PFC_string_formatter() << "(raw) " << release->title;
										m->release_year = release->release_year;
									}
									//..
									artist->master_releases.add_item(m);
									bool present = false;
                                    for (const Release_ptr& prior : m->sub_releases) {
                                        if (prior->id == release->id) { present = true; break; }
                                    }
                                    if (!present) m->sub_releases.add_item(release);
								}
							}
						}

						bool duplicate = false;

						for (Release_ptr walk_release : artist->releases) {
							if (release->id == walk_release.get()->id) {
								merge_search_result_role(*walk_release,
									JSONAttributeString(rel, "role"));

								duplicate = true;
								break;
							}
						}

						if (!duplicate) {
							release->loaded_preview = true;
							//todo: ?
							if (query_mode & SearchMode::VA) {
								if ((atoi(release->master_id))) {
									continue;
								}
							}

							// APPEND SEARCH ORDER AND REL
								artist->releases.append_single(std::move(release));
								artist->search_order_master.append_single(false);
						}
					}
					else {
						// OTHER OBJECTS IN RESULTS
					}
				}
			}
		}
	}
	catch (...) {
		foo_discogs_exception ex;
		ex << "Unknown error parsing artist releases.";
		throw ex;
	}
}

void Discogs::parseMasterVersions(json_t *root, MasterRelease *master_release, int query_mode, bool isolated_versions) {

	json_t *versions = json_object_get(root, "versions");

	if (json_is_array(versions)) {
		for (size_t i = 0; i < json_array_size(versions); i++) {
			json_t *rel = json_array_get(versions, i);

			if (json_is_object(rel)) {
				pfc::string8 release_id = JSONAttributeString(rel, "id");

				Release_ptr release;
                MasterRelease_ptr master;
				size_t lkey;
				size_t found_i = SIZE_MAX;

                // Modeless master expansion must never mutate cached Release
                // objects or read a live TreeView child's fields on the worker.
                // The UI thread cloned partial children before dispatch.
                if (isolated_versions) {
                    // An isolated snapshot may already contain a partial
                    // child even in full (query_mode=0) pagination. Update
                    // that clone by identity instead of silently leaving
                    // its preview stale. In AT partial-mode, unknown child
                    // IDs remain excluded; full mode may append new IDs.
                    if (!master_release->loaded_releases &&
                        master_release->sub_releases.get_count()) {
                        found_i = discogger_master::find_partial_version_index(
                            master_release->sub_releases.get_count(), [&](size_t candidate) {
                                const auto& child = master_release->sub_releases[candidate];
                                return child && child->id.equals(release_id);
                            });
                        if (found_i != SIZE_MAX) {
                            release = master_release->sub_releases[found_i];
                        } else if (query_mode & SearchMode::AT) {
                            continue;
                        } else {
                            release = std::make_shared<Release>(release_id.get_ptr());
                        }
                    } else {
                        release = std::make_shared<Release>(release_id.get_ptr());
                    }
                } else {
    				if (query_mode & SearchMode::AT) {
    
    					if (!master_release->loaded_releases && master_release->sub_releases.get_count()) {
    
    						found_i = discogger_master::find_partial_version_index(
                                master_release->sub_releases.get_count(), [&](size_t candidate) {
                                    const auto& existing = master_release->sub_releases[candidate];
                                    return existing && existing->id.equals(release_id);
                                });
    						if (found_i == SIZE_MAX) {
    							continue;
    						}
    						else {
    							lkey = encode_mr(0, atol(master_release->sub_releases[found_i]->id));
    							release = discogs_interface->get_release(lkey);
    						}
    					}
    					else {
    						lkey = encode_mr(0, atol(release_id));
    						release = discogs_interface->get_release(lkey);
    					}
    				}
    				else {
    					lkey = encode_mr(0, atol(release_id));
    					release = discogs_interface->get_release(lkey);
    				}
    
    				//mr from any artist
    				lkey = encode_mr(0, atol(master_release->id));
    				//..
                    master = discogs_interface->get_master_release(lkey);
    
                }

				if (!release->loaded_preview) {
                    if (isolated_versions) {
                        // No global master/cache mutation or fetch on the worker.
                        release->master_id = master_release->id;
                    } else {
                        release->set_master_release(master);
                    }
					release->title = JSONAttributeString(rel, "title");
					release->search_labels = JSONAttributeString(rel, "label");
					release->search_major_formats = JSONAttributeStringArray(rel, "major_formats");
					release->search_formats = JSONAttributeString(rel, "format");
					release->search_catno = JSONAttributeString(rel, "catno");
					release->release_year = JSONAttributeString(rel, "year");
					release->db_total_tracks = atoi(JSONAttributeString(rel, "db_total_tracks"));

					if (release->release_year.get_length() == 0) {
						release->release_year = getYearFromReleased(JSONAttributeString(rel, "released"));
					}
					release->country = JSONAttributeString(rel, "country");
				}

				bool duplicate = false;
				for (Release_ptr walk_subrelease : master_release->sub_releases) {

					if (walk_subrelease->id == release->id) {
						duplicate = true;
						break;
					}
				}
                // A matched partial version is a duplicate ID, but its
                // preview has nevertheless been successfully parsed.
                release->loaded_preview = true;
				if (!duplicate) {
					if (!(bool)query_mode || found_i == SIZE_MAX) {
						master_release->sub_releases.append_single(std::move(release));
					}
				}
			}
		}
	}
}


// The non-modeless master loader also needs the same fail-closed staging
// contract as the modeless UI path. A later page with invalid child IDs or a
// cancellation must not leave a half-updated shared master or cached release.
MasterRelease_ptr Discogs::stageMasterVersionPages(
    const MasterRelease& current, const std::vector<json_t*>& roots,
    int query_mode, const std::function<void()>& check_abort) {
    // Discogs pagination can shift while requests are in flight. Repeated
    // release identities make the assembled page set ambiguous: abort the
    // entire transaction rather than silently hiding a missing version.
    std::unordered_set<std::string> page_release_ids;
    return discogger_master::stage_master_pages<MasterRelease_ptr>(
        roots.size(),
        [&]() {
            MasterRelease_ptr staged = std::make_shared<MasterRelease>(current.id.c_str());
            staged->loaded_releases = current.loaded_releases;
            for (const Release_ptr& child : current.sub_releases) {
                if (!child)
                    throw foo_discogs_exception("Null existing master-version child; retaining prior list.");
                staged->sub_releases.append_single(std::make_shared<Release>(*child));
            }
            return staged;
        },
        [&](std::size_t page) {
            const json_t* versions = json_object_get(roots[page], "versions");
            if (!json_is_array(versions))
                throw foo_discogs_exception("Missing master versions array; retaining prior list.");
            for (std::size_t i = 0; i < json_array_size(versions); ++i) {
                json_t* child = json_array_get(versions, i);
                if (!json_is_object(child))
                    throw foo_discogs_exception("Invalid master version object; retaining prior list.");
                const pfc::string8 child_id = JSONAttributeString(child, "id");
                if (!discogger_search::valid_release_id(child_id.c_str()))
                    throw foo_discogs_exception("Invalid master version ID; retaining prior list.");
                if (!page_release_ids.emplace(child_id.c_str()).second)
                    throw foo_discogs_exception("Duplicate master version ID across pages; retaining prior list.");
            }
        },
        [&](MasterRelease_ptr& staged, std::size_t page) {
            // Isolated mode never retrieves/mutates canonical cached Release
            // objects. Partial AT children are deep-copied by the snapshot.
            parseMasterVersions(roots[page], staged.get(), query_mode, true);
        },
        check_abort);
}

void Discogs::parseArtist(Artist *artist, json_t *root) {

	assert_is_object(root);

	artist->profile = JSONAttributeString(root, "profile");
	artist->name = JSONAttributeString(root, "name");

	artist->id = JSONAttributeString(root, "id");

	artist->anvs = JSONAttributeStringArray(root, "namevariations");
	artist->urls = JSONAttributeStringArray(root, "urls");

	artist->realname = JSONAttributeString(root, "realname");

	json_t *images = json_object_get(root, "images");
	parseImages(images, artist->images);

	artist->aliases = JSONAttributeObjectArrayAttributeString(root, "aliases", "name");
	artist->ingroups = JSONAttributeObjectArrayAttributeString(root, "groups", "name");
	artist->members = JSONAttributeObjectArrayAttributeString(root, "members", "name");
}

void Discogs::parseArtistResults(json_t *root, pfc::array_t<Artist_ptr> &artists) {
	assert_is_object(root);
	json_t *results = json_object_get(root, "results");
	if (json_is_array(results)) {
		for (size_t i = 0; i < json_array_size(results); i++) {
			json_t *result = json_array_get(results, i);
			if (json_is_object(result)) {
				Artist_ptr artist = discogs_interface->get_artist(JSONAttributeString(result, "id"));
				artist->name = JSONAttributeString(result, "title");
				artists.append_single(std::move(artist));
			}
		}
	}
}


void initialize_null_artist(Artist *artist) {
	artist->profile = "";
	artist->anvs = pfc::array_t<pfc::string8>();
	artist->urls = pfc::array_t<pfc::string8>();
	artist->realname = "";
	artist->aliases = pfc::array_t<pfc::string8>();
	artist->ingroups = pfc::array_t<pfc::string8>();
	artist->members = pfc::array_t<pfc::string8>();
	artist->loaded = true;
}

void Discogs::ReleaseArtist::load(threaded_process_status& p_status, abort_callback& p_abort, bool throw_all) {

	if (!preload || loaded) { loaded = true; return; }
	Artist_ptr artist;
	if (id.get_length() && !id.equals("0")) {
		artist = discogs_interface->get_artist(id, false, p_status, p_abort, false, throw_all, false);
		if (artist->profile.equals("404")) {
			log_msg(PFC_string_formatter() << "Error 404 loading release artist " << id);
		}
	}
	full_artist = std::move(artist);
	loaded = true;
}

void Discogs::Artist::load(threaded_process_status &p_status, abort_callback &p_abort, bool throw_all) {

	if (loaded) {
		return;
	}

	bool db_isready = false;

	bool offline_can_read = ol::can_read();
	bool offline_can_write = ol::can_write();
	bool offline_can_overwrite = ol::can_ovr();

	bool expired = false;
	pfc::string8 n8_rel_path;
	bool offline_avail_data = ol::is_data_avail(id, "", ol::GetFrom::Artist, n8_rel_path, true, expired);
	bool btransient = !(offline_can_read && offline_avail_data) || offline_can_overwrite;

	btransient &= !db_isready;

	try {
		if (STR_EQUAL(id, "355") || STR_EQUAL(id, "Unknown Artist")) {
			name = "Unknown Artist";
			initialize_null_artist(this);
			return;
		}
		else if (STR_EQUAL(id, "118760") || STR_EQUAL(id, "No Artist")) {
			name = "No Artist";
			initialize_null_artist(this);
			return;
		}
		else if (STR_EQUAL(id, "194") || STR_EQUAL(id, "Various")) {
			name = "Various";
			initialize_null_artist(this);
			return;
		}

		pfc::string8 msg(PFC_string_formatter() << (offline_can_read && expired? "expired:" :"") << "loading artist " << id);
		msg << id << "...";
		p_status.set_item(msg);

		pfc::string8 json;
		pfc::string8 url;

		if (btransient) {
			url << "https://api.discogs.com/artists/" << id;
			discogs_interface->fetcher->fetch_html(url, "", json, p_abort);
		}
		else {
			if (offline_avail_data) {
				pfc::string8 empty_release_id;
                discogs_interface->get_entity_offline_cache(ol::GetFrom::Artist, id, empty_release_id, json, p_abort, "Fetching offline cache artist releases...", p_status);

				if (json.get_length()) {
					offline_can_write = false;
				}
				else {
					pfc::string8 msg;
					msg << "Error loading disk cache, deleting invalid cache (artist_id:" << id << ") ...";
					log_msg(msg);
					discogs_interface->delete_artist_cache(id);
					url << "https://api.discogs.com/artists/" << id;
					discogs_interface->fetcher->fetch_html(url, "", json, p_abort);
				}
			}
		}

		if (!json.get_length()) {
			foo_discogs_exception ex("Truncated results error.");
			throw ex;
		}

		// parse json and artist

		JSONParser jp(json);
		parseArtist(this, jp.root);

		//..

		loaded = true;
        if (btransient && offline_can_write) {
            discogger_cache::directory_cache_transaction tx(
                std::filesystem::u8path(n8_rel_path.c_str()),
                ol::cache_payload_is_valid_json);
            if (tx.ready() && discogs_interface->offline_cache_save(
                    ol::staged_cache_json_path(tx), jp.root))
                tx.publish("");
        }
	}
	catch (http_404_exception &e) {
		throw;
	}
	catch (foo_discogs_exception &e) {

		if (throw_all) {
			throw;
		}

		pfc::string8 error("Error loading artist ");
		error << id << " - " << name << " :" << e.what();
		throw foo_discogs_exception(error);
	}
}

void Discogs::Artist::load_mem_only(threaded_process_status& p_status, abort_callback& p_abort, bool throw_all) {

	if (loaded) {
		return;
	}

	try {
		if (STR_EQUAL(id, "355") || STR_EQUAL(id, "Unknown Artist")) {
			name = "Unknown Artist";
			initialize_null_artist(this);
			return;
		}
		else if (STR_EQUAL(id, "118760") || STR_EQUAL(id, "No Artist")) {
			name = "No Artist";
			initialize_null_artist(this);
			return;
		}
		else if (STR_EQUAL(id, "194") || STR_EQUAL(id, "Various")) {
			name = "Various";
			initialize_null_artist(this);
			return;
		}

		pfc::string8 msg(PFC_string_formatter() << "loading artist " << id <<": " << name << "...");
		p_status.set_item(msg);

		pfc::string8 json;
		pfc::string8 url;

		url << "https://api.discogs.com/artists/" << id;
		discogs_interface->fetcher->fetch_html(url, "", json, p_abort);


		if (!json.get_length()) {
			foo_discogs_exception ex("Truncated results error.");
			throw ex;
		}

		// parse json and artist

		JSONParser jp(json);
		parseArtist(this, jp.root);
		loaded = true;

	}
	catch (http_404_exception& e) {
		throw;
	}
	catch (foo_discogs_exception& e) {

		if (throw_all) {
			throw;
		}

		pfc::string8 error("Error loading artist ");
		error << id << " - " << name << " : " << e.what();
		throw foo_discogs_exception(error);
	}
}

void Discogs::Release::load(threaded_process_status& p_status, abort_callback& p_abort, bool throw_all) {
	if (loaded) {
		return;
	}
	try {
		pfc::string8 json;
		pfc::string8 url;
		url << "https://api.discogs.com/releases/" << id;
		discogs_interface->fetcher->fetch_html(url, "", json, p_abort);

		JSONParser jp(json);
		parseRelease(this, jp.root);
		loaded = true;
	}
	catch (foo_discogs_exception& e) {
		if (throw_all) {
			throw;
		}
		pfc::string8 error("Error loading release ");
		error << id << ": " << e.what();
		throw foo_discogs_exception(error);
	}
}
void Discogs::Release::load(threaded_process_status &p_status, abort_callback &p_abort, bool throw_all, pfc::string8 offlineArtistId, db_fetcher* dbfetcher) {

	if (loaded) {
		return;
	}
	pfc::string8 artist_id = offlineArtistId;
	pfc::string8 release_id = id;

	bool offline_can_read = ol::can_read();
	bool offline_can_write = ol::can_write();
	bool offline_can_ovr = ol::can_ovr();

	if (p_abort.is_aborting()) return;

	bool expired;
	pfc::string8 n8_rel_path;
	bool offline_avail_data = ol::is_data_avail(artist_id, release_id, ol::GetFrom::Release, n8_rel_path, true, expired);

	bool btransient = !(offline_can_read && offline_avail_data) || offline_can_ovr;
	bool db_isready = false;
	bool db_wantartwork = false;
	bool db_skip = !db_isready || db_wantartwork || !dbfetcher;

	btransient &= db_skip;

	try {

		pfc::string8 json;

		if (btransient) {

			pfc::string8 msg;
			msg << "loading online releases.(" << id << ")";
			p_status.set_item(msg);

			pfc::string8 url;
			url << "https://api.discogs.com/releases/" << id;
			discogs_interface->fetcher->fetch_html(url, "", json, p_abort);
		}
		else {
			discogs_interface->get_entity_offline_cache(ol::GetFrom::Release, artist_id, release_id, json, p_abort, "Fetching offline release...", p_status);
			offline_can_write = false;
		}

		//parse json and release

		JSONParser jp(json);
		parseRelease(this, jp.root);
		//..

		loaded = true;
        // Stable, explicitly narrow UTF-8 stamp; the pfc formatter cannot stream string8 objects directly.
        pfc::string8 cache_stamp;
        cache_stamp << date_added.get_ptr() << " " << date_changed.get_ptr();

        if (btransient && offline_can_write) {
            if (atoi(offlineArtistId) == pfc_infinite || !offlineArtistId.get_length()) {
                offlineArtistId = artists[0]->id;
                pfc::string8 alt_path;
                bool alt_expired = false;
                if (ol::is_data_avail(offlineArtistId, id, ol::GetFrom::Release,
                        alt_path, true, alt_expired) && !offline_can_ovr)
                    return;
            }
            n8_rel_path = ol::get_offline_path(offlineArtistId, ol::GetFrom::Release, id, true);
            discogger_cache::directory_cache_transaction tx(
                std::filesystem::u8path(n8_rel_path.c_str()),
                ol::cache_payload_is_valid_json);
            if (tx.ready() && discogs_interface->offline_cache_save(
                    ol::staged_cache_json_path(tx), jp.root))
                tx.publish(cache_stamp.c_str());
        }
	}
	catch (foo_discogs_exception &e) {
		if (throw_all) {
			throw;
		}
		pfc::string8 error("Error loading release ");
		error << id << ": " << e.what();
		throw foo_discogs_exception(error);
	}
}

void Discogs::Release::load_my_rating(threaded_process_status &p_status, abort_callback &p_abort, bool throw_all) {
	if (loaded_my_rating) {
		return;
	}
	try {
		pfc::string8 msg("loading release ");
		msg << id << " personal rating...";
		p_status.set_item(msg);

		pfc::string8 json;
		pfc::string8 url;
		url << "https://api.discogs.com/releases/" << id << "/rating/" << discogs_interface->get_username(p_status, p_abort);
		discogs_interface->fetcher->fetch_html(url, "", json, p_abort);

		JSONParser jp(json);
		parseReleaseRating(this, jp.root);
		loaded_my_rating = true;
	}
	catch (foo_discogs_exception &e) {
		if (throw_all) {
			throw;
		}
		pfc::string8 error("Error loading release ");
		error << id << ": " << e.what();
		throw foo_discogs_exception(error);
	}
}

void Discogs::MasterRelease::load(threaded_process_status &p_status, abort_callback &p_abort, bool throw_all) {
	bool noid = !atoi(id);
	if (loaded || !atoi(id)) {
		return;
	}
	try {
		pfc::string8 json;
		pfc::string8 url;
		url << "https://api.discogs.com/masters/" << id;
		discogs_interface->fetcher->fetch_html(url, "", json, p_abort);

		JSONParser jp(json);
		parseMasterRelease(this, jp.root);
		loaded = true;
	}
	catch (foo_discogs_exception &e) {
		if (throw_all) {
			throw;
		}
		pfc::string8 error("Error loading master release ");
		error << id << ": " << e.what();
		throw foo_discogs_exception(error);
	}
}


void Discogs::Artist::load_releases(threaded_process_status &p_status, abort_callback &p_abort, bool throw_all, db_fetcher* dbfetcher) {

    // Coalesce concurrent requests, but let waiting tasks honor cancellation.
    const auto observed_generation = releases_load_generation.load(std::memory_order_acquire);
    std::unique_lock<std::timed_mutex> load_guard(releases_load_mutex, std::defer_lock);
    while (!load_guard.try_lock_for(std::chrono::milliseconds(100))) {
        p_abort.check();
    }
    p_abort.check();
    if (discogger_cache::skip_coalesced_artist_load(
            loaded_releases, observed_generation,
            releases_load_generation.load(std::memory_order_acquire),
            release_refresh.pending())) {
        return;
    }
    // Only consume the request inside the per-artist lock. Commit the staged
    // result atomically, and restore the request on cancellation or failure.
    const bool explicit_refresh = release_refresh.take();

    // Disk-cache setup and configuration access can throw as well. Restore
    // explicit refresh requests regardless of where an exception occurs.
    try {
	bool offline_can_read = ol::can_read();
	bool offline_can_write = ol::can_write();
	bool offline_can_overwrite = ol::can_ovr();
	bool expired = false;
	pfc::string8 n8_rel_path;
	bool offline_avail_data = ol::is_data_avail(id, "", ol::GetFrom::ArtistReleases, n8_rel_path, true, expired);

	bool btransient = !(offline_can_read && offline_avail_data) || offline_can_overwrite || explicit_refresh;
	bool db_ready = false;

    btransient &= !db_ready;

    const auto memory_age = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - releases_loaded_at);
    if (discogger_cache::reuse_artist_releases(loaded_releases, releases_loaded_from_disk,
        offline_can_read, offline_avail_data, offline_can_overwrite || explicit_refresh, memory_age,
        discogger_cache::ttl(discogger_cache::group::artist_releases,
            CONF.artist_release_cache_hours, CONF.expiration_enabled(), CONF.expiration_days()))) {
        return;
    }

        if (explicit_refresh) p_status.set_item("Refreshing artist releases online...");
		pfc::string8 url;
		pfc::array_t<JSONParser_ptr> pages;
		if (btransient) {
			url << "https://api.discogs.com/artists/" << id << "/releases";
			pages = discogs_interface->get_all_pages(url, "", p_abort, "Fetching artist releases...", p_status);
		}
		else {
            try {
                pages = discogs_interface->get_all_pages_offline_cache(ol::GetFrom::ArtistReleases, id, id, "", p_abort, "Fetching offline artist releases...", p_status);
                offline_can_write = false;
            }
            catch (const exception_aborted&) { throw; }
            catch (const std::exception&) {
                // Never accept a partial or malformed artist cache as complete.
                p_abort.check();
                btransient = true;
                url << "https://api.discogs.com/artists/" << id << "/releases";
                pages = discogs_interface->get_all_pages(url, "", p_abort, "Refreshing invalid offline artist cache...", p_status);
            }
		}

        const size_t count = pages.get_count();
        if (!count) throw foo_discogs_exception("Artist release pages were empty; retaining previous results.");

        // Validate the actual items in each page, not just the pagination
        // header. A truncated but well-formed HTTP response must never
        // replace the previously good artist release list or disk cache.
        for (size_t i = 0; i < count; ++i) {
            p_abort.check();
            const auto& page = pages[i];
            const size_t per_page = page->get_object_int("pagination", "per_page");
            const size_t items = page->get_object_int("pagination", "items");
            const json_t* entries = json_object_get(page->root, "releases");
            if (!json_is_array(entries) ||
                !discogger_cache::valid_cache_payload_count(
                    i, count, per_page, items, json_array_size(entries))) {
                throw foo_discogs_exception(
                    "Incomplete Discogs artist release page; keeping the previous results.");
            }
        }

        // Stage the entire result before publishing or completing any disk marker.
        Artist staged(id.c_str());
        staged.search_role_list_pos = search_role_list_pos;
        std::map<std::string, MasterRelease_ptr> mva_masters;
        for (size_t i = 0; i < count; ++i) {
            p_abort.check();
            parseArtistReleases(pages[i]->root, &staged, SearchMode::DEFAULT_SEARCH, mva_masters);
        }

        bool cache_ready = !btransient;
        if (btransient && offline_can_write) {
            discogger_cache::directory_cache_transaction tx(
                std::filesystem::u8path(n8_rel_path.c_str()),
                ol::cache_payload_is_valid_json);
            if (tx.ready()) {
                bool saved = true;
                for (size_t page = 0; page < count; ++page) {
                    p_abort.check();
                    saved = discogs_interface->offline_cache_save(
                        ol::staged_cache_json_path(tx, page), pages[page]->root);
                    if (!saved) break;
                }
                if (saved) {
                    const pfc::string8 marker = JSONString(
                        json_object_get(pages[0]->root, "items"));
                    cache_ready = tx.publish(marker.c_str());
                }
            }
        }


        p_abort.check();
        master_releases = std::move(staged.master_releases);
        releases = std::move(staged.releases);
        search_order_master = std::move(staged.search_order_master);
        releases_loaded_from_disk = !btransient;
        loaded_releases_offline = cache_ready;
        releases_loaded_at = std::chrono::steady_clock::now();
        loaded_releases = true;
        releases_load_generation.fetch_add(1, std::memory_order_release);
	}
	catch (const exception_aborted&) {
        release_refresh.retry_after_failure(explicit_refresh);
		throw;
	}
	catch (foo_discogs_exception &e) {
        release_refresh.retry_after_failure(explicit_refresh);
		if (throw_all) {
			throw;
		}
		pfc::string8 error("Error loading artist releases (Artist id: ");
		error << id << ": " << name << ") " << e.what();
		throw foo_discogs_exception(error);
	}
	catch (...) {
        release_refresh.retry_after_failure(explicit_refresh);
		foo_discogs_exception ex;
		ex << "Unknown error loading releases.";
		throw ex;
	}
}

void Discogs::MasterRelease::load_releases(threaded_process_status &p_status, abort_callback &p_abort, bool throw_all, pfc::string8 offlineArtistId, db_fetcher* dbfetcher, int query_mode, bool isolated_versions) {
    // Both modeless and legacy callers use a private snapshot; the legacy
    // cache-backed parsing path could mutate child objects before page 2.
    (void)isolated_versions;

	if (loaded_releases || !id.get_length()) {
		return;
	}

	pfc::string8 artist_id = offlineArtistId;
	pfc::string8 master_id = id;


	bool offline_can_read = ol::can_read();
	bool offline_can_write = ol::can_write();
	bool offline_can_overwrite = ol::can_ovr();


	ol::GetFrom gfVersions = ol::GetFrom::Versions;
	bool expired = false;
	pfc::string8 n8_rel_path;
	bool offline_avail_data = ol::is_data_avail(artist_id, master_id, gfVersions, n8_rel_path, true, expired);

	pfc::string8 msg(PFC_string_formatter() << (offline_can_read && expired ? "(expired) " : "") << "loading MasterRelease ");
	msg << id << "...";
	p_status.set_item(msg);

	bool btransient = !(offline_can_read && offline_avail_data) || offline_can_overwrite;
	bool db_isready = false;
	btransient &= !db_isready;

	try {

		pfc::string8 url;
		pfc::array_t<JSONParser_ptr> pages;

		if (btransient) {
			url << "https://api.discogs.com/masters/" << id << "/versions";
			pages = discogs_interface->get_all_pages(url, "", p_abort, "Fetching master release versions...", p_status);
		}
		else {
            try {
                pages = discogs_interface->get_all_pages_offline_cache(gfVersions, artist_id, master_id, "", p_abort, "Fetching offline master releases versions ...", p_status);
                offline_can_write = false;
            }
            catch (const exception_aborted&) { throw; }
            catch (const std::exception&) {
                p_abort.check();
                btransient = true;
                url << "https://api.discogs.com/masters/" << id << "/versions";
                pages = discogs_interface->get_all_pages(url, "", p_abort, "Refreshing invalid offline master versions cache...", p_status);
            }
		}

		const size_t cPages = pages.get_count();
        if (!cPages) throw foo_discogs_exception("Master versions pages were empty; retaining previous results.");

        std::vector<json_t*> roots;
        roots.reserve(cPages);
        for (size_t page = 0; page < cPages; ++page) {
            p_abort.check();
            roots.push_back(pages[page]->root);
        }
        MasterRelease_ptr staged = stageMasterVersionPages(
            *this, roots, query_mode, [&p_abort]() { p_abort.check(); });
        if (btransient && offline_can_write) {
            discogger_cache::directory_cache_transaction tx(
                std::filesystem::u8path(n8_rel_path.c_str()),
                ol::cache_payload_is_valid_json);
            if (tx.ready()) {
                bool saved = true;
                for (size_t page = 0; page < cPages; ++page) {
                    p_abort.check();
                    saved = discogs_interface->offline_cache_save(
                        ol::staged_cache_json_path(tx, page), pages[page]->root);
                    if (!saved) break;
                }
                if (saved) {
                    const pfc::string8 marker = JSONString(
                        json_object_get(pages[0]->root, "items"));
                    tx.publish(marker.c_str());
                }
            }
        }
        // Only a fully parsed and cancellation-checked snapshot becomes
        // visible. Disk publication is independent; a failed offline save
        // never exposes a partially parsed master-version list.
        p_abort.check();
        sub_releases = std::move(staged->sub_releases);
		loaded_releases = true;
	}
	catch (foo_discogs_exception &e) {
		if (throw_all) {
			throw;
		}
		pfc::string8 error("Error loading master release versions");
		error << id << ": " << e.what();
		throw foo_discogs_exception(error);
	}
}

void Discogs::parseIdentity(json_t *root, Identity *identity) {

	assert_is_object(root);

	identity->user_id = JSONAttributeString(root, "id");
	identity->username = JSONAttributeString(root, "username");
	identity->consumer_name = JSONAttributeString(root, "consumer_name");

	pfc::string8 resource_url = JSONAttributeString(root, "resource_url");
}

void Discogs::parseCollection(json_t *root, pfc::array_t<pfc::string8> &collection) {

	assert_is_object(root);

	json_t *releases = json_object_get(root, "releases");
	assert_is_array(releases);

	for (size_t i = 0; i < json_array_size(releases); i++) {
		json_t *release = json_array_get(releases, i);
		if (json_is_object(release)) {
			json_t *info = json_object_get(release, "basic_information");
			if (json_is_object(info)) {
				collection.append_single(JSONAttributeString(info, "id"));
			}
		}
	}
}
