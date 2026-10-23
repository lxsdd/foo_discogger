#pragma once
// Runner-only, non-sensitive state counters for the protected native GUI test.
// No queries, URLs, response bodies, user profiles or OAuth material are stored.
#if defined(DISCOGGER_RUNNER_MEDIA_QA)
#include <atomic>
namespace discogger_live_search_qa {
inline std::atomic<unsigned> button{0};
inline std::atomic<unsigned> routed{0};
inline std::atomic<unsigned> worker_started{0};
inline std::atomic<unsigned> worker_advanced{0};
inline std::atomic<unsigned> worker_success{0};
inline std::atomic<unsigned> worker_error{0};
inline std::atomic<unsigned> worker_abort{0};
inline std::atomic<unsigned> response_pages{0};
inline std::atomic<unsigned> response_items{0};
inline std::atomic<unsigned> parsed_masters{0};
inline std::atomic<unsigned> parsed_releases{0};
inline std::atomic<unsigned> query_limit{0};
inline void reset() noexcept {
    button.store(0);
    routed.store(0);
    worker_started.store(0);
    worker_success.store(0);
    worker_error.store(0);
    worker_abort.store(0);
    response_pages.store(0);
    response_items.store(0);
    parsed_masters.store(0);
    parsed_releases.store(0);
    query_limit.store(0);
}
}
#endif
