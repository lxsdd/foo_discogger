#include "stdafx.h"

#include "fetcher.h"
#include "http_status_util.h"
#include "interruptible_wait.h"
#include "exception.h"
#include "utils.h"

#if defined(DISCOGGER_RUNNER_MEDIA_QA)
// Stable, non-secret identity for protected native Discogs read-only requests.
char USER_AGENT[] = "User-Agent: foobar2000-discogger-protected-qa/1.0 (+https://github.com/lxsdd/foo_discogger)";
#else
char USER_AGENT[] = "User-Agent: <<user agent>>";
#endif

const double RL_AVG_THRESHOLD = 0.5;
const size_t RL_REMAINING_THRESHOLD = 30;

#include <iomanip>
#include <optional>
#include <ostream>
#include <iostream>
#include <cstdlib>
#include <cstring>

std::ostream& operator<<(std::ostream& os, std::chrono::nanoseconds ns)
{
	using namespace std::chrono;
	using days = duration<int, std::ratio<86400>>;
	auto d = duration_cast<days>(ns);
	ns -= d;
	auto h = duration_cast<hours>(ns);
	ns -= h;
	auto m = duration_cast<minutes>(ns);
	ns -= m;
	auto s = duration_cast<seconds>(ns);
	ns -= s;
	std::optional<t_uint32> fs_count;
	switch (os.precision()) {
	case 9: fs_count = (t_uint32)ns.count();
		break;
	case 6: fs_count = (t_uint32)duration_cast<microseconds>(ns).count();
		break;
	case 3: fs_count = (t_uint32)duration_cast<milliseconds>(ns).count();
		break;
	}

	char fill = os.fill('0');
	if (d.count())
		os << d.count() << "d ";
	if (d.count() || h.count())
		os << std::setw(2) << h.count() << ":";
	if (d.count() || h.count() || m.count())
		os << std::setw(d.count() || h.count() ? 2 : 1) << m.count() << ":";
	os << std::setw(d.count() || h.count() || m.count() ? 2 : 1) << s.count();
	if (fs_count.has_value())
		os << "." << std::setw(os.precision()) << fs_count.value();
	if (!d.count() && !h.count() && !m.count())
		os << "s";

	os.fill(fill);
	return os;
}

void Fetcher::fetch_html(const pfc::string8 &url, const pfc::string8 &params, pfc::string8 &html, abort_callback &p_abort, bool use_oauth) {
	pfc::array_t<t_uint8> buffer;
	fetch_url(url, params, buffer, p_abort, use_oauth, "application/xml");
	html = pfc::string8((char*)buffer.get_ptr(), buffer.get_size());
}

