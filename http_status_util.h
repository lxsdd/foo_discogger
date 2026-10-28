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

// HTTP rate-limit headers are untrusted. Reject signs, junk, overflow and
// implausible counts before updating throttling state. A zero remaining count
// is meaningful; a zero maximum must never authorize division.
inline bool parse_discogs_rate_header(std::string_view value, std::size_t limit,
                                      std::size_t& result, bool allow_zero = true) noexcept {
    if (value.empty() || limit == 0) return false;
    const auto first = value.find_first_not_of(" \t");
    if (first == std::string_view::npos) return false;
    const auto last = value.find_last_not_of(" \t");
    value = value.substr(first, last - first + 1);
    if (value.empty()) return false;
    std::size_t parsed = 0;
    for (char c : value) {
        if (c < '0' || c > '9') return false;
        const std::size_t digit = static_cast<std::size_t>(c - '0');
        if (digit > limit || parsed > (limit - digit) / 10) return false;
        parsed = parsed * 10 + digit;
    }
    if (!allow_zero && parsed == 0) return false;
    result = parsed;
    return true;
}
inline std::size_t discogs_rate_threshold(std::size_t maximum) noexcept {
    return maximum > 30 ? maximum - 30 : maximum / 2;
}
inline std::size_t consume_discogs_rate(std::size_t remaining) noexcept {
    return remaining == 0 ? 0 : remaining - 1;
}
inline double discogs_rate_interval(std::size_t maximum) noexcept {
    return 60.0 / static_cast<double>(maximum == 0 ? 1 : maximum);
}

// Discogs 429 retries are bounded and cancellation-aware. Retry-After is
// untrusted: accept only decimal seconds up to 60. HTTP dates, overflow,
// negative values or requests for longer waits stop retries rather than
// issuing speculative follow-up requests against an exhausted quota.
inline unsigned discogs_429_retry_delay_ms(unsigned attempt,
                                           std::string_view retry_after = {}) noexcept {
    const unsigned fallback = attempt == 1 ? 2000u :
                              attempt == 2 ? 4000u :
                              attempt == 3 ? 8000u : 0u;
    if (!fallback || retry_after.empty()) return fallback;
    std::size_t seconds = 0;
    if (!parse_discogs_rate_header(retry_after, 60, seconds)) return 0;
    const unsigned requested = static_cast<unsigned>(seconds) * 1000u;
    return requested > fallback ? requested : fallback;
}
// Optional broad searches must not multiply requests after authorization
// denial or exhausted rate limits. Other optional errors can be contained
// while retaining already parsed original candidates.
inline bool stop_optional_discovery_on_http_status(int status) noexcept {
    return status == 401 || status == 403 || status == 429;
}

// Keep original structured Discogs search failures actionable after the
// fetcher's bounded retries. These static messages deliberately exclude
// OAuth-signed URLs, response bodies, request parameters and token material.
// Optional fallback has a separate containment policy.
inline const char* discogs_search_http_error_message(int status) noexcept {
    switch (status) {
    case 401: return "Discogs search authorization failed (HTTP 401); check OAuth and retry.";
    case 403: return "Discogs search access denied (HTTP 403); check permissions.";
    case 404: return "Discogs search endpoint unavailable (HTTP 404); no results published.";
    case 429: return "Discogs rate limit (HTTP 429) after bounded retries; retry later.";
    case 500: case 502: case 503: case 504:
        return "Discogs server unavailable after bounded retries; retry later.";
    default: return "Discogs search HTTP request failed; no results published.";
    }
}

// OAuth headers may contain signed credentials. Never attach them to
// untrusted resource URLs merely because their path/query mentions "api".
// Restrict signing to the exact HTTPS Discogs API origin.
inline bool is_discogs_api_url(std::string_view url) noexcept {
    constexpr std::string_view origin = "https://api.discogs.com";
    if (url.substr(0, origin.size()) != origin) return false;
    return url.size() == origin.size() || url[origin.size()] == '/' ||
           url[origin.size()] == '?';
}

// Full-size Discogs images share the rate limiter with API requests.
// An old `!url.find("h:150")` predicate always rejected actual HTTPS
// image URLs because the preview marker never occurs at offset zero.
// Match the exact HTTPS hosts, not lookalike subdomains or arbitrary URLs.
inline bool should_throttle_discogs_image(std::string_view url) noexcept {
    constexpr std::string_view old_host = "https://img.discogs.com/";
    constexpr std::string_view current_host = "https://i.discogs.com/";
    const bool trusted_host = url.substr(0, old_host.size()) == old_host ||
                              url.substr(0, current_host.size()) == current_host;
    return trusted_host && url.find("h:150") == std::string_view::npos;
}

inline int parse_http_status_code(std::string_view line) noexcept {
    // libcurl header callbacks can deliver CRLF-terminated status lines,
    // including HTTP/2 responses without a reason phrase ("HTTP/2 429\\r\\n").
    // Strip only trailing line endings; reject embedded lines and forged
    // non-HTTP prefixes before accepting a status or preview body.
    // Accept one optional terminator, not a second forged line.
    if (line.size() >= 2 && line.substr(line.size() - 2) == "\r\n")
        line.remove_suffix(2);
    else if (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
        line.remove_suffix(1);
    if (line.find_first_of("\r\n") != std::string_view::npos) return 0;
    const auto separator = line.find_first_of(" \t");
    if (separator == std::string_view::npos) return 0;
    const auto version = line.substr(0, separator);
    if (version != "HTTP/1.0" && version != "HTTP/1.1" &&
        version != "HTTP/2" && version != "HTTP/3") return 0;
    if (separator == std::string_view::npos) return 0;
    const auto first = line.find_first_not_of(" \t", separator);
    if (first == std::string_view::npos || line.size() - first < 3) return 0;
    const char a = line[first], b = line[first + 1], c = line[first + 2];
    if (a < '0' || a > '9' || b < '0' || b > '9' || c < '0' || c > '9') return 0;
    if (first + 3 < line.size() && line[first + 3] != ' ' && line[first + 3] != '\t') return 0;
    const int status = (a - '0') * 100 + (b - '0') * 10 + (c - '0');
    return status >= 100 && status <= 599 ? status : 0;
}
// Only idempotent GETs may retry transient server failures. Authorization,
// invalid requests, missing releases and Discogs-specific 429 retain their
// existing separate error/retry semantics; no partial payload is accepted.
inline bool retryable_discogs_server_status(int status) noexcept {
    return status == 500 || status == 502 || status == 503 || status == 504;
}
inline unsigned transient_server_retry_delay_ms(unsigned attempt) noexcept {
    return attempt == 1 ? 750u : attempt == 2 ? 1500u : 0u;
}
// Optional preview fetches consume only an unambiguous complete HTTP success,
// never a 404/429 JSON error document or a 206 partial body.
inline bool usable_preview_http_response(std::string_view status_line) noexcept {
    return parse_http_status_code(status_line) == 200;
}
}
