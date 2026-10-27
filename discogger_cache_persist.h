#pragma once
#include <new>
#include <string>
#include <string_view>
#include <utility>
#include "discogger_release_id.h"

// Validated online metadata must not be lost due to optional disk-cache I/O.
// Cancellation and allocation failures remain hard failures. Callers publish
// their staged in-memory result only after the final abort check.
namespace discogger_cache {
template <typename AbortException, typename Persist, typename CheckAbort>
bool persist_optional_cache(Persist&& persist, CheckAbort&& check_abort) {
    check_abort();
    try {
        const bool saved = std::forward<Persist>(persist)();
        check_abort();
        return saved;
    } catch (const AbortException&) {
        throw;
    } catch (const std::bad_alloc&) {
        throw;
    } catch (...) {
        check_abort();
        return false;
    }
}
// Cache paths must be keyed to a validated numeric artist identity. A
// malformed/missing preferred ID may be replaced by the first parsed artist;
// an empty release artist list must never be indexed unconditionally.
inline std::string release_cache_artist_id(
    std::string_view preferred, std::string_view first_release_artist) {
    if (discogger_search::valid_release_id(preferred))
        return std::string(preferred);
    if (discogger_search::valid_release_id(first_release_artist))
        return std::string(first_release_artist);
    return {};
}
} // namespace discogger_cache
