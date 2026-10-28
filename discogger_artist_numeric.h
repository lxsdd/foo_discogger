#pragma once
#include <cstddef>
#include <string_view>
#include "discogger_release_id.h"

// Legacy Win32 artist tracing accepts signed-int Discogs identities. Unlike
// atoi, this parser never aliases malformed, negative, partial or overflow
// input to artist 0. "0" is a synthetic search preview, never a tracer ID.
namespace discogger_ui {
inline std::size_t artist_id_or_invalid(std::string_view raw,
                                         bool allow_aggregate = false) noexcept {
    if (allow_aggregate && raw == "0") return 0;
    if (!discogger_search::valid_release_id(raw))
        return static_cast<std::size_t>(-1);
    std::size_t value = 0;
    for (char ch : raw)
        value = value * 10 + static_cast<unsigned>(ch - '0');
    return value;
}
inline bool is_tracer_artist_id(std::size_t id) noexcept {
    return id != 0 && id != static_cast<std::size_t>(-1);
}
} // namespace discogger_ui
