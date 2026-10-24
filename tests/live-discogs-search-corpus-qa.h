#pragma once
// Runner-only, public Discogs references independently located on discogs.com.
// This data is only consumed by QA binaries and portable tests. It contains no
// user collection data, credentials, writable paths, or automatic tag approval.
#include <array>
namespace discogger_live_corpus_qa {
struct search_case {
    long long expected_release_id;
    const char* artist;
    const char* release_title;
    const char* track_title;
    const char* encoded_search_params;
};
inline constexpr std::array<search_case, 11> cases{{
    {2844, "Daft Punk", "One More Time", "One More Time (Club Mix)", "artist=Daft%20Punk&title=One%20More%20Time&per_page=100&page=1"},
    {2783977, "Above & Beyond feat. Richard Bedford", "Sun & Moon", "Sun & Moon (Club Mix)", "q=%22Sun%20%26%20Moon%22&per_page=100&page=1"},
    {7809, "The Chemical Brothers", "Hey Boy Hey Girl", "Hey Boy Hey Girl", "artist=The%20Chemical%20Brothers&title=Hey%20Boy%20Hey%20Girl&per_page=100&page=1"},
    {32559, "Björk", "Jóga", "Jóga (Howie B Main Mix)", "artist=Bj%C3%B6rk&title=J%C3%B3ga&per_page=100&page=1"},
    {3666, "Aphex Twin", "Windowlicker", "Windowlicker", "artist=Aphex%20Twin&title=Windowlicker&per_page=100&page=1"},
    {294906, "Röyksopp", "Eple", "Eple (Original Edit)", "artist=R%C3%B6yksopp&title=Eple&per_page=100&page=1"},
    {13584056, "Underworld", "Born Slippy .NUXX", "Born Slippy .NUXX", "q=%22Born%20Slippy%22&per_page=100&page=1"},
    {18370, "Chicane Featuring Maire Brennan", "Saltwater", "Saltwater (Original Mix)", "q=%22Saltwater%22&per_page=100&page=1"},
    {82825, "Faithless", "Insomnia", "Insomnia (Monster Mix)", "artist=Faithless&title=Insomnia&per_page=100&page=1"},
    {31623, "Moby", "Porcelain", "Porcelain", "artist=Moby&title=Porcelain&per_page=100&page=1"},
    {18278686, "Bicep", "Glue EP", "Glue", "artist=Bicep&title=Glue%20EP&per_page=100&page=1"},
}};
} // namespace discogger_live_corpus_qa
