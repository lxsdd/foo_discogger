#include "../discogger_preview_completion.h"
#include <cassert>
#include <cstddef>
#include <limits>

int main() {
    using discogger_ui::complete_preview_jobs;
    std::size_t pending = 3;
    assert(!complete_preview_jobs(pending, 0) && pending == 3);
    assert(!complete_preview_jobs(pending, 1) && pending == 2);
    assert(complete_preview_jobs(pending, 2) && pending == 0);
    assert(!complete_preview_jobs(pending, 1) && pending == 0);
    pending = 2;
    assert(complete_preview_jobs(pending, 3) && pending == 0);
    pending = 1;
    assert(complete_preview_jobs(pending, (std::numeric_limits<std::size_t>::max)()) && pending == 0);
    pending = 0;
    assert(!complete_preview_jobs(pending, 0) && pending == 0);
}
