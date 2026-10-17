#pragma once

#include <string_view>

// Independent of the foobar2000 SDK so parsing can be tested on every CI host.
// Returns 0 for an unrecognized / malformed status line.
namespace discogger_net {
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
}
