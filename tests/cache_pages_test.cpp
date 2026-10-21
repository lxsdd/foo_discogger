#include "../discogger_cache_pages.h"
#include <cassert>

int main() {
    using namespace discogger_cache;
    // Discogs live pagination: zero-result pages, consistency across requests,
    // bounded page counts and fail-closed truncated/changed metadata.
    assert(valid_online_page(1, 1, 3, 100, 201, 0, 0, 0));
    assert(valid_online_page(2, 2, 3, 100, 201, 3, 100, 201));
    assert(valid_online_page(3, 3, 3, 100, 201, 3, 100, 201));
    assert(valid_online_page(1, 1, 0, 100, 0, 0, 0, 0));
    assert(valid_online_page(1, 1, 1, 100, 0, 0, 0, 0));
    // A Discogs response must report exactly ceil(items/per_page) pages:
    // otherwise a partial list can be incorrectly committed as complete.
    assert(valid_online_page(1, 1, 1, 100, 100, 0, 0, 0));
    assert(valid_online_page(1, 1, 2, 100, 101, 0, 0, 0));
    assert(!valid_online_page(1, 1, 1, 100, 101, 0, 0, 0));
    assert(!valid_online_page(1, 1, 2, 100, 201, 0, 0, 0));
    assert(!valid_online_page(1, 1, 4, 100, 201, 0, 0, 0));
    assert(!valid_online_page(1, 1, 3, 100, 200, 0, 0, 0));
    assert(valid_online_page(1, 1, 0, 100, 0, 0, 0, 0));
    assert(!valid_online_page(1, 1, 0, 100, 1, 0, 0, 0));
    assert(!valid_online_page(1, 1, 1, 100,
        static_cast<std::size_t>(-1), 0, 0, 0));
    assert(!valid_online_page(0, 0, 3, 100, 201, 0, 0, 0));
    assert(!valid_online_page(1, 0, 3, 100, 201, 0, 0, 0));
    assert(!valid_online_page(1, 2, 3, 100, 201, 0, 0, 0));
    assert(!valid_online_page(1, 1, 0, 100, 1, 0, 0, 0));
    assert(!valid_online_page(1, 1, 2, 100, 0, 0, 0, 0));
    assert(!valid_online_page(1, 1, max_cached_pages + 1, 100, 201, 0, 0, 0));
    assert(!valid_online_page(1, 1, 3, 0, 201, 0, 0, 0));
    assert(!valid_online_page(1, 1, 3, 101, 201, 0, 0, 0));
    assert(!valid_online_page(2, 2, 2, 100, 201, 3, 100, 201));
    assert(!valid_online_page(2, 2, 3, 50, 201, 3, 100, 201));
    assert(!valid_online_page(2, 2, 3, 100, 202, 3, 100, 201));
    assert(!valid_online_page(4, 4, 3, 100, 201, 3, 100, 201));
    assert(!valid_online_page(max_cached_pages + 1, max_cached_pages + 1,
                              max_cached_pages, 100, 201, max_cached_pages, 100, 201));
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
    // Online search/artist/collection/version pages must contain exactly
    // as many objects as promised by the Discogs pagination metadata.
    assert(valid_online_payload_count(1, 0, 100, 0, 0));
    assert(valid_online_payload_count(1, 1, 100, 0, 0));
    assert(valid_online_payload_count(1, 3, 100, 201, 100));
    assert(valid_online_payload_count(2, 3, 100, 201, 100));
    assert(valid_online_payload_count(3, 3, 100, 201, 1));
    assert(!valid_online_payload_count(1, 0, 100, 201, 0));
    assert(!valid_online_payload_count(1, 1, 100, 0, 1));
    assert(!valid_online_payload_count(1, 3, 100, 201, 99));
    assert(!valid_online_payload_count(2, 3, 100, 201, 99));
    assert(!valid_online_payload_count(3, 3, 100, 201, 0));
    assert(!valid_online_payload_count(3, 3, 100, 201, 2));
    assert(!valid_online_payload_count(0, 3, 100, 201, 100));
    assert(!valid_online_payload_count(4, 3, 100, 201, 0));
    // Live pages also need complete payloads; metadata alone is insufficient.
    assert(valid_cache_payload_count(0, 3, 100, 201, 100));
    assert(valid_cache_payload_count(1, 3, 100, 201, 100));
    assert(valid_cache_payload_count(2, 3, 100, 201, 1));
    assert(!valid_cache_payload_count(0, 3, 100, 201, 0));
    assert(!valid_cache_payload_count(1, 3, 100, 201, 99));
    assert(!valid_cache_payload_count(2, 3, 100, 201, 100));
    assert(!valid_cache_payload_count(0, 1, 100, 0, 1));
    assert(!valid_cache_payload_count(0, 3, 100, 201, 99));
    assert(!valid_cache_payload_count(2, 3, 100, 201, 0));
    assert(!valid_cache_payload_count(2, 3, 100, 201, 2));
    assert(!valid_cache_payload_count(0, 1, 100, 0, 1));
}