void Fetcher::fetch_url(const pfc::string8 &url, const pfc::string8 &params, pfc::array_t<t_uint8> & out, abort_callback &p_abort, bool use_oauth, const pfc::string8 &content_type) {

    // Cancellation is not a successful, empty HTTP response. A caller may
    // reuse its output buffer, so invalidate it before checking the abort.
    // Propagate exception_aborted to the modeless task just like later I/O.
    out.set_size(0);
    p_abort.check();

	pfc::string8 msg_average;
	pfc::string8 status;
	pfc::string8 request_url = "";
	pfc::string8 clean_url(url);

	if (!url.get_length()) {
		foo_discogs_exception e;
		e << "(empty url)";
		pfc::string8 log_msg("trying to fetch empty url");
		throw e;
		return;
	}

	if (params.get_length()) clean_url << "?" << params;
    const bool use_api = discogger_net::is_discogs_api_url(url.c_str());
	use_oauth &= use_api;

    // Classify full-resolution artwork, excluding small preview thumbnails.
    // This must remain separate from the API rate-limit classification.
    const bool isImageUrl = discogger_net::should_throttle_discogs_image(url.c_str());

	if (isImageUrl) {

		time_t currentTime;
		struct tm* localTime;

		time(&currentTime);
		localTime = localtime(&currentTime);

		int Min = localTime->tm_min;

		m_ratelimit_max = 60/3;
		m_ratelimit_threshold = 30/3;

		if (!use_api) {
			m_ratelimit_max = 20;
			if (m_ratelimit_remaining > m_ratelimit_max) {
				m_ratelimit_remaining = m_ratelimit_max;
			}
			else if (Min != m_current_minute) {
				m_ratelimit_remaining = m_ratelimit_max;
			}
			else if (m_ratelimit_remaining == 0) {
                // Keep exhausted counters at zero; never wrap the unsigned value.
                m_ratelimit_remaining = 0;
            }
		}
		m_current_minute = Min;
	}
	else {
		m_ratelimit_max = 60;
		m_ratelimit_threshold = 30;
	}

	//log: url and oauth status
	log_msg("Discogs HTTP request (URL and query redacted).");
	if (use_oauth) {
		log_msg("Url OAuth enabled");
	}

	if (use_api || isImageUrl) {
		chrono::steady_clock::time_point time_now = chrono::steady_clock::now();
		chrono::duration<double> time_span = time_now - m_last_fetch;

		m_fetch_wait_rolling_avg = ROLLING_AVG_ALPHA * time_span.count() + (1.0 - ROLLING_AVG_ALPHA) * m_fetch_wait_rolling_avg;
		m_fetch_wait_rolling_avg = (std::min)(m_fetch_wait_rolling_avg, 1.0);

		//record time
		m_last_fetch = time_now;

		//log: average and last delta
		if (time_span != chrono::steady_clock::duration::zero()) {
			msg_average << "Lapse: ";
			//double microsecs = time_span.count();
			pfc::string8 msg_delta;
			if (m_throttle_delta) {
				std::stringstream human_read_time;
				human_read_time.precision(2);
				human_read_time << std::fixed << m_throttle_delta;
				msg_delta << " (+ " << human_read_time.str().c_str();
				msg_delta << ")";
			}

			std::stringstream human_read_time;
			human_read_time << std::setprecision(3) << time_span.count();
			msg_average << pfc::string8(human_read_time.str().c_str()) << msg_delta;
		}


		//reset when rate remaining reaches max m_value again
		m_ratelimit_cruise &= (m_ratelimit_remaining < m_ratelimit_max); //60

		m_ratelimit_cruise |= isImageUrl;

		log_msg(msg_average);

		//apply delta/set cruise mode
		if (m_ratelimit_cruise ||
			(m_fetch_wait_rolling_avg < discogger_net::discogs_rate_interval(m_ratelimit_max) && m_ratelimit_remaining <= m_ratelimit_threshold)) {

			m_ratelimit_cruise = true;

			chrono::duration<double, std::milli> time_span_milli = time_span;
			double sleep_base = m_ratelimit_remaining < (m_ratelimit_max / 6) ? 5000 : m_ratelimit_remaining < (m_ratelimit_max / 3) ? 2000 : 1000;
			m_throttle_delta = (std::max)(sleep_base - (isImageUrl ? 0 : time_span_milli.count()), 0.0);

			DWORD dw = static_cast<DWORD>((int)m_throttle_delta);
			discogger_net::interruptible_wait(dw, p_abort);
		}
		else {
			m_throttle_delta = 0;
		}
	}

	try {
		const http_request::ptr request = client->create_request("GET");

		//OAuth header
		if (use_oauth) {
			request->add_header(oauth_sign_url_header(url, params));
		}

		//Accept-Encoding header
		request->add_header(dllinflateInit2 != NULL ? "Accept-Encoding: gzip, deflate" : "Accept-Encoding: identity");

		//Accept user User-Agent and header
		request->add_header(USER_AGENT);
		pfc::string8 accept_header;
		accept_header << "Accept: " << content_type;
		request->add_header(accept_header);

		int tries = 1; //3x on error loading resource

		while (1) {

			try {

				file::ptr f;
				f = request->run_ex(clean_url.get_ptr(), p_abort);

				http_reply::ptr r;
				f->service_query_t(r);
				pfc::string8 status;
				r->get_status(status);

				pfc::string8 rate_header_buffer;
				pfc::string8 msg_ratelimits;

				if (r->get_http_header("X-Discogs-Ratelimit", rate_header_buffer)) {
                    std::size_t parsed = 0;
                    if (discogger_net::parse_discogs_rate_header(
                            rate_header_buffer.c_str(), 600, parsed, false)) {
                        m_ratelimit_max = parsed;
                        m_ratelimit_threshold = discogger_net::discogs_rate_threshold(parsed);
                        if (m_ratelimit_remaining > parsed) m_ratelimit_remaining = parsed;
                        msg_ratelimits << "RL: " << parsed;
                    }
                }
				if (r->get_http_header("X-Discogs-Ratelimit-Used", rate_header_buffer)) {
					msg_ratelimits << " - Used: " << rate_header_buffer;
				}
				if (r->get_http_header("X-Discogs-Ratelimit-Remaining", rate_header_buffer)) {
                    std::size_t parsed = 0;
                    if (discogger_net::parse_discogs_rate_header(
                            rate_header_buffer.c_str(), m_ratelimit_max, parsed)) {
                        m_ratelimit_remaining = parsed;
                        msg_ratelimits << " - RMNG: " << parsed;
                    }
                }

				if (isImageUrl && !use_api) {
					m_ratelimit_max = 20;
                    m_ratelimit_remaining = discogger_net::consume_discogs_rate(m_ratelimit_remaining);
					msg_ratelimits << "RL: " << m_ratelimit_max;
					msg_ratelimits << " - Used: " << m_ratelimit_max - m_ratelimit_remaining;
					msg_ratelimits << " - RMNG: " << m_ratelimit_remaining;
				}

				if (m_throttle_delta) {
					std::stringstream human_read_time;
					human_read_time << std::setprecision(3) << m_throttle_delta;
					if (msg_ratelimits.get_length()) {
						msg_ratelimits << " - Throttle engaged (" << pfc::string8(human_read_time.str().c_str()) << "\xCE\xBCs)";
					}
					else {
						msg_ratelimits << "Image Throttle engaged (" << pfc::string8(human_read_time.str().c_str()) << "\xCE\xBCs)";
					}
				}

				if (msg_ratelimits.length())
					log_msg(msg_ratelimits);
				else {
					m_ratelimit_remaining = m_ratelimit_threshold; //if rate info is empty reset to threshold
				}

				// Parse the actual HTTP status before considering query-specific hints.
				// In particular, Discogs search requests must retain 429 so the
				// existing rate-limit retry path remains reachable.
				const int error_code = discogger_net::parse_http_status_code(status.c_str());
				if (error_code != 200) {
					const bool is_search = url.find_first("search") != SIZE_MAX;
					pfc::string8 msg_status;
					msg_status << (is_search ? "Search HTTP error status: " : "HTTP error status: ") << status;
					log_msg(msg_status);
					switch (error_code) {
					case 404:
						throw http_404_exception();
					case 401:
						if (is_search) throw http_401_Search_exception();
						throw http_401_exception();
					case 429:
						throw http_429_exception();
					case 0:
						throw http_exception(0, "Malformed HTTP status line");
					default:
						throw http_exception(error_code);
					}
				}

				pfc::string8 reply_content_type;
				f->get_content_type(reply_content_type);
				if (reply_content_type.is_empty()) {
					out.set_size(0);
					throw network_exception("Error reading network response.");
				}

				// read stream...
				out.set_size(1024);
				size_t bufferUsed = 0;
				for (;;) {
					p_abort.check();
					size_t delta = out.get_size() - bufferUsed;
					size_t done = f->read(out.get_ptr() + bufferUsed, delta, p_abort);
					bufferUsed += done;
					if (done != delta) {
						break;
					}
					const size_t next_capacity = discogger_net::next_http_buffer_capacity(out.get_size());
                    if (!next_capacity)
                        throw network_exception("Discogs response exceeds the 64 MiB safety limit.");
                    out.set_size(next_capacity);
				}
				out.set_size(bufferUsed);

				// see if we must unzip buffer
				if (out.get_size() >= 6 && out[0] == 0x1f && out[1] == 0x8b) {
					pfc::array_t<t_uint8> unzipped;
					const size_t decoded_size = pfc::decode_little_endian<t_uint32>(out.get_ptr() + out.get_size() - 4);
                    if (!discogger_net::acceptable_http_decoded_size(decoded_size))
                        throw network_exception("Discogs gzip response exceeds the 64 MiB decoded safety limit.");
                    unzipped.set_size(decoded_size);
					uLongf destLen = unzipped.get_size();
					int state = myUncompress(unzipped.get_ptr(), &destLen, out.get_ptr(), out.get_size());
					// The gzip trailer declares the expected decoded size. Do not
                    // accept a short successful inflate in Release builds.
                    if (state != Z_OK || destLen != unzipped.get_size())
                        throw network_exception("Invalid or truncated gzip response.");
					out = unzipped;
				}
				break;
			}
			catch (http_429_exception) {

				if (tries > 20) throw;

				pfc::string8 error_msg;
				error_msg << "Rate-limited. Retrying: " << tries;
				log_msg(error_msg);
				discogger_net::interruptible_wait(2000u * tries, p_abort);
			}
			catch (foobar2000_io::exception_io &) {

				if (tries > 5) throw;

				// Host transport errors may contain the fully signed OAuth URL.
                // Never send raw SDK exception text to diagnostics.
				pfc::string8 error_msg;
                error_msg << "Network transport error (URL redacted). Retrying: " << tries;

				log_msg(error_msg);

				discogger_net::interruptible_wait(2000u * (tries > 1 ? 2 : 1), p_abort);
			}
			tries++;
		}
	}
	catch (exception_aborted) {
		throw;
	}
	catch (foo_discogs_exception &) {
        // No raw query strings, decoded URLs or OAuth signatures in logs/errors.
        log_msg("Discogs request failed (URL redacted).");
        throw;
    }
	catch (const foobar2000_io::exception_io &) {
        log_msg("Discogs transport retries exhausted (URL redacted).");
        throw network_exception("Discogs network transport failed; check connectivity and retry.");
    }
	catch (const std::exception&) {
        // The previous handler swallowed this exception and exposed any
        // partially downloaded bytes as a successful response.
        out.set_size(0);
        log_msg("Discogs request failed unexpectedly (details redacted).");
        throw network_exception("Discogs request failed; response discarded.");
    }
	catch (...) {
        out.set_size(0);
        log_msg("Discogs request failed with unknown error (details redacted).");
        throw network_exception("Unknown Discogs network failure; response discarded.");
    }
}

