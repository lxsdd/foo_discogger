#include "../http_status_util.h"
#include <cassert>
#include <initializer_list>

int main() {
    using discogger_net::parse_http_status_code;
    assert(parse_http_status_code("HTTP/1.1 200 OK") == 200);
    assert(parse_http_status_code("HTTP/2 200\r\n") == 200);
    assert(parse_http_status_code("HTTP/2 429\r\n") == 429);
    assert(parse_http_status_code("HTTP/1.1 503 Service Unavailable\r\n") == 503);
    using discogger_net::retryable_discogs_server_status;
    using discogger_net::transient_server_retry_delay_ms;
    for (int status : {500, 502, 503, 504}) assert(retryable_discogs_server_status(status));
    for (int status : {0, 200, 201, 301, 400, 401, 403, 404, 408, 409, 425, 429, 501, 505, 599})
        assert(!retryable_discogs_server_status(status));
    assert(transient_server_retry_delay_ms(0) == 0u);
    assert(transient_server_retry_delay_ms(1) == 750u);
    assert(transient_server_retry_delay_ms(2) == 1500u);
    assert(transient_server_retry_delay_ms(3) == 0u);
    assert(transient_server_retry_delay_ms(999) == 0u);
    assert(parse_http_status_code("HTTP/3 200") == 200);
    assert(parse_http_status_code("HTTP/2\t429") == 429);
    assert(parse_http_status_code("X-Proxy 200 OK") == 0);
    assert(parse_http_status_code("HTTP/1.1x 200 OK") == 0);
    assert(parse_http_status_code("HTTP/2 200\r\nHTTP/2 429") == 0);
    assert(parse_http_status_code("HTTP/2 200\r\n\r\n") == 0);
    assert(parse_http_status_code("HTTP/2 200\n\n") == 0);
    assert(!discogger_net::usable_preview_http_response("HTTP/2 200\r\n\r\n"));
    assert(parse_http_status_code("HTTP/2 099") == 0);
    assert(parse_http_status_code("HTTP/2 600") == 0);
    assert(parse_http_status_code("HTTP/2 200") == 200);
    assert(parse_http_status_code("HTTP/2 429 Too Many Requests") == 429);
    assert(parse_http_status_code("HTTP/1.1 401 Unauthorized") == 401);
    assert(parse_http_status_code("HTTP/1.1 404 Not Found") == 404);
    assert(parse_http_status_code("HTTP/1.1 403 Forbidden") == 403);
    assert(parse_http_status_code("HTTP/1.0 500 Server Error") == 500);
    assert(parse_http_status_code("HTTP/1.1 2000 Invalid") == 0);
    assert(parse_http_status_code("garbage") == 0);
    assert(parse_http_status_code("HTTP/2 ???") == 0);
    assert(parse_http_status_code("HTTP/2") == 0);
    using discogger_net::usable_preview_http_response;
    assert(usable_preview_http_response("HTTP/1.1 200 OK"));
    assert(usable_preview_http_response("HTTP/2 200"));
    assert(usable_preview_http_response("HTTP/2 200\r\n"));
    assert(!usable_preview_http_response("X-Proxy 200 OK"));
    assert(!usable_preview_http_response("HTTP/1.1 206 Partial Content"));
    assert(!usable_preview_http_response("HTTP/2 301 Moved Permanently"));
    assert(!usable_preview_http_response("HTTP/2 401 Unauthorized"));
    assert(!usable_preview_http_response("HTTP/2 404 Not Found"));
    assert(!usable_preview_http_response("HTTP/2 429 Too Many Requests"));
    assert(!usable_preview_http_response("HTTP/2 500 Server Error"));
    assert(!usable_preview_http_response("HTTP/2 2000 Invalid"));
    assert(!usable_preview_http_response("garbage"));
    using discogger_net::should_throttle_discogs_image;
    assert(should_throttle_discogs_image("https://i.discogs.com/aBcD.jpeg"));
    assert(should_throttle_discogs_image("https://img.discogs.com/large.png"));
    assert(should_throttle_discogs_image("https://i.discogs.com/path/h:600/image.jpg"));
    assert(!should_throttle_discogs_image("https://i.discogs.com/path/h:150/image.jpg"));
    assert(!should_throttle_discogs_image("https://img.discogs.com/h:150/preview.png"));
    assert(!should_throttle_discogs_image("https://i.discogs.com/image.jpg?h:150"));
    assert(!should_throttle_discogs_image("https://i.discogs.com.evil.test/image.jpg"));
    assert(!should_throttle_discogs_image("https://i.discogs.comevil.test/image.jpg"));
    assert(!should_throttle_discogs_image("http://i.discogs.com/image.jpg"));
    assert(!should_throttle_discogs_image("https://api.discogs.com/database/search"));
    assert(!should_throttle_discogs_image("https://i.discogs.com"));
    assert(!should_throttle_discogs_image(""));
    using discogger_net::is_discogs_api_url;
    assert(is_discogs_api_url("https://api.discogs.com"));
    assert(is_discogs_api_url("https://api.discogs.com/database/search"));
    assert(is_discogs_api_url("https://api.discogs.com/oauth/identity"));
    assert(is_discogs_api_url("https://api.discogs.com?per_page=100"));
    assert(!is_discogs_api_url("http://api.discogs.com/oauth/identity"));
    assert(!is_discogs_api_url("https://api.discogs.com.evil.test/database/search"));
    assert(!is_discogs_api_url("https://api.discogs.com@evil.test/path"));
    assert(!is_discogs_api_url("https://api.discogs.com:8443/path"));
    assert(!is_discogs_api_url("https://i.discogs.com/cover.jpg"));
    assert(!is_discogs_api_url("https://discogs.com/api"));
    assert(!is_discogs_api_url("https://evil.test/path/api.discogs.com"));
    assert(!is_discogs_api_url(""));
    using discogger_net::parse_discogs_rate_header;
    using discogger_net::discogs_rate_threshold;
    using discogger_net::consume_discogs_rate;
    using discogger_net::discogs_rate_interval;
    std::size_t parsed_rate = 999;
    assert(parse_discogs_rate_header("60", 600, parsed_rate, false) && parsed_rate == 60);
    assert(parse_discogs_rate_header(" 20 ", 600, parsed_rate, false) && parsed_rate == 20);
    assert(parse_discogs_rate_header("\t20\t", 600, parsed_rate, false) && parsed_rate == 20);
    assert(!parse_discogs_rate_header("20\tjunk", 600, parsed_rate, false));
    assert(parse_discogs_rate_header("0", 60, parsed_rate) && parsed_rate == 0);
    assert(!parse_discogs_rate_header("0", 600, parsed_rate, false));
    assert(!parse_discogs_rate_header("-1", 600, parsed_rate));
    assert(!parse_discogs_rate_header("+60", 600, parsed_rate));
    assert(!parse_discogs_rate_header("60junk", 600, parsed_rate));
    assert(!parse_discogs_rate_header("18446744073709551616", 600, parsed_rate));
    assert(!parse_discogs_rate_header("601", 600, parsed_rate));
    assert(!parse_discogs_rate_header("61", 60, parsed_rate));
    assert(!parse_discogs_rate_header(" ", 60, parsed_rate));
    assert(!parse_discogs_rate_header("", 60, parsed_rate));
    assert(parsed_rate == 0); // Invalid headers never mutate the last valid value.
    assert(discogs_rate_threshold(60) == 30);
    assert(discogs_rate_threshold(20) == 10);
    assert(discogs_rate_threshold(1) == 0);
    assert(discogs_rate_threshold(0) == 0);
    assert(consume_discogs_rate(0) == 0);
    assert(consume_discogs_rate(1) == 0);
    assert(consume_discogs_rate(20) == 19);
    assert(discogs_rate_interval(60) == 1.0);
    assert(discogs_rate_interval(120) == 0.5);
    assert(discogs_rate_interval(0) == 60.0);
    // Network response sizes must not overflow through repeated doubling,
    // and hostile gzip trailers must never authorize huge allocations.
    using discogger_net::next_http_buffer_capacity;
    using discogger_net::acceptable_http_decoded_size;
    using discogger_net::max_http_response_bytes;
    assert(max_http_response_bytes == 64u * 1024u * 1024u);
    assert(next_http_buffer_capacity(0) == 0);
    assert(next_http_buffer_capacity(1024) == 2048);
    assert(next_http_buffer_capacity(max_http_response_bytes / 2) == max_http_response_bytes);
    assert(next_http_buffer_capacity(max_http_response_bytes - 1) == max_http_response_bytes);
    assert(next_http_buffer_capacity(max_http_response_bytes) == 0);
    assert(next_http_buffer_capacity(static_cast<std::size_t>(-1)) == 0);
    assert(!acceptable_http_decoded_size(0));
    assert(acceptable_http_decoded_size(1));
    assert(acceptable_http_decoded_size(max_http_response_bytes));
    assert(!acceptable_http_decoded_size(max_http_response_bytes + 1));
    assert(!acceptable_http_decoded_size(static_cast<std::size_t>(-1)));
    // 429 policy: three bounded, abort-responsive retries; malformed or
    // excessive server cooldowns must not trigger speculative requests.
    using discogger_net::discogs_429_retry_delay_ms;
    assert(discogs_429_retry_delay_ms(0) == 0u);
    assert(discogs_429_retry_delay_ms(1) == 2000u);
    assert(discogs_429_retry_delay_ms(2) == 4000u);
    assert(discogs_429_retry_delay_ms(3) == 8000u);
    assert(discogs_429_retry_delay_ms(4) == 0u);
    assert(discogs_429_retry_delay_ms(1, " 10 ") == 10000u);
    assert(discogs_429_retry_delay_ms(2, "1") == 4000u);
    assert(discogs_429_retry_delay_ms(1, "60") == 60000u);
    assert(discogs_429_retry_delay_ms(1, "0") == 2000u);
    assert(discogs_429_retry_delay_ms(1, "61") == 0u);
    assert(discogs_429_retry_delay_ms(1, "120") == 0u);
    assert(discogs_429_retry_delay_ms(1, "Wed, 21 Oct 2015 07:28:00 GMT") == 0u);
    assert(discogs_429_retry_delay_ms(1, "-1") == 0u);
    assert(discogs_429_retry_delay_ms(1, "1junk") == 0u);
    assert(discogs_429_retry_delay_ms(1, "18446744073709551616") == 0u);
    using discogger_net::stop_optional_discovery_on_http_status;
    for (int status : {401, 403, 429}) assert(stop_optional_discovery_on_http_status(status));
    for (int status : {0, 200, 400, 404, 408, 500, 503})
        assert(!stop_optional_discovery_on_http_status(status));
    // Original structured search must preserve actionable HTTP failure
    // classes after the fetcher's bounded retries, never signed OAuth URLs.
    using discogger_net::discogs_search_http_error_message;
    assert(std::string_view(discogs_search_http_error_message(401)).find("HTTP 401") != std::string_view::npos);
    assert(std::string_view(discogs_search_http_error_message(403)).find("HTTP 403") != std::string_view::npos);
    assert(std::string_view(discogs_search_http_error_message(404)).find("HTTP 404") != std::string_view::npos);
    assert(std::string_view(discogs_search_http_error_message(429)).find("HTTP 429") != std::string_view::npos);
    for (int status : {500, 502, 503, 504})
        assert(std::string_view(discogs_search_http_error_message(status)).find("server unavailable") != std::string_view::npos);
    for (int status : {0, 400, 408, 418, 599})
        assert(std::string_view(discogs_search_http_error_message(status)).find("request failed") != std::string_view::npos);
    for (int status : {0, 401, 403, 404, 429, 500, 502, 503, 504, 599}) {
        const std::string_view message(discogs_search_http_error_message(status));
        assert(message.find("https://") == std::string_view::npos);
        assert(message.find("oauth_token") == std::string_view::npos);
        assert(message.find("Authorization") == std::string_view::npos);
    }
    return 0;
}
