#pragma once
#include <climits>
#include <string_view>

// Discogs releases are handed to older signed-int parsing and cache-key
// paths. Reject partial numbers and overflow rather than relying on atoi.
namespace discogger_search {
inline bool valid_release_id(std::string_view id) {
    if (id.empty()) return false;
    unsigned long value = 0;
    for (const char ch : id) {
        if (ch < '0' || ch > '9') return false;
        const unsigned long digit = static_cast<unsigned long>(ch - '0');
        if (value > (static_cast<unsigned long>(INT_MAX) - digit) / 10UL)
            return false;
        value = value * 10UL + digit;
    }
    return value != 0;
}
}