void Fetcher::fetch_html_simple_log(const pfc::string8& url, const pfc::string8& params, pfc::string8& html, abort_callback& p_abort, bool throw_abort, const pfc::string8& content_type) {

	// This best-effort request must never hand partial or previously returned
    // HTTP data to the artwork preview after a size, decode, or network error.
    html = "";
	pfc::array_t<t_uint8> buffer;

	pfc::string8 status;
	pfc::string8 request_url = "";
	pfc::string8 clean_url(url);

	if (params.get_length()) clean_url << "?" << params;

	log_msg("Discogs HTTP request (URL and query redacted).");

	try {
		const http_request::ptr request = client->create_request("GET");

		//HEADERS
		//Accept-Encoding header
		request->add_header(dllinflateInit2 != NULL ? "Accept-Encoding: gzip, deflate" : "Accept-Encoding: identity");

		//Accept user User-Agent and header
		request->add_header(USER_AGENT);
		pfc::string8 accept_header;
		accept_header << "Accept: " << content_type;
		request->add_header(accept_header);

		//END HEADERS

		int tries = 1; //3x on error loading resource
		file::ptr f;

		while (1) {

			try {

				f = request->run_ex(clean_url.get_ptr(), p_abort);

				http_reply::ptr r;
				f->service_query_t(r);
				pfc::string8 status;
				r->get_status(status);

				pfc::string8 debug_keep_alive;
				r->get_http_header("Connection", debug_keep_alive);

				// check status (applies only to request->run_ex)

                // This best-effort preview path used to merely log a 404,
                // 429 or partial HTTP response, then expose its body as if
                // artwork/JSON had been fetched successfully. Reject it before
                // reading any bytes; do not retry an unchanged bad status.
                if (!discogger_net::usable_preview_http_response(status.c_str())) {
                    buffer.set_size(0);
                    pfc::string8 message;
                    message << "Preview HTTP response rejected (status "
                            << discogger_net::parse_http_status_code(status.c_str()) << ").";
                    log_msg(message);
                    break;
                }

				//Sleep(1000);

				pfc::string8 reply_content_type;
				f->get_content_type(reply_content_type);
				if (reply_content_type.is_empty()) {
					buffer.set_size(0);
					log_msg("Error reading network response.");
					break;
				}

				// read stream...
				buffer.set_size(1024);
				size_t bufferUsed = 0;
				for (;;) {
					p_abort.check();
					size_t delta = buffer.get_size() - bufferUsed;
					size_t done = f->read(buffer.get_ptr() + bufferUsed, delta, p_abort);
					bufferUsed += done;
					if (done != delta) {
						break;
					}
					const size_t next_capacity = discogger_net::next_http_buffer_capacity(buffer.get_size());
                    if (!next_capacity)
                        throw network_exception("Discogs response exceeds the 64 MiB safety limit.");
                    buffer.set_size(next_capacity);
				}
				buffer.set_size(bufferUsed);

				// see if we must unzip buffer
				if (buffer.get_size() >= 6 && buffer[0] == 0x1f && buffer[1] == 0x8b) {
					pfc::array_t<t_uint8> unzipped;
					const size_t decoded_size = pfc::decode_little_endian<t_uint32>(buffer.get_ptr() + buffer.get_size() - 4);
                    if (!discogger_net::acceptable_http_decoded_size(decoded_size))
                        throw network_exception("Discogs gzip response exceeds the 64 MiB decoded safety limit.");
                    unzipped.set_size(decoded_size);
					uLongf destLen = unzipped.get_size();
					int state = myUncompress(unzipped.get_ptr(), &destLen, buffer.get_ptr(), buffer.get_size());
					// Preview fetches must not return compressed bytes as if they
                    // were decoded artwork after a failed inflate.
                    if (state != Z_OK || destLen != unzipped.get_size()) {
                        buffer.set_size(0);
                        log_msg("Invalid or truncated gzip preview response.");
                        break;
                    }
					buffer = unzipped;
				}

				break;
			}
			catch (foobar2000_io::exception_io&) {

				if (tries > 5) throw;

				// Host transport errors may contain the fully signed OAuth URL.
                // Never send raw SDK exception text to diagnostics.
				pfc::string8 error_msg;
                error_msg << "Network transport error (URL redacted). Retrying: " << tries;
				log_msg(error_msg);
				discogger_net::interruptible_wait(2000u * (tries > 1 ? 2 : 1), p_abort);
			}
			tries++;
		}
	}
	catch (exception_aborted) {
        buffer.set_size(0);
		if (throw_abort)
			throw;
	}
	catch (foo_discogs_exception&) {
        buffer.set_size(0);
        log_msg("Discogs preview request failed (URL redacted).");
    }
	catch (const foobar2000_io::exception_io&) {
        buffer.set_size(0);
        log_msg("Discogs preview transport retries exhausted (URL redacted).");
        throw network_exception("Discogs preview network transport failed.");
    }
	catch (const std::exception&) {
        buffer.set_size(0);
        log_msg("Discogs preview request failed unexpectedly (details redacted).");
    }
	catch (...) {
        buffer.set_size(0);
        log_msg("Unknown Discogs preview request failure (details redacted).");
    }

	html = pfc::string8((char*)buffer.get_ptr(), buffer.get_size());
}

