#include "../discogger_tree_snapshot.h"
#include <cassert>
#include <cstddef>
#include <limits>
#include <memory>
#include <vector>
struct fake_tracks { std::size_t count = 0; std::size_t size() const { return count; } };
struct fake_disc { fake_tracks tracks; };
struct fake_release {
    struct disc_list {
        std::vector<std::shared_ptr<fake_disc>> items;
        std::size_t get_count() const { return items.size(); }
        const std::shared_ptr<fake_disc>& operator[](std::size_t i) const { return items[i]; }
    } discs;
};
int main() {
    using discogger_tree::row_kind;
    using discogger_tree::valid_row_indices;
    assert(valid_row_indices(row_kind::master, 0, 9999, 1, 0, 0));
    assert(!valid_row_indices(row_kind::master, 1, 0, 1, 0, 0));
    assert(valid_row_indices(row_kind::subrelease, 0, 1, 1, 2, 0));
    assert(!valid_row_indices(row_kind::subrelease, 0, 2, 1, 2, 0));
    assert(!valid_row_indices(row_kind::subrelease, 1, 0, 1, 2, 0));
    assert(valid_row_indices(row_kind::standalone, 9999, 0, 0, 0, 1));
    assert(!valid_row_indices(row_kind::standalone, 0, 1, 0, 0, 1));
    assert(!valid_row_indices(row_kind::invalid, 0, 0, 1, 1, 1));
    assert(!valid_row_indices(row_kind::master,
        (std::numeric_limits<std::size_t>::max)(), 0, 1, 0, 0));
    std::shared_ptr<fake_release> missing;
    assert((discogger_tree::count_cached_tracks(missing) == std::pair<std::size_t, std::size_t>{0, 0}));
    auto release = std::make_shared<fake_release>();
    assert((discogger_tree::count_cached_tracks(release) == std::pair<std::size_t, std::size_t>{0, 0}));
    release->discs.items.push_back(std::make_shared<fake_disc>(fake_disc{{3}}));
    release->discs.items.push_back(nullptr);
    release->discs.items.push_back(std::make_shared<fake_disc>(fake_disc{{2}}));
    assert((discogger_tree::count_cached_tracks(release) == std::pair<std::size_t, std::size_t>{2, 5}));
    release->discs.items[0]->tracks.count = (std::numeric_limits<std::size_t>::max)();
    assert((discogger_tree::count_cached_tracks(release) == std::pair<std::size_t, std::size_t>{0, 0}));
}
