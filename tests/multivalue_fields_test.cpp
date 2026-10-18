#include "../discogger_multivalue.h"
#include <cassert>
#include <string_view>

int main() {
    using discogger_multivalue::contains;
    using discogger_multivalue::same_field;
    constexpr std::string_view defaults =
        "ARTIST;ALBUM ARTIST;PRODUCER;COMPOSER;PERFORMER;GENRE;STYLE";
    assert(contains(defaults, "ARTIST"));
    assert(contains(defaults, "album artist"));
    assert(contains(defaults, "Genre"));
    assert(contains(defaults, "STYLE"));
    assert(!contains(defaults, "ART"));
    assert(!contains(defaults, "ARTISTS"));
    assert(!contains(defaults, "ALBUM"));
    assert(!contains(defaults, "REPLAYGAIN_TRACK_GAIN"));
    assert(!contains(defaults, ""));
    assert(!contains("", "ARTIST"));
    assert(contains(" ; ; ARTIST ; ;", "ARTIST"));
    assert(!contains(" ; ; ", "ARTIST"));
    assert(contains(" ; custom tag ;GENRE;", " CUSTOM TAG "));
    assert(!contains(" ; custom tag ;GENRE;", "custom"));
    assert(same_field(" ARTIST\t", "artist"));
    assert(!same_field("ARTIST", "ARTIST SORT"));
    assert(contains("A;AB;ABC;", "abc"));
    assert(!contains("A;AB;ABC;", "abcd"));
    return 0;
}
