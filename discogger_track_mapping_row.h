#pragma once
#include <cstddef>
#include <limits>

// A Discogs row without a corresponding physical file must never inherit
// the preceding row's file index, including after list reorder/crop.
namespace discogger_matching {
inline int mapping_file_index(std::size_t row, std::size_t file_count,
                              std::size_t original_index, std::size_t invalid_index) noexcept {
    if (row >= file_count || original_index == invalid_index ||
        original_index > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
        return -1;
    return static_cast<int>(original_index);
}
} // namespace discogger_matching
