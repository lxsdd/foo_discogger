#pragma once
#include <utility>
#include <set>
#include <string>
#include <string_view>
#include "discogger_release_id.h"

// An advanced Discogs search can parse several pages, try optional providers,
// and load native foobar previews before it has a complete result. Never
// mutate caller-visible exact/other buckets until all stages have succeeded
// and the caller has checked cancellation. No network, cache or tag writes.
namespace discogger_search {
inline constexpr std::string_view review_only_group_name =
    "Broader Discogs suggestions (review required)";
inline bool is_review_only_group(std::string_view id, std::string_view name) noexcept {
    return id == "0" && name == review_only_group_name;
}
class review_only_identity_filter {
    std::set<std::string> original_releases_, original_masters_;
    std::set<std::string> emitted_releases_, emitted_masters_;
public:
    void remember_original(bool master, std::string_view id) {
        const auto key = canonical_release_id(id);
        if (!key.empty()) (master ? original_masters_ : original_releases_).insert(key);
    }
    bool allow_suggestion(bool master, std::string_view id) {
        const auto key = canonical_release_id(id);
        if (key.empty()) return false;
        const auto& original = master ? original_masters_ : original_releases_;
        auto& emitted = master ? emitted_masters_ : emitted_releases_;
        return original.find(key) == original.end() && emitted.insert(key).second;
    }
};
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
