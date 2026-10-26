#pragma once
#include <cstddef>

namespace discogger_search {
struct result_filter_decision {
    bool exact_only;
    bool show_alternatives;
};
// If the search returns only non-exact suggestions, do not keep an old
// checkbox state that hides all of them while visually clearing the checkbox.
inline result_filter_decision resolve_result_filter(
    bool exact_only_requested, std::size_t exact_count, std::size_t other_count) {
    const bool show_alternatives = exact_count == 0 && other_count != 0;
    return {exact_only_requested && !show_alternatives, show_alternatives};
}
} // namespace discogger_search
