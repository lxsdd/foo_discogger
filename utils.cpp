#include "stdafx.h"

#include "foo_discogs.h"
#include "utils_path.h"
#include "utils.h"
#include <algorithm>

#ifdef _WIN64
size_t MAX_ARTISTS = 200;
#else
size_t MAX_ARTISTS = 63; //max - id size = 32bits-24bits = 6bites -> 2^6 - 1 = 63
#endif


const char *whitespace = " \t\r\n";

float cfg_find_release_colummn_showid_width = 50.0f;
bool cfg_find_release_colummn_showid = false;

dll_inflateInit2 dllinflateInit2 = NULL;
dll_inflate dllinflate = NULL;
dll_inflateEnd dllinflateEnd = NULL;

dll_deflateInit_ dlldeflateInit = NULL;
dll_deflate dlldeflate = NULL;
dll_deflateEnd dlldeflateEnd = NULL;

dll_adler32 dlladler32 = NULL;

HINSTANCE hGetProcIDDLL = NULL;

const int IMAGELIST_OFFLINE_CACHE_NDX = 8;
const int IMAGELIST_OFFLINE_DB_NDX = 9;

void init_scroolbars(HWND wnd) {

	::ShowScrollBar(wnd, SB_VERT, true);
	::ShowScrollBar(wnd, SB_HORZ, true);
	CRect rc;
	::GetWindowRect(wnd, rc);
	const SIZE sz = { rc.Width(), rc.Height() };

	SCROLLINFO si = {};
	si.cbSize = sizeof(SCROLLINFO);
	si.fMask = SIF_PAGE | SIF_POS | SIF_RANGE;
	si.nPos = si.nMin = 1;

	si.nMax = sz.cx;
	si.nPage = sz.cx;
	SetScrollInfo(wnd, SB_HORZ, &si, false);

	si.nMax = sz.cy;
	si.nPage = sz.cy;
	SetScrollInfo(wnd, SB_VERT, &si, false);
}

bool enable_multiline_list_input(HWND wnd, CFontHandle font, const float col_width, const pfc::string8 &str) {

	CWindowDC dc(wnd);
	SelectObjectScope fontScope(dc, font);
	SIZE sz = {};
	const auto wout = pfc::wideFromUTF8(str);
	WIN32_OP_D(dc.GetTextExtent(wout, str.get_length(), &sz));
	unsigned fixedWidth = MulDiv(sz.cx, 3, 2);

	return !(sz.cx < (col_width - 10));
}

//todo:
inline pfc::string EscapeWin(pfc::string8 keyWord) {
	pfc::string8 out_keyWord(keyWord);
	out_keyWord.replace_string("&", "&&");
	return out_keyWord;
}

inline bool remove_continous_space(pfc::string8 in, pfc::string8& out, std::regex &regex_v) {

	std::string tmpstr(in.c_str());

	if (regex_v._Empty()) {
		try {
			regex_v = std::regex("\\s*[ ]\\s*");
		}
		catch (std::regex_error e) {
			return false;
		}
	}

	try {
		tmpstr = std::regex_replace(tmpstr, regex_v, " ");
	}
	catch (std::regex_error e) {
		return false;
	}

	out.set_string(tmpstr.c_str());
	return tmpstr.length() != in.length();
}

std::string any_case_substring(std::string s, std::string a, const std::string& b)
{
	if (a.empty())
		return s;

	std::string res = s;
	std::transform(s.begin(), s.end(), s.begin(), ::tolower);
	std::transform(a.begin(), a.end(), a.begin(), ::tolower);

	size_t pos = s.rfind(a);
	while (pos != std::string::npos)
	{
		res.replace(res.begin() + pos, res.begin() + pos + a.length(), b);

		if (pos == 0)
			return res;

		pos = s.rfind(a, pos - 1);
	}

	return res;
}

pfc::string8 sanitize_track_semi_media(const pfc::string8& tracks) {

	pfc::string8 res;
	std::vector<pfc::string8> v_vols;
	split(tracks, ";", 0, v_vols); //split by volumes
	for (auto& walk_vol : v_vols) {
		//CD 1: ... ; CD 2: ... or CD: ...; LP: ... ?
		std::vector<pfc::string8> v_voldesc;
		split(walk_vol, ":", 0, v_voldesc); //split by :
		bool hasvol = v_voldesc.size() == 2;
		size_t volnum = ~0;
		pfc::string8 volspec = "";
		if (hasvol) {
			std::vector<pfc::string8> v_voltoken;
			split(trim(v_voldesc.at(0)), " ", 0, v_voltoken); //split by spc

			if (v_voltoken.size() == 2) {
				volnum = is_number(v_voltoken.at(1).c_str()) ? volnum = atoi(v_voltoken.at(1)) : pfc_infinite;
			}
			volspec = v_voltoken.at(0);
			walk_vol = "";

			//..

			std::vector<pfc::string8> v_trackranges;
			split(v_voldesc.at(1), ",", 0, v_trackranges); //split by commas
			for (auto& walk_trackranges : v_trackranges) {

				std::vector<pfc::string8> v_trackto;
				split(walk_trackranges, " to ", 0, v_trackto); //split by " to "
				pfc::string trk = "";
				//dont really need a loop here
				for (auto walk_trackto : v_trackto) {
					if (trk.get_length())
						walk_vol << " to ";

					if (volnum != ~0) {
						trk << volnum;
					}
					else {
						trk << volspec;
					}
					trk << "-" << trim(walk_trackto);
					// 1-1 to 1-4 or just 1-5
				}
				res << (res.get_length() ? "," : "") << trk;
			}
		}
		else {
			res << (res.get_length() ? "," : "") << walk_vol;
		}
	}
	return res;
}

