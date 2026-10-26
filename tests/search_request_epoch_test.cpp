#include "../discogger_search_request.h"
#include <cassert>
#include <cstdint>
int main() {
    using discogger_async::search_request_epoch;
    search_request_epoch request;
    const auto initial = request.current();
    assert(initial != 0 && request.matches(initial));
    assert(!request.matches(0));
    const auto first = request.begin(); // First artist search accepted.
    assert(request.matches(first) && !request.matches(initial));
    const auto second = request.begin(); // New direct-ID or advanced search.
    assert(request.matches(second) && !request.matches(first));
    // A modeless presentation completion from the first query is stale
    // even when it targets the SAME still-open foobar dialog HWND.
    bool presented = false;
    if (request.matches(first)) presented = true;
    assert(!presented);
    if (request.matches(second)) presented = true;
    assert(presented);
    // A failed/aborted task cannot reactivate its previous ticket.
    assert(!request.matches(first) && request.matches(second));
    // The release-tree double-click is another async request. Selecting a
    // release invalidates an in-flight artist search and the previous release
    // on the same HWND; only the newest selection may open track matching.
    const auto release_a = request.begin();
    assert(request.matches(release_a) && !request.matches(second));
    const auto release_b = request.begin();
    assert(request.matches(release_b) && !request.matches(release_a));
    // Aborting an old release cannot restore an obsolete dialog's visibility.
    bool old_abort_can_show = request.matches(release_a);
    assert(!old_abort_can_show);
    // A later search also supersedes a pending release detail callback.
    const auto later_search = request.begin();
    assert(request.matches(later_search) && !request.matches(release_b));
    for (int i = 0; i != 8; ++i) {
        const auto previous = request.current();
        const auto current = request.begin();
        assert(!request.matches(previous) && request.matches(current));
    }
    // A->B->A text edits can have the same final filter string: equality
    // of text is insufficient to accept a delayed filter completion.
    search_request_epoch filter;
    const auto filter_a = filter.begin();
    const auto filter_b = filter.begin();
    const auto filter_a_again = filter.begin();
    assert(!filter.matches(filter_a) && !filter.matches(filter_b));
    assert(filter.matches(filter_a_again));
    // A new search also invalidates a master-expansion or filter callback,
    // even if the query mode and visible filter text remain identical.
    const auto old_search = request.current();
    const auto old_filter = filter.current();
    const auto next_search = request.begin();
    assert(!request.matches(old_search) && request.matches(next_search));
    assert(filter.matches(old_filter)); // Independent generation, both checked.
    const auto next_filter = filter.begin(); // Checkbox change or edit.
    assert(!filter.matches(old_filter) && filter.matches(next_filter));
    return 0;
}
