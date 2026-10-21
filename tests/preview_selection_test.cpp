#include "../discogger_preview_selection.h"
#include <cassert>
#include <cstddef>
#include <limits>

int main() {
    using discogger_ui::choose_preview_detail;
    auto empty = choose_preview_detail(0, 0, true);
    assert(!empty.valid && empty.index == 0);
    empty = choose_preview_detail((std::numeric_limits<std::size_t>::max)(), 0, false);
    assert(!empty.valid && empty.index == 0);
    auto current = choose_preview_detail(0, 1, true);
    assert(current.valid && current.index == 0);
    current = choose_preview_detail(0, 1, false);
    assert(current.valid && current.index == 0); // no size_t underflow
    current = choose_preview_detail(2, 3, true);
    assert(current.valid && current.index == 2);
    current = choose_preview_detail(2, 3, false);
    assert(current.valid && current.index == 1);
    current = choose_preview_detail(3, 3, false);
    assert(current.valid && current.index == 2);
    current = choose_preview_detail(3000, 2, true);
    assert(current.valid && current.index == 1);
    current = choose_preview_detail(
        (std::numeric_limits<std::size_t>::max)(), 1, false);
    assert(current.valid && current.index == 0);
}
