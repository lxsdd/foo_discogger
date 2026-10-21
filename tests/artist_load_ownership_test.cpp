#include "../discogger_artist_load_ownership.h"
#include <cassert>
#include <string>
#include <vector>

int main() {
    const std::vector<std::size_t> duplicates{100,200,100,300,200,0,0};
    assert((discogger_async::unique_artist_ids(duplicates) == std::vector<std::size_t>{100,200,300,0}));
    assert(discogger_async::unique_artist_ids(std::vector<std::size_t>{}).empty());

    auto equal = [](const std::string& a, const std::string& b) { return a == b; };
    const std::vector<std::string> acquired{"12","35"};
    assert(discogger_async::owns_id(acquired, std::string("12"), equal));
    assert(!discogger_async::owns_id(acquired, std::string("13"), equal));
    assert(!discogger_async::owns_id(std::vector<std::string>{}, std::string("12"), equal));
    assert(discogger_async::should_load_owned_releases(true, true));
    assert(!discogger_async::should_load_owned_releases(true, false));
    assert(!discogger_async::should_load_owned_releases(false, true));
    assert(!discogger_async::should_load_owned_releases(false, false));

    // All selected artists are already being loaded by another task.
    // The second task must still visit every artist for preview, while
    // requesting zero redundant release downloads.
    const std::vector<std::string> all_busy{"12","35","88"};
    const std::vector<std::string> none_owned;
    std::size_t visited = 0, duplicate_downloads = 0;
    for (const auto& artist_id : all_busy) {
        ++visited;
        if (discogger_async::should_load_owned_releases(
                true, discogger_async::owns_id(none_owned, artist_id, equal))) {
            ++duplicate_downloads;
        }
    }
    assert(visited == all_busy.size() && duplicate_downloads == 0);

    // Partial overlap still loads only the ID actually acquired by this task.
    const std::vector<std::string> partially_owned{"35"};
    std::size_t partial_downloads = 0;
    for (const auto& artist_id : all_busy) {
        if (discogger_async::should_load_owned_releases(
                true, discogger_async::owns_id(partially_owned, artist_id, equal)))
            ++partial_downloads;
    }
    assert(partial_downloads == 1);
    std::vector<std::string> active{"other", "mine-1", "mine-2"};
    std::vector<std::string> mine{"mine-1", "mine-2"};
    std::vector<std::string> skipped; // "other" belonged to another task.
    assert(discogger_async::release_owned_ids(mine, active, equal) == 2);
    assert(mine.empty() && active == std::vector<std::string>{"other"});
    active.push_back("mine-1"); // A later task registers the same ID.
    assert(discogger_async::release_owned_ids(mine, active, equal) == 0);
    assert(active == (std::vector<std::string>{"other", "mine-1"}));
    // This task cannot unregister "other", which it never claimed.
    assert(discogger_async::release_owned_ids(skipped, active, equal) == 0);
    assert(active == (std::vector<std::string>{"other", "mine-1"}));
    // A stale owned ID must not affect a different current registration.
    std::vector<std::string> old{"missing"};
    assert(discogger_async::release_owned_ids(old, active, equal) == 0);
    assert(old.empty() && active == (std::vector<std::string>{"other", "mine-1"}));
    std::vector<std::string> second{"mine-1"};
    assert(discogger_async::release_owned_ids(second, active, equal) == 1);
    assert(active == std::vector<std::string>{"other"});
    assert(discogger_async::release_owned_ids(second, active, equal) == 0);
    std::vector<std::string> other_owner{"other"};
    assert(discogger_async::release_owned_ids(other_owner, active, equal) == 1);
    assert(active.empty());
}
