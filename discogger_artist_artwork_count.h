#pragma once
#include <cstddef>

// Unresolved artist credits have no loaded image collection. Both preview
// enumeration and mapping must treat these credits as having zero images.
namespace discogger_ui {
template <typename ArtistRef>
inline std::size_t artist_artwork_count(const ArtistRef& artist) {
    if (!artist || !artist->full_artist) return 0;
    return static_cast<std::size_t>(artist->full_artist->images.get_count());
}
} // namespace discogger_ui