void Fetcher::set_oauth(const pfc::string8 &token, const pfc::string8 &token_secret) {
#if defined(DISCOGGER_RUNNER_MEDIA_QA)
	// Test-only process-memory OAuth injection. Never compile secrets into the DLL,
	// write them to a profile, or enable this path in production component builds.
	const char* runner = std::getenv("GITHUB_ACTIONS");
	const char* key = std::getenv("DISCOGS_OAUTH_CONSUMER_KEY");
	const char* secret = std::getenv("DISCOGS_OAUTH_CONSUMER_SECRET");
	const char* access = std::getenv("DISCOGS_OAUTH_ACCESS_TOKEN");
	const char* access_secret = std::getenv("DISCOGS_OAUTH_TOKEN_SECRET");
	const bool any = (key && *key) || (secret && *secret) ||
		(access && *access) || (access_secret && *access_secret);
	const bool all = (key && *key) && (secret && *secret) &&
		(access && *access) && (access_secret && *access_secret);
	if (any && (!all || !runner || std::strcmp(runner, "true") != 0)) {
		throw foo_discogs_exception("Incomplete or unauthorized runner OAuth configuration.");
	}
	if (all) {
		consumer = new OAuth::Consumer(key, secret);
		oauth_token = access;
		oauth_token_secret = access_secret;
		init_oauth();
		return;
	}
#endif
	consumer = new OAuth::Consumer(CONSUMER_KEY, CONSUMER_SECRET);
	oauth_token = token;
	oauth_token_secret = token_secret;
	init_oauth();
}

