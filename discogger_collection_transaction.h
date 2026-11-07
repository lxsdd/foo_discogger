#pragma once
#include <cstddef>
#include <stdexcept>

// Build complete Discogs collection snapshots privately. Caller owns publication.
namespace discogger_collection {
template <typename Stage, typename Parse, typename Check>
Stage stage_collection_pages(std::size_t count, Parse&& parse, Check&& check) {
    if (!count) throw std::invalid_argument("Empty collection release page set");
    Stage staged{};
    for (std::size_t page = 0; page < count; ++page) {
        check();
        parse(staged, page);
    }
    check();
    return staged;
}
} // namespace discogger_collection
