#pragma once
// Portable, allocation-free matching of configurable foobar tag field names.
// This header deliberately has no SDK dependency; both MSVC and GCC test it.
#include <string_view>

namespace discogger_multivalue {
inline std::string_view trim(std::string_view s) noexcept {
    constexpr std::string_view whitespace = " \t\r\n";
    const auto first = s.find_first_not_of(whitespace);
    if (first == std::string_view::npos) return {};
    const auto last = s.find_last_not_of(whitespace);
    return s.substr(first, last - first + 1);
}

inline unsigned char fold_ascii(unsigned char c) noexcept {
    return c >= 'A' && c <= 'Z' ? static_cast<unsigned char>(c - 'A' + 'a') : c;
}

inline bool same_field(std::string_view a, std::string_view b) noexcept {
    a = trim(a);
    b = trim(b);
    if (a.empty() || a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (fold_ascii(static_cast<unsigned char>(a[i])) !=
            fold_ascii(static_cast<unsigned char>(b[i]))) return false;
    }
    return true;
}

inline bool contains(std::string_view configured, std::string_view field) noexcept {
    field = trim(field);
    if (field.empty()) return false;
    while (!configured.empty()) {
        const auto sep = configured.find(';');
        const auto token = configured.substr(0, sep);
        if (same_field(token, field)) return true;
        if (sep == std::string_view::npos) break;
        configured.remove_prefix(sep + 1);
    }
    return false;
}
} // namespace discogger_multivalue
