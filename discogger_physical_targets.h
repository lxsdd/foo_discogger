#pragma once
#include "discogger_write_safety.h"

// SDK adapter for the portable target-batch validator. Do not canonicalize,
// resolve symlinks or access any real media files during safety preflight.
inline discogger_write_safety::target_batch_decision discogger_inspect_file_targets(
    const metadb_handle_list& handles, bool physical_write) {
    return discogger_write_safety::inspect_target_batch(
        handles.get_count(), physical_write,
        [&](size_t i) { return handles[i].get_ptr() != nullptr; },
        [&](size_t i) { return handles[i]->get_path(); },
        [&](size_t i) { return handles[i]->get_subsong_index(); },
        [](const std::string& a, const std::string& b) {
            return pfc::stringCompareCaseInsensitive(a.c_str(), b.c_str());
        });
}

inline void discogger_require_physical_targets(const metadb_handle_list& handles) {
    using discogger_write_safety::target_batch_decision;
    switch (discogger_inspect_file_targets(handles, true)) {
    case target_batch_decision::ready: return;
    case target_batch_decision::reject_empty_selection:
        throw foo_discogs_exception("Physical write stopped: no files selected.");
    case target_batch_decision::reject_null_handle:
        throw foo_discogs_exception("Physical write stopped: selection contains an invalid file handle.");
    case target_batch_decision::reject_empty_path:
        throw foo_discogs_exception("Physical write stopped: selection contains an empty file path.");
    case target_batch_decision::reject_virtual_subsong:
        throw foo_discogs_exception("Physical write stopped: virtual CUE/subsong tracks cannot be written as physical files.");
    case target_batch_decision::reject_duplicate_path:
        throw foo_discogs_exception("Physical write stopped: multiple selected entries address the same file.");
    }
    throw foo_discogs_exception("Physical write stopped: unknown file target validation error.");
}
