#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

namespace discogger_write_safety {
struct tag_deletion_report {
    std::size_t removed_fields = 0;
    std::size_t removed_values = 0;
    bool cuesheet_modified = false;
};

// Pure read-only comparison: matches the foobar2000 file_info metadata API.
// CUESHEET content is immutable through the generic physical tag-writer.
template <class Before, class After, class RemovedName>
tag_deletion_report inspect_tag_deletions(const Before& original,
    const After& proposed, RemovedName&& removed_name) {
    tag_deletion_report result;
    const auto old_cue = original.meta_find("CUESHEET");
    const auto new_cue = proposed.meta_find("CUESHEET");
    if ((old_cue == SIZE_MAX) != (new_cue == SIZE_MAX)) {
        result.cuesheet_modified = true;
    } else if (old_cue != SIZE_MAX) {
        const auto old_n = original.meta_enum_value_count(old_cue);
        const auto new_n = proposed.meta_enum_value_count(new_cue);
        if (old_n != new_n) {
            result.cuesheet_modified = true;
        } else {
            for (std::size_t i = 0; i < old_n; ++i) {
                const char* lhs = original.meta_enum_value(old_cue, i);
                const char* rhs = proposed.meta_enum_value(new_cue, i);
                if (!lhs || !rhs || std::strcmp(lhs, rhs) != 0) {
                    result.cuesheet_modified = true;
                    break;
                }
            }
        }
    }
    for (std::size_t i = 0; i < original.meta_get_count(); ++i) {
        const char* name = original.meta_enum_name(i);
        if (!name) continue;
        const auto new_field = proposed.meta_find(name);
        if (new_field == SIZE_MAX) {
            ++result.removed_fields;
            removed_name(name);
            continue;
        }
        const auto previous_count = original.meta_enum_value_count(i);
        const auto next_count = proposed.meta_enum_value_count(new_field);
        // A same-sized multivalue edit can still remove a pre-existing
        // value (e.g. GENRE=Trance;Progressive -> Trance;House). Compare
        // the exact values as a multiset, including duplicate multiplicity.
        // Single-value replacements remain ordinary edits, not removals.
        std::size_t removed = 0;
        if (previous_count <= 1) {
            if (previous_count > next_count) removed = previous_count - next_count;
        } else {
            std::vector<bool> matched(next_count, false);
            for (std::size_t old_value = 0; old_value < previous_count; ++old_value) {
                const char* old_text = original.meta_enum_value(i, old_value);
                bool found = false;
                if (old_text) {
                    for (std::size_t new_value = 0; new_value < next_count; ++new_value) {
                        if (matched[new_value]) continue;
                        const char* new_text = proposed.meta_enum_value(new_field, new_value);
                        if (new_text && std::strcmp(old_text, new_text) == 0) {
                            matched[new_value] = true;
                            found = true;
                            break;
                        }
                    }
                }
                if (!found) ++removed;
            }
        }
        if (removed) {
            result.removed_values += removed;
            removed_name(name);
        }
    }
    return result;
}
// Approval masks use a bounded search, not an accidental boolean count.
template<class Mask>
bool has_any_user_approval(const Mask& approvals) {
    const auto count = approvals.size();
    return count > 0 && approvals.find_first(true, 0, count) < count;
}
} // namespace discogger_write_safety
