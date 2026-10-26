#pragma once

#include <cstddef>
#include <cstdint>
#include <ios>

// Cached thumbnails are untrusted disk input: bound allocations, and reject
// partial reads rather than returning truncated image bytes to the UI.
namespace discogger_thumbnail {
inline constexpr std::uintmax_t max_cache_bytes = 16u * 1024u * 1024u;

inline bool valid_cache_size(std::uintmax_t bytes) noexcept {
    return bytes > 0 && bytes <= max_cache_bytes;
}

// Map a flattened artist-image index without cumulative-size overflow.
// Zero-image artists are skipped; a miss leaves remaining as the residual.
inline bool consume_artist_images(std::size_t image_count, std::size_t& remaining) noexcept {
    if (remaining < image_count) return true;
    remaining -= image_count;
    return false;
}

inline bool complete_read(std::streamsize received, std::size_t expected) noexcept {
    return received >= 0 && static_cast<std::uintmax_t>(received) == expected;
}
} // namespace discogger_thumbnail
