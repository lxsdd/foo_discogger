#include <algorithm>
#include <cassert>
#include <chrono>
// Mimic Windows headers' function-like max macro while compiling our policy.
#define max(a, b) forbidden_legacy_windows_max_macro
#include "../discogger_cache_expiry.h"
#undef max
using namespace std::chrono;
int main() {
    using discogger_cache::group;
    using discogger_cache::ttl;
    using discogger_cache::expired;
    assert(discogger_cache::artist_hours(0) == 1);
    assert(discogger_cache::artist_hours(10000) == 720);
    assert(discogger_cache::entity_days(0) == 1);
    const seconds artist = ttl(group::artist_releases, 24, false, 30);
    const seconds entity = ttl(group::other_entity, 24, true, 30);
    assert(artist == hours(24) && entity == hours(720));
    assert(ttl(group::other_entity, 24, false, 30) == seconds::max());
    assert(!expired(hours(23), artist));
    assert(!expired(artist - seconds(1), artist));
    assert(expired(artist, artist) && expired(artist + seconds(1), artist));
    assert(!expired(-minutes(5), artist) && expired(-minutes(6), artist));
    assert(!expired(hours(50000), seconds::max()));
    assert(ttl(group::artist_releases, 1, true, 365) == hours(1));
    assert(ttl(group::artist_releases, 720, false, 0) == hours(720));
    assert(ttl(group::other_entity, 2, true, 0) == hours(24));
}