pfc::string8 sanitize_track_commas(const pfc::string8& tracks, std::regex& regex_v) {

	if (regex_v._Empty()) {
		regex_v = std::regex("[ ]+,[ ]+");
	}

	std::string res_no_extra_comma_spc(tracks.c_str());
	std::sregex_iterator begin = std::sregex_iterator(res_no_extra_comma_spc.begin(), res_no_extra_comma_spc.end(), regex_v);
	std::sregex_iterator end = std::sregex_iterator();
	if (begin == end) return tracks;


	try {

		res_no_extra_comma_spc = std::regex_replace(res_no_extra_comma_spc.c_str(), regex_v, ", ");
	}
	catch (std::regex_error e) {
		return tracks;
	}

	pfc::string8 tracks_semied = sanitize_track_semi_media(res_no_extra_comma_spc.c_str());

	//split
	std::vector<pfc::string8>v;
	split(trim(tracks_semied), " ", 0, v);
	for (int i = 0; i < v.size(); i++) {
		if (i < v.size() - 2) {
			if (v.at(i).equals("to") && (v.at(i + 1).find_first(',', 0) == pfc_infinite)) {
				auto c = std::find(v.begin() + i, v.end(), ",");
				if (c == v.end() || (c - v.begin() + i > 1)) {
					v.at(i + 1) = PFC_string_formatter() << v.at(i + 1) << ",";
				}
			}
		}
		v.at(i).replace_string("to", " to ");
	}

	//rebuild
	pfc::string8 res;
	std::vector<pfc::string8>::iterator iter_forward = v.begin();
	for (auto w : v) {
		iter_forward++;
		size_t comma_pos = w.find_first(',', 0);
		if (comma_pos != pfc_infinite) {
			res << w << " ";
			continue;
		}
		if (w.equals(" to ")) {
			res << w;
			continue;
		}
		res << w << (iter_forward != v.end() && !iter_forward->equals(" to ") ? ", " : "");
	}
	return res;
}

pfc::string8 sanitize_track_to(const pfc::string8& tracks) {

	std::regex regex_v;

	regex_v = std::regex("[A - Za - z1 - 9 - ][^, ]{1,8} to [A - Za - z1 - 9 - ][^, ]{1,8}");

	std::string res_no_extra_comma_spc(tracks.c_str());
	std::sregex_iterator begin = std::sregex_iterator(res_no_extra_comma_spc.begin(), res_no_extra_comma_spc.end(), regex_v);
	std::sregex_iterator end = std::sregex_iterator();

	if (begin == end) {
		//EXIT
		return tracks;
	}

	int occurrences = 0;
	pfc::string8 res;
	std::sregex_iterator i = begin;
	if (i->prefix().matched) {
		res << i->prefix().str().c_str();
	}
	res << i->str().c_str();
	std::string str_suffix(trim(i->suffix().str().c_str()).c_str());
	if (i->suffix().matched && str_suffix.length()) {
		if (str_suffix.at(0) != ',') {
			res << ",";
		}
		//recursion
		res << sanitize_track_to(trim(i->suffix().str().c_str()));
	}
	return res;
}

void replace_track_volume_desc(const pfc::string& sometrack, nota_info& out, const std::vector<pfc::string8> vvoldescs) {

	pfc::string8 res;

	pfc::string8 sometrack_fixed = sometrack;
	pfc::string8 credits_disk_notation = "";
	pfc::string8 tracks_disk_notation = "";

	std::vector<pfc::string8> v_nota;
	split(sometrack_fixed, "-", 0, v_nota);

	if (is_number(v_nota.at(0).c_str())) {
		out.is_number = true;
		if (v_nota.size() < 2) {
			out.disk = 0;
			out.track = atoi(v_nota.at(0).c_str()) - 1;
		}
		else {
			out.disk = atoi(v_nota.at(0).c_str()) - 1;
			out.track = is_number(v_nota.at(1).c_str()) ? (atoi(v_nota.at(1).c_str()) - 1) : ~0;;
			out.is_number &= (out.track != ~0);
		}
		return;
	}
	else {
		out.is_number = false;
		out.disk = ~0;
		out.track = ~0;
	}
	return;
}

inline pfc::string8 trim(const pfc::string8 &str, const char *ch) {
	return ltrim(rtrim(str, ch), ch);
}

// TODO: get rid std::string?

inline pfc::string8 ltrim(const pfc::string8 &str, const char *ch) {
	std::string dest((const char*)str);
	dest.erase(0, dest.find_first_not_of(ch));
	return pfc::string8(dest.c_str());
}

inline pfc::string8 rtrim(const pfc::string8 &str, const char *ch) {
	std::string dest((const char*)str);
	dest.erase(dest.find_last_not_of(ch) + 1);
	return pfc::string8(dest.c_str());
}

extern void szcstr(size_t n, pfc::string8& out) {
	out = std::to_string(n).c_str();
}

bool is_number(const std::string& s)
{
	return !s.empty() && std::find_if(s.begin(),
		s.end(), [](unsigned char c) { return !std::isdigit(c); }) == s.end();
}

bool replace_last_alpha_by_dot(std::string& s) {
	auto fit_any_alpha = std::find_if(s.rbegin(),
		s.rend(), [](unsigned char c) { return std::isdigit(c) == false; });
	if (fit_any_alpha.base() != s.begin() && fit_any_alpha.base() != s.end()) {
 		size_t dist = std::distance(s.begin(), fit_any_alpha.base());
		s.replace(dist - 1, 1, ".");
		return true;
	}
	return false;
}

size_t split(pfc::string8 str, pfc::string8 token, size_t index, std::vector<pfc::string8>& out) {
	size_t last_index = index;

	index = str.find_first(token, index);
	if (index != pfc_infinite) {
		out.push_back(pfc::string_part(str + last_index, index - last_index));
		//recursion
		return split(str, token, index + token.length(), out);
	}
	else {
		pfc::string8 notoken;
		if ((notoken = substr(str, last_index, str.get_length())).get_length())
			out.push_back(notoken);
		return out.size();
	}
}

