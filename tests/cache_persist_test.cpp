#include "../discogger_cache_persist.h"
#include <cassert>
#include <new>
#include <stdexcept>
#include <string>
struct canceled : std::runtime_error {
    canceled() : std::runtime_error("canceled") {}
};
int main() {
    using discogger_cache::persist_optional_cache;
    using discogger_cache::release_cache_artist_id;
    // A release with no parsed artist must still load, but cannot create an
    // ambiguous disk-cache directory. A valid supplied ID takes precedence.
    assert(release_cache_artist_id("194", "355") == "194");
    assert(release_cache_artist_id("", "355") == "355");
    assert(release_cache_artist_id("0", "37332711") == "37332711");
    assert(release_cache_artist_id("999999999999999999", "38012673") == "38012673");
    assert(release_cache_artist_id("-1", "") == "");
    assert(release_cache_artist_id("", "") == "");
    assert(release_cache_artist_id("12oops", "bad") == "");
    bool abort = false;
    int checks = 0;
    const auto check = [&] { ++checks; if (abort) throw canceled{}; };
    std::string visible = "last good release list";
    assert(persist_optional_cache<canceled>([] { return true; }, check));
    assert(checks == 2 && visible == "last good release list");
    assert(!persist_optional_cache<canceled>([] { return false; }, check));
    assert(!persist_optional_cache<canceled>([]() -> bool {
        throw std::runtime_error("disk unavailable");
    }, check));
    // A complete online result can still be committed when the disk fails.
    visible = "new complete online release list";
    assert(visible == "new complete online release list");
    visible = "last good release list";
    abort = true;
    bool entered = false;
    try {
        persist_optional_cache<canceled>([&] { entered = true; return true; }, check);
        visible = "invalid new result";
        assert(false);
    } catch (const canceled&) {}
    assert(!entered && visible == "last good release list");
    abort = false;
    try {
        persist_optional_cache<canceled>([]() -> bool { throw canceled{}; }, check);
        visible = "invalid new result";
        assert(false);
    } catch (const canceled&) {}
    assert(visible == "last good release list");
    try {
        persist_optional_cache<canceled>([&] { abort = true; return true; }, check);
        visible = "invalid new result";
        assert(false);
    } catch (const canceled&) {}
    assert(visible == "last good release list");
    abort = false;
    try {
        persist_optional_cache<canceled>([]() -> bool { throw std::bad_alloc{}; }, check);
        assert(false);
    } catch (const std::bad_alloc&) {}
    assert(visible == "last good release list");
    abort = false;
    try {
        persist_optional_cache<canceled>([&]() -> bool {
            abort = true;
            throw std::runtime_error("write error while canceled");
        }, check);
        assert(false);
    } catch (const canceled&) {}
    assert(visible == "last good release list");
}
