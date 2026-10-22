#pragma once

#include <string_view>
#include <cstddef>

// Independent of the foobar2000 SDK so parsing can be tested on every CI host.
// Returns 0 for an unrecognized / malformed status line.
namespace discogger_net {
// Cap both compressed HTTP responses and their declared decoded size.
// A hostile gzip ISIZE must never allocate an unbounded buffer before inflate.
inline constexpr std::size_t max_http_response_bytes = 64u * 1024u * 1024u;
inline std::size_t next_http_buffer_capacity(std::size_t current) noexcept {
    if (current == 0 || current >= max_http_response_bytes) return 0;
    return current > max_http_response_bytes / 2
        ? max_http_response_bytes : current * 2;
}
inline bool acceptable_http_decoded_size(std::size_t size) noexcept {
    return size != 0 && size <= max_http_response_bytes;
}

inline int parse_http_status_code(std::string_view line) noexcept {
    const auto separator = line.find_first_of(" \t");
    if (separator == std::string_view::npos) return 0;
    const auto first = line.find_first_not_of(" \t", separator);
    if (first == std::string_view::npos || line.size() - first < 3) return 0;
    const char a = line[first], b = line[first + 1], c = line[first + 2];
    if (a < '0' || a > '9' || b < '0' || b > '9' || c < '0' || c > '9') return 0;
    if (first + 3 < line.size() && line[first + 3] != ' ' && line[first + 3] != '\t') return 0;
    return (a - '0') * 100 + (b - '0') * 10 + (c - '0');
}
// Optional preview fetches consume only an unambiguous complete HTTP success,
// never a 404/429 JSON error document or a 206 partial body.
inline bool usable_preview_http_response(std::string_view status_line) noexcept {
    return parse_http_status_code(status_line) == 200;
}
}
