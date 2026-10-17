#pragma once

#include <string>
#include <string_view>

// Search-plan helper, deliberately free of Discogs API and foobar2000 SDK
// dependencies to make artist normalization independently testable.
namespace discogger_search {
inline std::string trailing_collaborator(std::string_view artist) {
    std::string lowered(artist);
    for (char& ch : lowered) {
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    }
    constexpr std::string_view joins[] = {
        " x ", " vs. ", " vs ", " feat. ", " feat ", " featuring "
    };
    std::size_t position = std::string_view::npos;
    std::size_t offset = 0;
    for (const auto join : joins) {
        const auto p = lowered.rfind(join);
        if (p != std::string::npos && (position == std::string_view::npos || p > position)) {
            position = p;
            offset = join.size();
        }
    }
    if (position == std::string_view::npos) return {};
    const auto first = artist.find_first_not_of(" \t", position + offset);
    if (first == std::string_view::npos) return {};
    const auto last = artist.find_last_not_of(" \t");
    if (last == std::string_view::npos || last < first) return {};
    return std::string(artist.substr(first, last - first + 1));
}
}