size_t encode_mr(const int a, const unsigned long b) {
	//search artists array ndx (last 6)
	size_t enc_a = (size_t)a << 26;
	//master id (first 26)
	size_t coded = enc_a | b;
	return coded;
}

size_t encode_mr(const int a, const pfc::string8& sb) {
	return encode_mr(a, atoi(sb));
}

std::pair<int,unsigned long> decode_mr(const size_t coded) {
#ifdef _WIN64
	size_t dec_a = (coded >> 26);
	dec_a &= (1ULL << 38/*(64-26)*/) - 1;
	//master id (first 26)
	unsigned long dec_b = coded & ((1 << 26) - 1);
#else
	size_t dec_a = (coded >> 26) & ((1 << (32-26)) - 1);
	unsigned long dec_b = coded & ((1 << 26) - 1);
#endif

	return std::pair((int)dec_a, dec_b);
}

pfc::string8 lowercase(pfc::string8 str) {
	std::string result((const char*)str);
	std::transform(result.begin(), result.end(), result.begin(), ::tolower);
	return pfc::string8(result.c_str());
}

pfc::string8 join(const pfc::array_t<pfc::string8> &in, const pfc::string8 &join_field) {
	pfc::string8 out = "";
	const size_t count = in.get_count();
	for (size_t i = 0; i < count; i++) {
		out << in[i];
		if (i != count - 1) {
			out << join_field;
		}
	}
	return out;
}

extern bool remove_parenthesized(pfc::string8& src) {
	if (!src.get_length()) return 0;

	bool bdone = false;
	size_t bb = src.find_first("(");
	if (bb < src.get_length()) {
		size_t be = src.find_last(")");
		if (be < src.get_length() && (be > bb)) {
			bdone = true;
			src = substr(src, 0, bb);
		}
	}

	return bdone;
}

int tokenize_quoted(pfc::string8& src, pfc::array_t<pfc::string8>& tokens) {

	tokens.force_reset();
	if (!src.get_length()) return 0;

	std::regex regex_v;
	try {
		regex_v = std::regex("\"[^\"]*\"");
	}
	catch (std::regex_error e) {
		return 0;
	}

	std::string str_with_quotes(src.c_str());
	std::sregex_iterator begin = std::sregex_iterator(str_with_quotes.begin(), str_with_quotes.end(), regex_v);
	std::sregex_iterator end = std::sregex_iterator();

	bool bdone = false;
	for (std::sregex_iterator i = begin; i != end; i++) {
		bdone |= true;
		pfc::string8 token = i->str().c_str();
		tokens.append_single(token.replace("\"", ""));
	}

	try {
		src = std::regex_replace(src.c_str(), regex_v, "").c_str();
	}
	catch (std::regex_error e) {
		return 0;
	}

	if (bdone) {
		remove_continous_space(src, src);
	}
	return (int)tokens.get_size();
}

int tokenize(const pfc::string8 &src, const pfc::string8 &delim, pfc::array_t<pfc::string8> &tokens, bool remove_blanks, bool reset_tokens) {

	if (reset_tokens) {
		tokens.force_reset();
	}
	if (!src.get_length()) return 0;

	size_t pos;
	pfc::string8 tmp = src;
	size_t dlength = delim.get_length();
	while ((pos = tmp.find_first(delim)) != pfc::infinite_size) {
		pfc::string8 token = substr(tmp, 0, pos);
		if (remove_blanks) {
			token = trim(token);
		}
		tokens.append_single(token);
		tmp = substr(tmp, pos + dlength, tmp.length() - 1);
	}
	if (remove_blanks) {
		tmp = trim(tmp);
	}
	if (tmp.get_length()) {
		tokens.append_single(tmp);
	}
	return (int)tokens.get_size();
}

bool replace_bracketed_commas(pfc::string8& out, pfc::string8 what, pfc::string8 with) {

	//all bracketed commas
	std::regex regex_v;
	std::string str_exp(what);
	str_exp.append("(?=((?!\\[).)*?\\])");

	bool regex_ok = vregex_util_get(str_exp.c_str(), regex_v, true);
	if (!regex_ok) {
		return false;
	}

	std::string res_no_bracketed_commas(out.c_str());
	std::sregex_iterator begin = std::sregex_iterator(res_no_bracketed_commas.begin(), res_no_bracketed_commas.end(), regex_v);
	std::sregex_iterator end = std::sregex_iterator();
	if (begin != end) {


		try {
			res_no_bracketed_commas = std::regex_replace(res_no_bracketed_commas.c_str(), regex_v, with.c_str());
			out = res_no_bracketed_commas.c_str();
			return true;
		}
		catch (std::regex_error e) {
			return false;
		}

	}
	return false;
}

int tokenize_non_bracketed(const pfc::string8& src, const pfc::string8& delim, pfc::array_t<pfc::string8>& tokens, bool remove_blanks) {

	tokens.force_reset();
	if (!src.get_length()) return 0;

	pfc::string8 tmp_src(src);

	if (tmp_src.find_first('[') != pfc_infinite) {
		replace_bracketed_commas(tmp_src, ",", "%");
	}

	size_t pos;
	pfc::string8 tmp = tmp_src;
	size_t dlength = delim.get_length();

	while ((pos = tmp.find_first(delim)) != pfc::infinite_size) {
		pfc::string8 token = substr(tmp, 0, pos);
		if (remove_blanks) {
			token = trim(token);
		}

		if (token.find_first('[') != pfc_infinite) {
			replace_bracketed_commas(token, "%", ",");
		}

		tokens.append_single(token);
		tmp = substr(tmp, pos + dlength, tmp.length() - 1);
	}
	if (remove_blanks) {
		tmp = trim(tmp);
	}

	if (tmp.find_first('[') != pfc_infinite) {
		//last or unique token
		replace_bracketed_commas(tmp, "%", ",");
	}
	tokens.append_single(tmp);
	return (int)tokens.get_size();
}

