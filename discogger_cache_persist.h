#pragma once
#include <new>
#include <utility>

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
} // namespace discogger_cache
