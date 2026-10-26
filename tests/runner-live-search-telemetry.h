#pragma once
// Runner-only, non-sensitive state counters for the protected native GUI test.
// No queries, URLs, response bodies, user profiles or OAuth material are stored.
#if defined(DISCOGGER_RUNNER_MEDIA_QA)
#include <atomic>
namespace discogger_live_search_qa {
inline std::atomic<unsigned> button{0};
inline std::atomic<unsigned> input_nonempty{0};
inline std::atomic<unsigned> input_nonempty_calls{0};
inline std::atomic<unsigned> input_empty_calls{0};
inline std::atomic<unsigned> input_member_valid{0};
inline std::atomic<unsigned> input_hwnd_match{0};
inline std::atomic<unsigned> input_member_length{0};
inline std::atomic<unsigned> input_live_length{0};
inline std::atomic<unsigned> parsed_fields{0};
inline std::atomic<unsigned> query_minimal{0};
inline std::atomic<unsigned> query_detected{0};
inline std::atomic<unsigned> alt_mode{0};
inline std::atomic<unsigned> repeat_query{0};
// 0=routed/none, 1=empty, 2=incomplete, 3=various, 5=alternate mode, 7=empty route.
inline std::atomic<unsigned> early_exit{0};
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
// Non-secret phase codes for failures after an API response:
// 1=primary fetch, 2=primary parse, 3=fallback fetch,
// 4=fallback parse, 5=preview, 6=post-preview, 7=complete.
inline std::atomic<unsigned> worker_phase{0};
inline std::atomic<unsigned> fallback_attempts{0};
inline std::atomic<unsigned> fallback_fetch_errors{0};
inline std::atomic<unsigned> preview_attempts{0};
inline std::atomic<unsigned> preview_successes{0};
inline std::atomic<unsigned> http_server_retries{0};
inline void reset() noexcept {
    button.store(0);
    input_nonempty.store(0);
    input_nonempty_calls.store(0);
    input_empty_calls.store(0);
    input_member_valid.store(0);
    input_hwnd_match.store(0);
    input_member_length.store(0);
    input_live_length.store(0);
    parsed_fields.store(0);
    query_minimal.store(0);
    query_detected.store(0);
    alt_mode.store(0);
    repeat_query.store(0);
    early_exit.store(0);
    routed.store(0);
    worker_started.store(0);
    worker_advanced.store(0);
    worker_success.store(0);
    worker_error.store(0);
    worker_abort.store(0);
    response_pages.store(0);
    response_items.store(0);
    parsed_masters.store(0);
    parsed_releases.store(0);
    query_limit.store(0);
    worker_phase.store(0);
    fallback_attempts.store(0);
    fallback_fetch_errors.store(0);
    preview_attempts.store(0);
    preview_successes.store(0);
    http_server_retries.store(0);
}
}
#endif
