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
