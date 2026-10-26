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
    return 0;
}