int tokenize_multi(const pfc::string8 &src, const pfc::array_t<pfc::string8> &delims, pfc::array_t<pfc::string8> &tokens, bool trim_tokens, bool remove_blanks) {
	size_t pos, pos2, dlen;
	tokens.force_reset();
	pfc::string8 tmp = src;
	const size_t dcount = delims.get_size();
	while (true) {
		pos = pfc::infinite_size;
		for (size_t i = 0; i < dcount; i++) {
			pos2 = tmp.find_first(delims[i]);
			if (pos2 < pos) {
				pos = pos2;
				dlen = delims[i].get_length();
			}
		}
		if (pos == pfc::infinite_size) {
			break;
		}
		pfc::string8 token = substr(tmp, 0, pos);
		if (trim_tokens) {
			token = trim(token);
		}
		if (!remove_blanks || token.get_length()) {
			tokens.append_single(token);
		}
		tmp = substr(tmp, pos + dlen, tmp.length() - 1);
	}
	if (trim_tokens) {
		tmp = trim(tmp);
	}
	if (!remove_blanks || tmp.get_length()) {
		tokens.append_single(tmp);
	}
	return (int)tokens.get_size();
}

bool tokenize_filter(pfc::string8 filter, pfc::array_t<pfc::string>& out_filter_words_lowercase) {
	pfc::array_t<pfc::string8> filter_words;
	tokenize_quoted(filter, filter_words);
	tokenize(filter, " ", filter_words, true, false);

	for (size_t i = 0; i < filter_words.get_size(); i++) {
		out_filter_words_lowercase.append_single(pfc::string(filter_words[i].get_ptr()).toLower());
	}
	return out_filter_words_lowercase.get_count() > 0;
}

void makeFsCompliant(pfc::string8 &str) {
	for (t_size n = 0; n < str.get_length(); n++)
	{
		auto s = &(str[n]);
		if (*s == '\\' || *s == '/' || *s == ':' || *s == '*' || *s == '?' || *s == '"' || *s == '<' || *s == '>' || *s == '|') {
			str.set_char(n, '_');
		}
	}
}

pfc::string8 urlEscape(const pfc::string8 &src) {
	pfc::string8 dst = "";
	for (size_t i = 0; i < src.get_length(); i++) {
		switch (src[i]) {
			case ' ': dst << "%20"; break;
			case '<': dst << "%3C"; break;
			case '>': dst << "%3E"; break;
			case '#': dst << "%23"; break;
			case '%': dst << "%25"; break;
			case '+': dst << "%2B"; break;
			case '{': dst << "%7B"; break;
			case '}': dst << "%7D"; break;
			case '|': dst << "%7C"; break;
			case '\\': dst << "%5C"; break;
			case '^': dst << "%5E"; break;
			case '~': dst << "%7E"; break;

			case '[': dst << "%5B"; break;
			case ']': dst << "%5D"; break;
			case '`': dst << "%60"; break;
			case ';': dst << "%3B"; break;
			case '/': dst << "%2F"; break;
			case '?': dst << "%3F"; break;
			case ':': dst << "%3A"; break;
			case '@': dst << "%40"; break;
			case '=': dst << "%3D"; break;
			case '&': dst << "%26"; break;
			case '$': dst << "%24"; break;

			case '*': dst << "%2A"; break;
			case '(': dst << "%28"; break;
			case ')': dst << "%29"; break;
			case ',': dst << "%2C"; break;
			case '\'': dst << "%27"; break;
			case '"': dst << "%22"; break;
			case '!': dst << "%21"; break;

			default:
				if (src[i] < 0) {
					unsigned char c = src[i];
					char buf[16];
					sprintf(buf, "%%%X", (unsigned int)c);
					dst << buf;
				}
				else {
					dst.add_char(src[i]);
					break;
				}
		}
	}
	return dst;
}


void urldecode2(char* dst, const char* src)
{
	char a, b;
	while (*src) {
		if ((*src == '%') &&
			((a = src[1]) && (b = src[2])) &&
			(isxdigit(a) && isxdigit(b))) {
			if (a >= 'a')
				a -= 'a' - 'A';
			if (a >= 'A')
				a -= ('A' - 10);
			else
				a -= '0';
			if (b >= 'a')
				b -= 'a' - 'A';
			if (b >= 'A')
				b -= ('A' - 10);
			else
				b -= '0';
			*dst++ = 16 * a + b;
			src += 3;
		}
		else if (*src == '+') {
			*dst++ = ' ';
			src++;
		}
		else {
			*dst++ = *src++;
		}
	}
	*dst++ = '\0';
}

void display_url(const pfc::string8 &url) {
	pfc::stringcvt::string_wide_from_ansi wurl(url.get_ptr());

	SHELLEXECUTEINFO sinfo;
	memset(&sinfo, 0, sizeof(sinfo));
	sinfo.cbSize = sizeof(sinfo);
	sinfo.fMask = SEE_MASK_CLASSNAME;
	sinfo.lpVerb = _T("open");
	sinfo.lpFile = wurl;
	sinfo.nShow = SW_SHOWNORMAL;
	sinfo.lpClass = _T("https");

	if (!ShellExecuteEx(&sinfo)) {
		DWORD err = GetLastError();
		if (err != 0) {
			LPSTR messageBuffer = nullptr;
			FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, nullptr);

			foo_discogs_exception ex;
			ex << "Error opening browser page: " << wurl << messageBuffer;

			LocalFree(messageBuffer);

			throw ex;
		}
	}
}


