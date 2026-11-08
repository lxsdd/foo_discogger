#include "../discogger_collection_transaction.h"
#include <cassert>
#include <stdexcept>
#include <vector>
#include <string>
int main() {
    // Never follow an authenticated folder resource_url outside Discogs API.
    const std::vector<const char*> good = {
        "https://api.discogs.com/users/a/collection/folders/0",
        "https://api.discogs.com/users/a_-%20B/collection/folders/12345",
        "https://api.discogs.com/users/%E2%82%AC/collection/folders/123"
    };
    const std::vector<const char*> bad = {
        "", "http://api.discogs.com/users/a/collection/folders/0",
        "https://api.discogs.com.evil.invalid/users/a/collection/folders/0",
        "https://api.discogs.com@evil.invalid/users/a/collection/folders/0",
        "https://evil.invalid/users/a/collection/folders/0",
        "https://api.discogs.com/users//collection/folders/0",
        "https://api.discogs.com/users/a/collection/folders/",
        "https://api.discogs.com/users/a/collection/folders/0?token=abc",
        "https://api.discogs.com/users/a/collection/folders/0#fragment",
        "https://api.discogs.com/users/a/collection/folders/0/extra",
        "https://api.discogs.com/users/a/collection/folders/-1",
        "https://api.discogs.com/users/a/collection/folders/000000000000000000000",
        "https://api.discogs.com/users/a%2G/collection/folders/0",
        "https://api.discogs.com/users/a%/collection/folders/0",
        "https://api.discogs.com/users/a\\b/collection/folders/0",
        "https://api.discogs.com/users/a/b/collection/folders/0",
        "https://api.discogs.com/users/a/collection/folders/0\r\nHost: attacker"
    };
    for (const auto* url : good) assert(discogger_collection::valid_folder_resource_url(url));
    for (const auto* url : bad) assert(!discogger_collection::valid_folder_resource_url(url));
    assert(!discogger_collection::valid_folder_resource_url(std::string(2049, 'x')));

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
