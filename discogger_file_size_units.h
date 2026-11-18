#pragma once
#include <cstddef>
#include <cstdint>

namespace discogger_file_size {
    struct parts {
        std::size_t unit_index;
        std::uint64_t whole;
        std::uint64_t remainder;
    };

    // The original Win32 artwork list exposes B/KB/MB/GB/TB only.
    // Stop before the last unit; larger values remain representable in TB.
    constexpr std::size_t unit_count = 5;
    constexpr parts split(std::uint64_t bytes) noexcept {
        std::size_t unit_index = 0;
        std::uint64_t remainder = 0;
        while (bytes >= 1024 && unit_index + 1 < unit_count) {
            remainder = bytes % 1024;
            bytes /= 1024;
            ++unit_index;
        }
        return {unit_index, bytes, remainder};
    }
}