// ROMAN

// Function to convert a Roman numeral to an integer
int roman_to_integer(pfc::string8 rstr) {
	int n = int(rstr.length());
	if (0 == n) {
		return 0;
	}

	// Initialize the result
	int result = 0;

	// Iterate through each character in the Roman numeral string
	for (int i = 0; i < n; i++) {
		// Switch statement to handle different Roman numeral characters
		switch (rstr[i]) {
		case 'I':
			result += 1;
			break;
		case 'V':
			result += 5;
			break;
		case 'X':
			result += 10;
			break;
		case 'L':
			result += 50;
			break;
		case 'C':
			result += 100;
			break;
		case 'D':
			result += 500;
			break;
		case 'M':
			result += 1000;
			break;
		}
	}

	// Loop to handle subtraction cases (like IV, IX, XL, XC, etc.)
	for (int i = 1; i < n; i++) {
		if ((rstr[i] == 'V' || rstr[i] == 'X') && rstr[i - 1] == 'I') {
			result -= 1 + 1; // Subtract 1 for I and add 1 for V or X
		}

		if ((rstr[i] == 'L' || rstr[i] == 'C') && rstr[i - 1] == 'X') {
			result -= 10 + 10; // Subtract 10 for X and add 10 for L or C
		}

		if ((rstr[i] == 'D' || rstr[i] == 'M') && rstr[i - 1] == 'C') {
			result -= 100 + 100; // Subtract 100 for C and add 100 for D or M
		}
	}

	return result; // Return the resulting integer
}

bool validation_of_roman_number(std::string str)
{
	if (str.empty()) return false;
	const std::regex pattern("^M{0,3}(CM|CD|D?C{0,3})(XC|XL|L?X{0,3})(IX|IV|V?I{0,3})$");

	if (std::regex_match(str, pattern)) {
		return true;
	}
	else {
		return false;
	}
}

std::unordered_map<char, int> kmRomanToNumber = { {'I', 1},
																									{'V', 5},
																									{'X', 10},
																									{'L', 50},
																									{'C', 100},
																									{'D', 500},
																									{'M', 1000} };

int RomanToDecimal(const pfc::string8& s) {

	int res = 0;
	for (int i = 0; i < s.length(); i++) {

		// if the current value is less than the next value, 
		// subtract current from next and add to res
		if (i + 1 < s.length() && kmRomanToNumber[s[i]] < kmRomanToNumber[s[i + 1]]) {
			res += kmRomanToNumber[s[i + 1]] - kmRomanToNumber[s[i]];

			// skip the next symbol
			i++;
		}
		else {

			// otherwise, add the current value to res
			res += kmRomanToNumber[s[i]];
		}
	}

	return res;
}

const std::unordered_map<std::string_view, size_t> kEnglishToNumberMap = {
		{"zero", 0},
		{"one", 1},
		{"two", 2},
		{"three", 3},
		{"four", 4},
		{"five", 5},
		{"six", 6},
		{"seven", 7},
		{"eight", 8},
		{"nine", 9},
		{"ten", 10},
		{"eleven", 11},
		{"twelve", 12},
		{"thirteen", 13},
		{"fourteen", 14},
		{"fifteen", 15},
		{"sixteen", 16},
		{"seventeen", 17},
		{"eighteen", 18},
		{"nineteen", 19},
		{"twenty", 20},
		{"thirty", 30},
		{"forty", 40},
		{"fifty", 50},
		{"sixty", 60},
		{"seventy", 70},
		{"eighty", 80},
		{"ninety", 90},
		{"hundred", 100},
		{"thousand", 1000},
		//{"million", 1000000},
		//{"billion", 1000000000},
		// ... Add more mappings here
};

size_t EnglishToDecimal(const std::string& s) {
	size_t res, trot_num;
	res = 0;
	trot_num = SIZE_MAX;

	pfc::string8 str8(s.c_str());
	std::istringstream ss(str8.toLower().c_str());
	std::string word;


	bool bneg = false;

	while (ss >> word) {
		if (word == "and") {
			continue;
		}
		if (word == "minus" || word == "negative") {
			/*bneg = true;
			continue;*/
			return SIZE_MAX;
		}
		auto it = kEnglishToNumberMap.find(word);
		if (it != kEnglishToNumberMap.end()) {

			if (trot_num == SIZE_MAX) { trot_num = 0; }

			if (it->second >= 100) {
				trot_num *= it->second;
			}
			else {
				trot_num += it->second;
			}
		}
		if (trot_num >= 1000) {
			res += trot_num;
			trot_num = 0;
		}
	}

	res += trot_num;
	if (bneg) {
		res *= -1;
	}
	return res;
}


pfc::string8 substr(const pfc::string8 &s, size_t start, size_t count) {
	return pfc::string8(s.get_ptr() + start, count);
}

//modes: 'a' 'r' 'w' for artist and release refs, and web links
pfc::string8 extract_max_number(const pfc::string8& s, const char mode, const bool once) {

	std::regex regex_v; // ("[\\d]+");

	size_t prefix_len;
	size_t postfix_len;
	switch (mode) {
	case 'a':
		prefix_len = 2;
		postfix_len = 1;
		vregex_util_get("\\[a\\d+?\\]", regex_v, true);
		break;
	case 'r':
		prefix_len = 2;
		postfix_len = 1;
		vregex_util_get("\\[r\\d+?\\]", regex_v, true);
		break;
	case 'z':
		prefix_len = 0;
		postfix_len = 0;
		vregex_util_get("[\\d]", regex_v, true);
		break;
	case 'w':
		[[fallthrough]];
	default:
		prefix_len = 1;
		postfix_len = 0;
		vregex_util_get("/[\\d]+", regex_v, true);
	}

	size_t max = 0;
	std::string str(s);
	std::sregex_iterator begin = std::sregex_iterator(str.begin(), str.end(), regex_v);
	std::sregex_iterator end = std::sregex_iterator();

	for (std::sregex_iterator i = begin; i != end; i++) {
		const std::string_view sv{
			str.data() + i->position() + prefix_len,
			i->str().length() - (prefix_len + postfix_len)
		};
		if (size_t ival = atoi(sv.data()); ival > max) {
			max = ival;
			if (once) break;
		}
	}
	return max ? std::to_string(max).c_str() : "";
}

