#include "../discogger_collection_transaction.h"
#include <cassert>
#include <stdexcept>
#include <vector>
int main() {
    const std::vector<int> cached{7, 8};
    int checks = 0;
    auto parse = [](std::vector<int>& staged, std::size_t i) {
        staged.push_back(static_cast<int>(i + 100));
    };
    auto full = discogger_collection::stage_collection_pages<std::vector<int>>(
        3, parse, [&] { ++checks; });
    assert((full == std::vector<int>{100,101,102}) && checks == 4);
    for (int failure = 0; failure < 6; ++failure) {
        bool rejected = false;
        int step = 0;
        try {
            (void)discogger_collection::stage_collection_pages<std::vector<int>>(
                failure == 0 ? 0 : 3,
                [&](std::vector<int>& staged, std::size_t i) {
                    if (failure == 5 && i == 2) throw std::runtime_error("malformed page");
                    parse(staged, i);
                },
                [&] { if (failure > 0 && failure < 5 && ++step == failure)
                    throw std::runtime_error("cancelled"); });
        } catch (const std::exception&) { rejected = true; }
        assert(rejected && (cached == std::vector<int>{7, 8}));
    }
    return 0;
}
