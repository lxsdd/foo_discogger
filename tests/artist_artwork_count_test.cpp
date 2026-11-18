#include "../discogger_artist_artwork_count.h"
#include <cassert>
#include <cstddef>
#include <memory>
#include <vector>
struct Images { std::size_t count; std::size_t get_count() const { return count; } };
struct FullArtist { Images images; };
struct Credit { std::shared_ptr<FullArtist> full_artist; };
int main() {
    using discogger_ui::artist_artwork_count;
    std::shared_ptr<Credit> absent;
    auto pending = std::make_shared<Credit>();
    auto resolved = std::make_shared<Credit>();
    resolved->full_artist = std::make_shared<FullArtist>(FullArtist{{3}});
    assert(artist_artwork_count(absent) == 0);
    assert(artist_artwork_count(pending) == 0);
    assert(artist_artwork_count(resolved) == 3);
    std::vector<std::shared_ptr<Credit>> artists{pending, resolved, absent};
    std::size_t total = 0;
    for (const auto& artist : artists) total += artist_artwork_count(artist);
    assert(total == 3);
    return 0;
}