pfc::string8 extract_musicbrainz_mib(const pfc::string8& s) {

	std::regex regex_rmb("[0-9a-f]+\\-[0-9a-f]+\\-[0-9a-f]+\\-[0-9a-f]+\\-[0-9a-f]+");

	std::smatch match;
	std::string str(s);
	if (std::regex_search(str, match, regex_rmb)) {
		return pfc::string8(match.str().c_str());
	}
	return "";
}

void load_dlls()
{
	hGetProcIDDLL = LoadLibrary(L"zlib1.dll");
	if (!hGetProcIDDLL) {
		console::print("Error loading zlib1.dll");
		return;
	}
	dllinflateInit2 = (dll_inflateInit2)GetProcAddress(hGetProcIDDLL, "inflateInit2_");
	dllinflate = (dll_inflate)GetProcAddress(hGetProcIDDLL, "inflate");
	dllinflateEnd = (dll_inflateEnd)GetProcAddress(hGetProcIDDLL, "inflateEnd");
	dlladler32 = (dll_adler32)GetProcAddress(hGetProcIDDLL, "adler32");
}

void unload_dlls()
{
	if (hGetProcIDDLL) {
		FreeLibrary(hGetProcIDDLL);
	}
	hGetProcIDDLL = NULL;
}


// replacement for zlib uncompress() that deals with 
int myUncompress(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen) {
	z_stream stream;
	int err;

	stream.next_in = (Bytef*)source;
	stream.avail_in = (uInt)sourceLen;
	// Check for source > 64K on 16-bit machine:
	if ((uLong)stream.avail_in != sourceLen) {
		return Z_BUF_ERROR;
	}

	stream.next_out = dest;
	stream.avail_out = (uInt)*destLen;
	if ((uLong)stream.avail_out != *destLen) {
		return Z_BUF_ERROR;
	}

	stream.zalloc = (alloc_func)nullptr;
	stream.zfree = (free_func)nullptr;

	err = (*dllinflateInit2)(&stream, 15 + 32, ZLIB_VERSION, (int)sizeof(z_stream));
	if (err != Z_OK) {
		return err;
	}

	err = (*dllinflate)(&stream, Z_FINISH);
	if (err != Z_STREAM_END) {
		(*dllinflateEnd)(&stream);
		if (err == Z_NEED_DICT || (err == Z_BUF_ERROR && stream.avail_in == 0)) {
			return Z_DATA_ERROR;
		}
		return err;
	}
	*destLen = stream.total_out;

	err = (*dllinflateEnd)(&stream);
	return err;
}

void ensure_directory_exists(const char* dir) {
	abort_callback_impl _abort;

	pfc::string8 path = pfc::string8(dir);
	int pos;
	pfc::array_t<pfc::string8> missing;

	try {
		while (true) {
			if (!foobar2000_io::filesystem::g_exists(path.get_ptr(), _abort)) {
				missing.append_single(path);
			}
			else {

				for (int i = missing.get_size(); i > 0; i--) {
					pfc::string8 make = missing[i - 1];
					pfc::string8 msg("creating directory: ");
					msg << make;
					log_msg(msg);

					foobar2000_io::filesystem::g_create_directory(make.get_ptr(), _abort);

					if (!foobar2000_io::filesystem::g_exists(make.get_ptr(), _abort)) {
						foo_discogs_exception ex;
						ex << "Error creating directory: \"" << make.get_ptr() << "\"";
						throw ex;
					}
				}
				return;
			}

			if (path[path.length() - 1] == '\\') {
				path = substr(path, 0, path.length() - 2);
			}
			if ((pos = path.find_last("\\")) == pfc::infinite_size) {
				break;
			}
			else {
				path = substr(path, 0, pos + 1);
			}
		}
	}
	catch (foobar2000_io::exception_io &e) {
		foo_discogs_exception ex;
		ex << "Error creating directory: \"" << dir << "\" [" << e.what() << "]";
		throw ex;
	}
}

bool check_os_win_eleven() {
	OSVERSIONINFO ver = { sizeof(ver) };
	WIN32_OP_D(GetVersionEx(&ver));
	if (ver.dwMajorVersion == 10 && ver.dwBuildNumber >= 22000)
		return true;
	else
		return false;
}

// wine colors

bool is_dark_luminance(COLORREF fore, COLORREF back) {

	const uint8_t Rf = GetRValue(fore);
	const uint8_t Gf = GetGValue(fore);
	const uint8_t Bf = GetBValue(fore);

	const uint8_t Rb = GetRValue(back);
	const uint8_t Gb = GetGValue(back);
	const uint8_t Bb = GetBValue(back);

	const double rgf = Rf <= 10 ? Rf / 3294.0 : std::pow((Rf / 269.0) + 0.0513, 2.4);
	const double ggf = Gf <= 10 ? Gf / 3294.0 : std::pow((Gf / 269.0) + 0.0513, 2.4);
	const double bgf = Bf <= 10 ? Bf / 3294.0 : std::pow((Bf / 269.0) + 0.0513, 2.4);

	double lum_fore = (1 - 0.2126 * rgf) + (1 - 0.7152 * ggf) + (1 - 0.0722 * bgf);

	const double rgb = Rb <= 10 ? Rb / 3294.0 : std::pow((Rb / 269.0) + 0.0513, 2.4);
	const double ggb = Gb <= 10 ? Gb / 3294.0 : std::pow((Gb / 269.0) + 0.0513, 2.4);
	const double bgb = Bb <= 10 ? Bb / 3294.0 : std::pow((Bb / 269.0) + 0.0513, 2.4);

	double lum_back = (1 - 0.2126 * rgb) + (1 - 0.7152 * ggb) + (1 - 0.0722 * bgb);
	return lum_fore < lum_back;
}