void Fetcher::init_oauth() {
	if (!oauth_token.get_length() && !oauth_token_secret.get_length()) {
		if (oauth != nullptr) {
			delete oauth;
			oauth = nullptr;
		}
		if (access_token != nullptr) {
			delete access_token;
			access_token = nullptr;
		}
		oauth = new OAuth::Client(consumer);
	}
	else {
		if (access_token != nullptr) {
			delete access_token;
			access_token = nullptr;
		}
		if (oauth != nullptr) {
			delete oauth;
			oauth = nullptr;
		}
		access_token = new OAuth::Token(oauth_token.get_ptr(), oauth_token_secret.get_ptr());
		oauth = new OAuth::Client(consumer, access_token);
	}
}

void Fetcher::update_oauth(const pfc::string8 &token, const pfc::string8 &token_secret) {
	if (token == oauth_token && token_secret == oauth_token_secret) {
		return;
	}
	oauth_token = token;
	oauth_token_secret = token_secret;
	init_oauth();
}

void Fetcher::test_oauth(const pfc::string8 &token, const pfc::string8 &token_secret, abort_callback &p_abort) {
	OAuth::Consumer c = OAuth::Consumer(CONSUMER_KEY, CONSUMER_SECRET);
	OAuth::Token at = OAuth::Token(token.get_ptr(), token_secret.get_ptr());
	OAuth::Client oa = OAuth::Client(&c, &at);

	const char *identity = "https://api.discogs.com/oauth/identity"; 
	pfc::string8 url = "";
	url << identity << "?";
	url << oa.getURLQueryString(OAuth::Http::Get, identity, "").c_str();

	pfc::string8 html;
	// throws network_exception on error
	fetch_html(url, "", html, p_abort, false);
}

