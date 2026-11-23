#include "../discogger_preview_selection.h"
#include "../discogger_preview_ownership.h"
#include <vector>
#include <cassert>
#include <cstddef>
#include <limits>

int main() {
    using discogger_ui::choose_preview_detail;
    using discogger_ui::preview_source_matches;
    discogger_ui::preview_request_versions versions;
    const auto first = versions.next(0, false);
    assert(versions.matches(0, false, first));
    const auto second = versions.next(0, false); // refresh same source
    assert(!versions.matches(0, false, first)); // stale completion
    assert(versions.matches(0, false, second));
    const auto artist = versions.next(0, true);
    const auto other_slot = versions.next(1, false);
    assert(versions.matches(0, true, artist));
    assert(versions.matches(1, false, other_slot));
    assert(versions.matches(0, false, second));
    assert(!versions.matches(0, false, 0));
    const int release_a = 1, release_b = 2;
    const int* a = &release_a;
    const int* b = &release_b;
    std::vector<int> files_a{10, 20};
    std::vector<int> same_files{10, 20};
    std::vector<int> swapped_files{10, 30};
    std::vector<int> shorter_files{10};
    assert(preview_source_matches(a, a, &files_a, &same_files));
    assert(!preview_source_matches(a, b, &files_a, &same_files));
    assert(!preview_source_matches(a, a, &files_a, &swapped_files));
    assert(!preview_source_matches(a, a, &files_a, &shorter_files));
    assert(preview_source_matches(a, a, static_cast<const std::vector<int>*>(nullptr), static_cast<const std::vector<int>*>(nullptr)));
    assert(!preview_source_matches(a, a, &files_a, static_cast<const std::vector<int>*>(nullptr)));
    assert(!preview_source_matches(a, a, static_cast<const std::vector<int>*>(nullptr), &same_files));
    using discogger_ui::can_fetch_file_artwork_preview;
    assert(!can_fetch_file_artwork_preview(false, 0));
    assert(!can_fetch_file_artwork_preview(true, 0));
    assert(!can_fetch_file_artwork_preview(false, 1));
    assert(can_fetch_file_artwork_preview(true, 1));
    assert(can_fetch_file_artwork_preview(true, 3));
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
