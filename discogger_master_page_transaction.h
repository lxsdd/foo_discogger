#pragma once

#include <cstddef>
#include <stdexcept>

// Prepare every master-version page against a private snapshot. No caller-
// visible master/child may be published until validation, parsing and the
// final cancellation check all complete. The snapshot factory must deep-copy
// any mutable child objects: copying shared_ptr containers is insufficient.
namespace discogger_master {
template <typename Stage, typename Snapshot, typename Validate,
          typename Parse, typename Check>
Stage stage_master_pages(std::size_t count, Snapshot snapshot,
                        Validate validate, Parse parse, Check check) {
    if (!count) throw std::invalid_argument("Empty master-version page set");
    Stage staged = snapshot();
    for (std::size_t page = 0; page < count; ++page) {
        check();
        validate(page);
        parse(staged, page);
    }
    check();
    return staged;
}
} // namespace discogger_master
