#pragma once
#include <cstddef>
#include <limits>

// The provider's page position is not an index into an existing partial
// advanced-search version list. Match the actual release identity instead.
namespace discogger_master {
template <typename Predicate>
inline std::size_t find_partial_version_index(std::size_t count, Predicate&& matches) {
    for (std::size_t index = 0; index < count; ++index) {
        if (matches(index)) return index;
    }
    return (std::numeric_limits<std::size_t>::max)();
}
}
