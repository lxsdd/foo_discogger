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
    return 0;
}
