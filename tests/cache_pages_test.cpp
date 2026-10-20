#include "../discogger_cache_pages.h"
#include <cassert>

int main() {
    using namespace discogger_cache;
    std::size_t n = 999;
    assert(parse_cache_page_folder("page-0", n) && n == 0);
    assert(parse_cache_page_folder("page-17", n) && n == 17);
    assert(parse_cache_page_folder("page-9999", n) && n == 9999);
    for (const auto bad : {"page-", "page--1", "page-01", "page-10000",
         "page-999999999999999999999", "page-1x", "page- 1",
         "page-1/root.json", "pages-1", "page-+1"}) {
        assert(!parse_cache_page_folder(bad, n));
    }
    assert(complete_cache_page_sequence({0}));
    assert(complete_cache_page_sequence({0, 1, 2}));
    assert(!complete_cache_page_sequence({}));
    assert(!complete_cache_page_sequence({1, 2}));
    assert(!complete_cache_page_sequence({0, 2}));
    assert(!complete_cache_page_sequence({0, 1, 1}));
    assert(valid_cache_pagination(0, 3, 1, 3, 100, 201));
    assert(valid_cache_pagination(1, 3, 2, 3, 100, 201));
    assert(valid_cache_pagination(2, 3, 3, 3, 100, 201));
    assert(valid_cache_pagination(0, 1, 1, 1, 100, 0));
    assert(!valid_cache_pagination(0, 2, 1, 3, 100, 201));
    assert(!valid_cache_pagination(1, 3, 1, 3, 100, 201));
    assert(!valid_cache_pagination(0, 3, 1, 3, 0, 201));
    assert(!valid_cache_pagination(0, 3, 1, 3, 101, 201));
    assert(!valid_cache_pagination(0, 3, 1, 3, 100, 0));
    assert(!valid_cache_pagination(0, 3, 1, 3, 100, 200));
    assert(!valid_cache_pagination(0, 0, 1, 0, 100, 0));
    assert(valid_cache_payload_count(0, 3, 100, 201, 100));
    assert(valid_cache_payload_count(1, 3, 100, 201, 100));
    assert(valid_cache_payload_count(2, 3, 100, 201, 1));
    assert(valid_cache_payload_count(0, 1, 100, 0, 0));
    assert(!valid_cache_payload_count(0, 3, 100, 201, 99));
    assert(!valid_cache_payload_count(2, 3, 100, 201, 0));
    assert(!valid_cache_payload_count(2, 3, 100, 201, 2));
    assert(!valid_cache_payload_count(0, 1, 100, 0, 1));
}
