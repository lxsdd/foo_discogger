#include "../discogger_artist_load_ownership.h"
#include <cassert>
#include <string>
#include <vector>

int main() {
    const std::vector<std::size_t> duplicates{100,200,100,300,200,0,0};
    assert((discogger_async::unique_artist_ids(duplicates) == std::vector<std::size_t>{100,200,300,0}));
    assert(discogger_async::unique_artist_ids(std::vector<std::size_t>{}).empty());

    auto equal = [](const std::string& a, const std::string& b) { return a == b; };
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
