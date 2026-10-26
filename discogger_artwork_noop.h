#pragma once
// SDK-independent exact-byte artwork preflight. No physical file is modified here.
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace discogger_write_safety {
inline bool artwork_bytes_equal(
    const void* existing, std::size_t existing_len,
    const void* desired, std::size_t desired_len) noexcept {
    // Empty/invalid data must not silently qualify as a successful idempotent write.
    if (existing_len == 0 || desired_len == 0) return false;
    if (existing_len != desired_len || existing == nullptr || desired == nullptr) return false;
    return std::memcmp(existing, desired, desired_len) == 0;
}

// Compare large existing sidecar artwork in bounded buffers; short reads fail
// closed rather than authorizing truncation of a previously valid destination.
template<class ReadChunk>
bool artwork_stream_matches(
    std::size_t old_len, const void* desired, std::size_t desired_len,
    ReadChunk&& reader) {
    if (old_len != desired_len || desired_len == 0 || desired == nullptr) return false;
    const auto* next = static_cast<const unsigned char*>(desired);
    unsigned char chunk[16384];
    std::size_t offset = 0;
    while (offset < desired_len) {
        const auto remaining = desired_len - offset;
        const auto n = remaining < sizeof(chunk) ? remaining : sizeof(chunk);
        if (std::forward<ReadChunk>(reader)(chunk, n) != n) {
            throw std::runtime_error("Short read while validating existing artwork");
        }
        if (std::memcmp(chunk, next + offset, n) != 0) return false;
        offset += n;
    }
    return true;
}
}