using Tokens = std::vector<std::string>;

inline Tokens StringSplit(const std::string& text, int separator) {
	Tokens vs(text.empty() ? 0 : 1);
	for (const char ch : text) {
		if (ch == separator) {
			vs.emplace_back();
		}
		else {
			vs.back() += ch;
		}
	}
	return vs;
}

LONG GetStringRegKey(HKEY hKey, const std::wstring& strValueName, std::wstring& strValue, const std::wstring& strDefaultValue)
{
	strValue = strDefaultValue;
	WCHAR szBuffer[512];
	DWORD dwBufferSize = sizeof(szBuffer);
	ULONG nError;
	nError = RegQueryValueExW(hKey, strValueName.c_str(), 0, NULL, (LPBYTE)szBuffer, &dwBufferSize);
	if (ERROR_SUCCESS == nError)
	{
		strValue = szBuffer;
	}
	return nError;
}

#ifndef W2U
#define W2U(Text) pfc::stringcvt::string_utf8_from_wide(Text).get_ptr()
#endif

bool CheckDarkLuminance()
{
	bool check_dark = false;

	HKEY openKey = HKEY_CURRENT_USER;
	const char* regkey = "Control Panel\\Colors";

	HKEY hKey = 0;
	TCHAR wregkey[MAX_PATH] = { 0 };
	/*auto wlen = */pfc::stringcvt::convert_utf8_to_wide(wregkey, MAX_PATH, regkey, strlen(regkey));
	//CSTR lpSubKey
	LONG retValue = RegOpenKeyEx(openKey, wregkey, 0, KEY_READ/*KEY_WOW64_32KEY*/, &hKey);
	//#endif

	std::wstring strValueOfButtonFace;
	std::wstring strValueOfButtonText;

	bool done = GetStringRegKey(hKey, L"ButtonFace", strValueOfButtonFace, L"") == ERROR_SUCCESS;
	done &= GetStringRegKey(hKey, L"ButtonText", strValueOfButtonText, L"") == ERROR_SUCCESS;

	if (done) {

		pfc::string8 buf = W2U(strValueOfButtonFace.c_str()/*, strValueOfButtonFace.length()*/);
		auto tokens = StringSplit(buf.c_str(), ' ');

		COLORREF back = RGB(atoi(tokens[0].c_str()), atoi(tokens[1].c_str()), atoi(tokens[2].c_str()));

		buf = W2U(strValueOfButtonText.c_str()/*, strValueOfButtonText.length()*/);
		tokens = StringSplit(buf.c_str(), ' ');

		COLORREF fore = RGB(atoi(tokens[0].c_str()), atoi(tokens[1].c_str()), atoi(tokens[2].c_str()));

		check_dark = is_dark_luminance(fore, back);
	}

	RegCloseKey(hKey);
	return check_dark;
}

pfc::string8 check_os_wine() {

	pfc::string8 wine_ver = "";

	HMODULE hntdll = GetModuleHandle(L"ntdll.dll");
	if (!hntdll)
	{
		puts("Not running on NT.");
		return "";
	}

	FARPROC fpwine_get_version = GetProcAddress(hntdll, "wine_get_version");
	auto pcwine_get_version = reinterpret_cast<char* (CDECL * )(void)>(fpwine_get_version);

	if (pcwine_get_version)
	{
		wine_ver = PFC_string_formatter() << pcwine_get_version();
		return wine_ver;
	}
	else
	{
		return "";
	}
}

bool check_os_wine_ver_after(int amajor, int aminor, pfc::string8 currver) {
	int major = 0;
	int minor = 0;
	std::vector<pfc::string8> vsplit;
	split(currver, ".", 0, vsplit);
	if (vsplit.size() < 2) return false;
	if (vsplit.size() > 0) major = std::stoi(vsplit[0].c_str());
	if (vsplit.size() > 1) minor = std::stoi(vsplit[1].c_str());
	//bool bres = check_os_wine();
	if (major > amajor)
		return true;
	else if (major < amajor)
		return false;

	if (minor > aminor)
		return true;
	else if (minor < aminor)
		return false;

	return false;
}

bool check_os_wine_dark_no_theme() {

	if (!IsWine()) return false;

	bool is_wine_dark_no_theme = true;

	TCHAR theme_name[MAX_PATH] = { 0 };
	HRESULT hres = GetCurrentThemeName(theme_name, MAX_PATH, nullptr, 0, nullptr, 0);
	if (hres == S_OK) {
		hres = GetThemeDocumentationProperty(theme_name,
			SZ_THDOCPROP_DISPLAYNAME, theme_name, MAX_PATH);
	}
	pfc::string8 str_cvt = pfc::stringcvt::string_utf8_from_os(theme_name, MAX_PATH).get_ptr();

	is_wine_dark_no_theme &= !str_cvt.length();

	if (is_wine_dark_no_theme) {
		is_wine_dark_no_theme &= CheckDarkLuminance();
	}

	return is_wine_dark_no_theme;
}

