#pragma once
#include <utility>

// An advanced Discogs search can parse several pages, try optional providers,
// and load native foobar previews before it has a complete result. Never
// mutate caller-visible exact/other buckets until all stages have succeeded
// and the caller has checked cancellation. No network, cache or tag writes.
namespace discogger_search {
template <typename Array>
struct staged_search_result_pair {
    Array exact{};
    Array other{};

    void publish(Array& visible_exact, Array& visible_other) {
        visible_exact = std::move(exact);
        visible_other = std::move(other);
    }
};
} // namespace discogger_search
