#pragma once
#include <climits>
#include <string>
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

// Discogs IDs are numerical identities, not distinct padded spellings.
// Canonicalize preview/cache keys without altering any user source metadata.
inline std::string canonical_release_id(std::string_view id) {
    if (!valid_release_id(id)) return {};
    return std::string(id.substr(id.find_first_not_of('0')));
}

// Strict positive integer conversion for master/release cache keys. The
// numeric result is assigned only on success: malformed provider identities
// cannot alias the synthetic artist 0 or an existing cached master.
inline bool parse_positive_discogs_id(std::string_view id,
                                      unsigned long& result) noexcept {
    if (!valid_release_id(id)) return false;
    unsigned long parsed = 0;
    for (const char digit : id)
        parsed = parsed * 10UL + static_cast<unsigned long>(digit - '0');
    result = parsed;
    return true;
}

// Strict preview identity: malformed IDs never compare equal through atoi.
inline bool same_positive_discogs_id(std::string_view a, std::string_view b) {
    const auto canonical = canonical_release_id(a);
    return !canonical.empty() && canonical == canonical_release_id(b);
}
// Artist zero is reserved for explicit synthetic unknown-artist previews.
// On failure neither output may be changed.
inline bool parse_preview_cache_ids(std::string_view artist, std::string_view item,
                                    bool synthetic_unknown_artist,
                                    unsigned long& artist_out,
                                    unsigned long& item_out) noexcept {
    unsigned long a = 0, r = 0;
    if (!parse_positive_discogs_id(item, r) ||
        (!synthetic_unknown_artist && !parse_positive_discogs_id(artist, a)))
        return false;
    artist_out = a; item_out = r;
    return true;
}
// Invalid nonzero master links must never masquerade as standalone releases.
inline bool parse_master_link(std::string_view raw, bool& has_master) noexcept {
    if (raw.empty() || raw == "0") { has_master = false; return true; }
    unsigned long value = 0;
    if (!parse_positive_discogs_id(raw, value)) return false;
    has_master = true;
    return true;
}
// URL navigation is a lookup hint, never a tag or release-identity approval.
// Extract only the ID immediately following the expected Discogs path kind.
// In particular, do not choose the largest number from a release title/slug.
enum class discogs_reference_kind { artist, release };
inline bool parse_discogs_reference(std::string_view input,
                                    discogs_reference_kind kind,
                                    std::string& id) {
    const auto start = input.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) return false;
    input.remove_prefix(start);
    const auto last = input.find_last_not_of(" \t\r\n");
    input = input.substr(0, last + 1);

    const char marker = kind == discogs_reference_kind::release ? 'r' : 'a';
    if (input.size() >= 4 && input.front() == '[' && input.back() == ']' &&
        input[1] == marker) {
        auto number = input.substr(2, input.size() - 3);
        if (!number.empty() && number.front() == '=') number.remove_prefix(1);
        if (!valid_release_id(number)) return false;
        id = canonical_release_id(number);
        return true;
    }

    // Accept only the actual website host, never an embedded or lookalike URL.
    constexpr std::string_view prefixes[] = {
        "https://www.discogs.com/", "https://discogs.com/",
        "http://www.discogs.com/", "http://discogs.com/",
        "www.discogs.com/", "discogs.com/"
    };
    for (const auto prefix : prefixes) {
        if (input.substr(0, prefix.size()) == prefix) {
            input.remove_prefix(prefix.size());
            break;
        }
    }
    if (!input.empty() && input.front() == '/') input.remove_prefix(1);

    // Discogs shares localized links such as /de/release/123-title.
    constexpr std::string_view locales[] = {
        "de/", "en/", "fr/", "es/", "it/", "pt/", "ja/"
    };
    for (const auto locale : locales) {
        if (input.substr(0, locale.size()) == locale) {
            input.remove_prefix(locale.size());
            break;
        }
    }
    const std::string_view path = kind == discogs_reference_kind::release
        ? std::string_view("release/") : std::string_view("artist/");
    if (input.substr(0, path.size()) != path) return false;
    input.remove_prefix(path.size());

    std::size_t digits = 0;
    while (digits < input.size() && input[digits] >= '0' &&
           input[digits] <= '9') ++digits;
    const auto number = input.substr(0, digits);
    if (!valid_release_id(number)) return false;

    // A slug is optional; it may contain digits, but never supplies the ID.
    std::size_t pos = digits;
    if (pos < input.size() && input[pos] == '-') {
        ++pos;
        const std::size_t slug_start = pos;
        while (pos < input.size() && input[pos] != '?' &&
               input[pos] != '#' && input[pos] != '/') {
            const unsigned char ch = static_cast<unsigned char>(input[pos]);
            if (ch <= 32 || ch == 127) return false;
            ++pos;
        }
        if (pos == slug_start) return false;
    }
    if (pos < input.size() && input[pos] == '/') ++pos;
    if (pos < input.size() && input[pos] != '?' && input[pos] != '#')
        return false;
    id = canonical_release_id(number);
    return true;
}
} // namespace discogger_search
