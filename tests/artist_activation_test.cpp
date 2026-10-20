#include "../discogger_artist_activation.h"
#include <cassert>
#include <cstddef>
int main() {
    using discogger_ui::should_load_artist_on_double_click;
    using discogger_ui::valid_artist_list_row;
    const std::size_t invalid = static_cast<std::size_t>(-1);
    assert(valid_artist_list_row(0, 1));
    assert(valid_artist_list_row(1, 2));
    assert(!valid_artist_list_row(-1, 3));
    assert(!valid_artist_list_row(0, 0));
    assert(!valid_artist_list_row(2, 2));
    assert(!valid_artist_list_row(2147483647, 1));
    assert(should_load_artist_on_double_click(0, 0, 1));
    assert(should_load_artist_on_double_click(2, 2, 3));
    assert(!should_load_artist_on_double_click(-1, 0, 1)); // blank area
    assert(!should_load_artist_on_double_click(0, invalid, 1)); // no selection
    assert(!should_load_artist_on_double_click(2, 1, 3)); // old selection
    assert(!should_load_artist_on_double_click(2, 2, 2)); // stale row
    assert(!should_load_artist_on_double_click(0, 0, 0)); // empty list
    assert(!should_load_artist_on_double_click(-2, invalid, 0));
    return 0;
}