pfc::string8 Fetcher::oauth_sign_url_header(const pfc::string8& url, const pfc::string8& params) {
	pfc::string8 oauth_header;
	if (params.get_length()) {
		pfc::string8 tmp = url;
		tmp << "?" << params;
		oauth_header = oauth->getFormattedHttpHeader(OAuth::Http::Get, tmp.get_ptr()).c_str();
	}
	else {
		oauth_header = oauth->getFormattedHttpHeader(OAuth::Http::Get, url.get_ptr()).c_str();
	}

	return oauth_header;
}

pfc::string8 Fetcher::get_oauth_authorize_url(abort_callback &p_abort) {
	OAuth::Client oa = OAuth::Client(consumer);

	pfc::string8 oauth_query_str = oa.getURLQueryString(OAuth::Http::Get, REQUEST_TOKEN_URL).c_str();

	pfc::string8 html;
	fetch_html(REQUEST_TOKEN_URL, oauth_query_str, html, p_abort, false);

	// Contains oauth_token and oauth_token_secret; never log raw body.
    log_msg("OAuth request-token response received (redacted).");

	if (request_token != nullptr) {
		delete request_token;
		request_token = nullptr;
	}
	OAuth::Token t = OAuth::Token::extract(html.get_ptr());
	request_token = new OAuth::Token(t.key(), t.secret());

	pfc::string8 auth_url;
	auth_url << AUTHORIZE_URL << "?oauth_token=" << request_token->key().c_str();
	return auth_url;
}

OAuth::Token * Fetcher::generate_oauth(pfc::string8 &code, abort_callback &p_abort) {
	if (request_token == nullptr) {
		return nullptr;
	}
	request_token->setPin(code.get_ptr());

	OAuth::Client oa = OAuth::Client(consumer, request_token);

	pfc::string8 oauth_query_str= oa.getURLQueryString(OAuth::Http::Get, ACCESS_TOKEN_URL, "", true).c_str();

	pfc::string8 html;
	fetch_html(ACCESS_TOKEN_URL, oauth_query_str, html, p_abort, false);

	// Contains persistent access token and secret; never log raw body.
    log_msg("OAuth access-token response received (redacted).");

	OAuth::KeyValuePairs access_token_resp_data = OAuth::ParseKeyValuePairs(html.get_ptr());
	OAuth::Token t = OAuth::Token::extract(access_token_resp_data);

	OAuth::Token *generated_token = new OAuth::Token(t.key(), t.secret());
	return generated_token;
}
