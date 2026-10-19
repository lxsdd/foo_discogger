#pragma once
#include <cstdint>

// Stable CRC-32/ISO-HDLC of query UTF-8 bytes, used only as a history key.
// Portable implementation: no third-party header or runtime dependency.
namespace discogger_crc {
template <typename Iterator>
std::uint32_t crc32(Iterator first, Iterator last) {
    std::uint32_t state = 0xFFFFFFFFu;
    for (; first != last; ++first) {
        state ^= static_cast<unsigned char>(*first);
        for (unsigned bit = 0; bit < 8; ++bit) {
            state = (state >> 1) ^ ((state & 1u) ? 0xEDB88320u : 0u);
        }
    }
    return ~state;
}
} // namespace discogger_crc
