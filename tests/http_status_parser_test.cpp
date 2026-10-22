#include "../http_status_util.h"
#include <cassert>

int main() {
    using discogger_net::parse_http_status_code;
    assert(parse_http_status_code("HTTP/1.1 200 OK") == 200);
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
    return 0;
}
