#include "../discogger_search_visibility.h"
#include <cassert>
int main() {
    using discogger_search::resolve_result_filter;
    const auto alt = resolve_result_filter(true, 0, 3);
    assert(!alt.exact_only && alt.show_alternatives);
    const auto alt_unchecked = resolve_result_filter(false, 0, 3);
    assert(!alt_unchecked.exact_only && alt_unchecked.show_alternatives);
    const auto mixed = resolve_result_filter(true, 2, 5);
    assert(mixed.exact_only && !mixed.show_alternatives);
    const auto exact = resolve_result_filter(true, 1, 0);
    assert(exact.exact_only && !exact.show_alternatives);
    const auto empty = resolve_result_filter(true, 0, 0);
    assert(empty.exact_only && !empty.show_alternatives);
    const auto empty_unchecked = resolve_result_filter(false, 0, 0);
    assert(!empty_unchecked.exact_only && !empty_unchecked.show_alternatives);
    return 0;
}
