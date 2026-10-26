#include "../discogger_master_page_transaction.h"
#include <cassert>
#include <stdexcept>
#include <vector>

int main() {
    using discogger_master::stage_master_pages;
    const std::vector<int> visible{41};
    int checks = 0;
    auto snapshot = [&] { return visible; };
    auto check = [&] { ++checks; };
    auto valid = [](std::size_t) {};
    auto parse = [](std::vector<int>& staged, std::size_t page) {
        staged.push_back(static_cast<int>(page + 42));
    };
    const auto complete = stage_master_pages<std::vector<int>>(
        2, snapshot, valid, parse, check);
    assert((complete == std::vector<int>{41, 42, 43}));
    assert((visible == std::vector<int>{41}));
    assert(checks == 3);

    // A structurally bad second page must not publish the valid first page.
    bool rejected = false;
    try {
        (void)stage_master_pages<std::vector<int>>(
            2, snapshot,
            [](std::size_t page) {
                if (page == 1) throw std::runtime_error("bad page 2");
            }, parse, check);
    } catch (const std::runtime_error&) { rejected = true; }
    assert(rejected && (visible == std::vector<int>{41}));

    // Abort between pages and immediately before commit both leave the
    // caller's visible version list and existing child values unchanged.
    for (int abort_after : {2, 3}) {
        int calls = 0;
        rejected = false;
        try {
            (void)stage_master_pages<std::vector<int>>(
                2, snapshot, valid, parse, [&] {
                    if (++calls == abort_after)
                        throw std::runtime_error("cancelled");
                });
        } catch (const std::runtime_error&) { rejected = true; }
        assert(rejected && (visible == std::vector<int>{41}));
    }
    rejected = false;
    try {
        (void)stage_master_pages<std::vector<int>>(
            0, snapshot, valid, parse, check);
    } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected && (visible == std::vector<int>{41}));

    // An explicitly successful commit happens only after the final check.
    std::vector<int> published = complete;
    assert((published == std::vector<int>{41, 42, 43}));
    return 0;
}
