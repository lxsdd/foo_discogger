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
