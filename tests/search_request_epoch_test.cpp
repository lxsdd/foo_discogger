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
    for (int i = 0; i != 8; ++i) {
        const auto previous = request.current();
        const auto current = request.begin();
        assert(!request.matches(previous) && request.matches(current));
    }
    return 0;
}