COLORREF get_hyper_link_color(bool is_dark) {

	COLORREF link_color;

	bool bv2 = core_version_info_v2::get()->test_version(2, 0, 0, 0);

	if (bv2) {
		if (g_os_is_wine) {

			if (is_dark) {
				//old versions with wine dark mode
				link_color = is_dark ? GetSysColor(COLOR_HIGHLIGHT) : GetSysColor(COLOR_HOTLIGHT);
			}
			else {
				//newer versions dark mode with winecfg (isDark() = false)
				link_color = ui_config_manager::get()->getSysColor(COLOR_HOTLIGHT);
			}
		}
		else {
			link_color = is_dark ? GetSysColor(COLOR_HIGHLIGHT) : GetSysColor(COLOR_HOTLIGHT);
		}
	}
	else {
		link_color = GetSysColor(COLOR_HOTLIGHT);
	}

	return link_color;
}

bool subclass_hyper_link_help_syntax(CHyperLink& help_link, HWND hWnd, bool is_dark) {

	help_link.SetHyperLinkExtendedStyle(0, HLINK_UNDERLINED);
	help_link.SetHyperLinkExtendedStyle(HLINK_NOTUNDERLINED, HLINK_NOTUNDERLINED);
	help_link.SetHyperLinkExtendedStyle(HLINK_SINGLELINE, HLINK_SINGLELINE);
	help_link.SetHyperLinkExtendedStyle(HLINK_NOTOOLTIP, HLINK_NOTOOLTIP);

	help_link.SubclassWindow(hWnd);

	COLORREF link_color = get_hyper_link_color(is_dark);

	help_link.m_clrLink = link_color;
	help_link.m_clrVisited = link_color;

	pfc::string8 url = profile_usr_components_path();
	url << "\\" << "foo_discogs_help.html";

	pfc::stringcvt::string_wide_from_utf8 wtext(url.get_ptr());
	help_link.SetHyperLink((LPCTSTR)const_cast<wchar_t*>(wtext.get_ptr()));
	return hWnd;
}

void CustomFont(HWND hwndParent, size_t flag, bool check_font, bool apply) {

	if (!flag) {
		if (check_font) {
			g_hFont = (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
			g_hFontTabs = (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
		}
	}
	else if (flag & (1 << 0)) {
		//expanded
		if (check_font) {
			LOGFONTW lf;
			CWindowDC dc(core_api::get_main_window());
			CTheme wtheme;
			HTHEME theme = wtheme.OpenThemeData(core_api::get_main_window(), L"TEXTSTYLE");
			GetThemeFont(theme, dc, TEXT_EXPANDED, 0, TMT_FONT, &lf);
			g_hFont = CreateFontIndirectW(&lf);
			g_hFontTabs = CreateFontIndirectW(&lf);
		}
	}
	else if (flag & (1 << 1)) {

		if (check_font) {
			bool bv2 = core_version_info_v2::get()->test_version(2, 0, 0, 0);
			if (bv2) {
				auto ui_cfg_mng = ui_config_manager::get();
				if (!g_hFont) {
					g_hFont = ui_cfg_mng->query_font(ui_font_lists);
				}
				if (!g_hFontTabs) {
					g_hFontTabs = ui_cfg_mng->query_font(ui_font_tabs);
				}
			}
		}
	}

	if (apply) {
		for (HWND walk = ::GetWindow(hwndParent, GW_CHILD); walk != NULL; ) {
			wchar_t buffer[128] = {};
			::GetClassName(walk, buffer, (int)(std::size(buffer) - 1));
			const wchar_t* cls = buffer;
			HWND next = ::GetWindow(walk, GW_HWNDNEXT);
			if (next && ::IsWindow(next)) {
				if (g_hFont && (((_wcsnicmp(cls, L"libPPUI:", 8) == 0) || (_wcsnicmp(cls, L"SysTreeV", 8) == 0)))) {
					CWindow cwnd(walk);
					cwnd.SetFont(g_hFont);
				}
				else if (g_hFontTabs && (_wcsnicmp(cls, L"SysTabControl32", 15) == 0)) {
					CWindow cwnd(walk);
					cwnd.SetFont(g_hFontTabs);
				}
			}
			walk = next;
		}
	}
}

bool sortByVal(const std::pair<int, int>& a, const std::pair<int, int>& b)
{
	if (a.second == b.second)
		return a.first > b.first;
	return a.second < b.second;
}

int duration_in_seconds(pfc::string8 duration) {
	int duration_seconds;
	size_t pos = min(duration.find_first(':'), duration.find_first('.'));
	size_t pos2 = min(duration.find_first(':', pos + 1), duration.find_first('.', pos + 1));
	if (pos2 != pfc::infinite_size) {
		pfc::string8 hour = substr(duration, 0, pos);
		pfc::string8 min = substr(duration, pos + 1, pos2);
		pfc::string8 sec = substr(duration, pos2 + 1);
		duration_seconds = (3600 * atoi(hour.get_ptr()) + 60 * atoi(min.get_ptr()) + atoi(sec.get_ptr()));
	}
	else if (pos != pfc::infinite_size) {
		pfc::string8 min = substr(duration, 0, pos);
		pfc::string8 sec = substr(duration, pos + 1);
		duration_seconds = (60 * atoi(min.get_ptr()) + atoi(sec.get_ptr()));
	}
	else {
		duration_seconds = 0;
	}
	return duration_seconds;
}

extern bool sanitaze_track_title(pfc::string8& out) {
	auto ext = filesystem::g_get_extension(out);
	if (ext.get_length()) {
		out = out.subString(0, out.get_length() - ext.get_length() - 1 /*dot*/);
	}

	size_t cp = 0;
	while (pfc::char_is_numeric(out[cp]) || out[cp] == '.'
		|| out[cp] == '-' || out[cp] == ' ') {
		++cp;
	}
	out = out.subString(cp == out.get_length() ? 0 : cp);

	return true;
}