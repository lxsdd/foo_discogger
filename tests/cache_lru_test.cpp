#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <list>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

// Minimal foobar-style string stub to compile the production header unchanged.
namespace pfc {
struct string8 {
    std::string value;
    string8() = default;
    string8(const char* s) : value(s) {}
    const char* get_ptr() const { return value.c_str(); }
};
}
inline bool STR_EQUAL(const pfc::string8& a, const pfc::string8& b) { return a.value == b.value; }
inline std::pair<int, unsigned long> decode_mr(size_t key) {
    return {static_cast<int>(key >> 26), static_cast<unsigned long>(key & ((1u << 26) - 1))};
}
inline size_t encode_mr(int role, unsigned long id) { return (static_cast<size_t>(role) << 26) | id; }

#include "../cache.h"

// Inject a deterministic value-copy failure into put(), including the
// replacement path. No external test data or foobar2000 installation needed.
struct throwing_value {
    int number;
    static bool reject_copy;
    explicit throwing_value(int n = 0) : number(n) {}
    throwing_value(const throwing_value& rhs) : number(rhs.number) {
        if (reject_copy) throw std::runtime_error("injected copy failure");
    }
    throwing_value& operator=(const throwing_value&) = default;
};
bool throwing_value::reject_copy = false;

int main() {
    lru_cache<size_t, std::shared_ptr<int>> c(2);
    assert(!c.exists(1));
    assert(!c.remove(1));
    auto first = std::make_shared<int>(12);
    c.put(1, first);
    c.put(2, std::make_shared<int>(20));
    assert(c.size() == 2);
    auto snapshot = c.get(1);
    c.put(3, std::make_shared<int>(30));
    assert(c.exists(1) && !c.exists(2) && c.exists(3));
    assert(*snapshot == 12);
    c.put(1, std::make_shared<int>(13));
    assert(*c.get(1) == 13 && *snapshot == 12);
    assert(c.remove(3));
    assert(!c.remove(3));
    c.set_max_size(0);
    assert(c.size() == 0);
    c.put(4, std::make_shared<int>(40));
    assert(c.size() == 0);
    bool threw = false;
    try { (void)c.get(4); } catch (const std::out_of_range&) { threw = true; }
    assert(threw);
    c.set_max_size(3);
    std::shared_ptr<int> missing;
    assert(!c.try_get(7, missing));
    c.put(7, first);
    assert(c.try_get(7, missing) && missing == first);
    c.clear();
    assert(c.size() == 0);

    // An unsuccessful replacement must not evict or change an existing value.
    lru_cache<size_t, throwing_value> fail_cache(2);
    fail_cache.put(1, throwing_value(11));
    fail_cache.put(2, throwing_value(22));
    throwing_value::reject_copy = true;
    bool copy_failed = false;
    try { fail_cache.put(1, throwing_value(99)); }
    catch (const std::runtime_error&) { copy_failed = true; }
    throwing_value::reject_copy = false;
    assert(copy_failed && fail_cache.size() == 2);
    assert(fail_cache.get(1).number == 11);
    assert(fail_cache.get(2).number == 22);

    // An unsuccessful insertion must not change the existing LRU capacity.
    throwing_value::reject_copy = true;
    copy_failed = false;
    try { fail_cache.put(3, throwing_value(33)); }
    catch (const std::runtime_error&) { copy_failed = true; }
    throwing_value::reject_copy = false;
    assert(copy_failed && fail_cache.size() == 2);
    assert(!fail_cache.exists(3));
    fail_cache.put(1, throwing_value(44));
    assert(fail_cache.get(1).number == 44);
    fail_cache.put(3, throwing_value(55));
    assert(fail_cache.exists(1) && fail_cache.exists(3) && !fail_cache.exists(2));

    lru_cache<size_t, int> role_cache(4);
    const auto a = encode_mr(4, 12001);
    const auto b = encode_mr(7, 12001);
    role_cache.put(a, 4);
    role_cache.put(b, 7);
    size_t alternate = 999;
    auto flags = role_cache.exists_comp(12001, alternate);
    assert(!flags.first && !flags.second && alternate == b);
    (void)role_cache.get(a);
    flags = role_cache.exists_comp(12001, alternate);
    assert(!flags.first && !flags.second && alternate == a);
    role_cache.put(12001, 0);
    flags = role_cache.exists_comp(12001, alternate);
    assert(flags.first && flags.second && alternate == 0);
    flags = role_cache.exists_comp(encode_mr(9, 12001), alternate);
    assert(flags.first && !flags.second && alternate == 0);
    flags = role_cache.exists_comp(encode_mr(1, 99999), alternate);
    assert(!flags.first && !flags.second && alternate == 0);

    lru_cache<pfc::string8, bool> strings(2);
    strings.put(pfc::string8("abc"), true);
    bool found = false;
    assert(strings.try_get(pfc::string8("abc"), found) && found);
    strings.put(pfc::string8("def"), false);
    strings.put(pfc::string8("ghi"), true);
    assert(!strings.exists(pfc::string8("abc")));

    lru_cache<size_t, std::shared_ptr<int>> parallel(48);
    std::atomic<bool> good{true};
    std::vector<std::thread> threads;
    for (size_t worker = 0; worker < 6; ++worker) {
        threads.emplace_back([worker, &parallel, &good] {
            for (size_t i = 0; i < 1200; ++i) {
                const size_t key = worker * 10000 + i % 24;
                parallel.put(key, std::make_shared<int>(static_cast<int>(i)));
                std::shared_ptr<int> got;
                if (parallel.try_get(key, got) && !got) good = false;
                if (i % 5 == 0) parallel.remove(key);
            }
        });
    }
    for (auto& thread : threads) thread.join();
    assert(good && parallel.size() <= 48);
}
