#include "../discogger_release_id.h"
#include <cassert>
#include <climits>
#include <string>
int main() {
    using discogger_search::valid_release_id;
    using discogger_search::canonical_release_id;
    using discogger_search::discogs_reference_kind;
    using discogger_search::parse_discogs_reference;
    for (const char* id : {
        "38012673", "37332711", "36397609", "36819730",
        "35534398", "1", "2147483647", "0000001"
    }) assert(valid_release_id(id));
    for (const char* bad : {
        "", "0", "00000", "-1", "+1", "2147483648", "99999999999999999999",
        "123abc", "1e3", "12 3", " 123", "123 ", "/release/123", "0x123"
    }) assert(!valid_release_id(bad));
    assert(!valid_release_id(std::string(4096, '9')));
    assert(canonical_release_id("00038012673") == "38012673");
    assert(canonical_release_id("0000001") == "1");
    assert(canonical_release_id("2147483647") == "2147483647");
    for (const auto bad : {"", "00000", "2147483648", "123x"})
        assert(canonical_release_id(bad).empty());

    using discogger_search::parse_positive_discogs_id;
    unsigned long numeric = 777;
    for (const char* id : {"38012673", "37332711", "36397609",
                            "36819730", "35534398", "00038012673",
                            "2147483647"}) {
        assert(parse_positive_discogs_id(id, numeric));
        assert(numeric > 0 && numeric <= static_cast<unsigned long>(INT_MAX));
    }
    assert(parse_positive_discogs_id("00038012673", numeric));
    assert(numeric == 38012673UL);
    for (const char* bad : {"", "0", "000", " 123", "-1", "123junk",
                            "2147483648", "99999999999999999999"}) {
        numeric = 777;
        assert(!parse_positive_discogs_id(bad, numeric));
        assert(numeric == 777); // never overwrite a cache-key output on failure
    }

    using discogger_search::same_positive_discogs_id;
    using discogger_search::parse_preview_cache_ids;
    using discogger_search::parse_master_link;
    assert(same_positive_discogs_id("00038012673", "38012673"));
    for (const char* bad : {"", "0", "123junk", "2147483648"})
        assert(!same_positive_discogs_id(bad, bad));
    assert(!same_positive_discogs_id("38012673", "37332711"));
    unsigned long ca = 777, ci = 888;
    for (const char* release : {"38012673", "37332711", "36397609",
                                "36819730", "35534398"}) {
        assert(parse_preview_cache_ids("00012345", release, false, ca, ci));
        assert(ca == 12345 && ci > 0);
    }
    for (const char* invalid : {"", "0", "-1", "123junk", "2147483648",
                                "999999999999999999999"}) {
        ca = 777; ci = 888;
        assert(!parse_preview_cache_ids(invalid, "38012673", false, ca, ci));
        assert(ca == 777 && ci == 888);
        assert(!parse_preview_cache_ids("12345", invalid, false, ca, ci));
        assert(ca == 777 && ci == 888);
    }
    assert(parse_preview_cache_ids("unknown", "00038012673", true, ca, ci));
    assert(ca == 0 && ci == 38012673);
    bool has_master = true;
    for (const char* standalone : {"", "0"})
        assert(parse_master_link(standalone, has_master) && !has_master);
    for (const char* linked : {"38012673", "00037332711"})
        assert(parse_master_link(linked, has_master) && has_master);
    for (const char* bad : {"000", "123junk", "-1", "2147483648"}) {
        has_master = true;
        assert(!parse_master_link(bad, has_master) && has_master);
    }
    struct good_case { const char* text; discogs_reference_kind kind; const char* id; };
    for (const auto& test : {
        good_case{"https://www.discogs.com/release/38012673-1000-Years-2026", discogs_reference_kind::release, "38012673"},
        good_case{"https://discogs.com/release/37332711-2027-Remix?ev=rb", discogs_reference_kind::release, "37332711"},
        good_case{"http://www.discogs.com/de/release/36397609-2028-Edit#images", discogs_reference_kind::release, "36397609"},
        good_case{"www.discogs.com/release/36819730-9999999999999", discogs_reference_kind::release, "36819730"},
        good_case{" /release/35534398 ", discogs_reference_kind::release, "35534398"},
        good_case{"[r=38012673]", discogs_reference_kind::release, "38012673"},
        good_case{"[r=00038012673]", discogs_reference_kind::release, "38012673"},
        good_case{"https://discogs.com/release/00038012673-Title-2026", discogs_reference_kind::release, "38012673"},
        good_case{"https://discogs.com/artist/00012345-Name", discogs_reference_kind::artist, "12345"},
        good_case{"[r37332711]", discogs_reference_kind::release, "37332711"},
        good_case{"https://www.discogs.com/artist/12345-Artist-9999999", discogs_reference_kind::artist, "12345"},
        good_case{"https://discogs.com/fr/artist/23456-Name", discogs_reference_kind::artist, "23456"},
        good_case{"[a=34567]", discogs_reference_kind::artist, "34567"},
        good_case{"[a45678]", discogs_reference_kind::artist, "45678"},
        good_case{"release/2147483647", discogs_reference_kind::release, "2147483647"},
        good_case{"HTTPS://WWW.DISCOGS.COM/RELEASE/00038012673-2026-Edit", discogs_reference_kind::release, "38012673"},
        good_case{"//discogs.com/de/release/37332711-Kyau-Albert-Maywave", discogs_reference_kind::release, "37332711"},
        good_case{"https://api.discogs.com/releases/38012673", discogs_reference_kind::release, "38012673"},
        good_case{"https://api.discogs.com/releases/37332711?source=copy", discogs_reference_kind::release, "37332711"},
        good_case{"HTTPS://API.DISCOGS.COM/RELEASES/00036397609", discogs_reference_kind::release, "36397609"},
        good_case{"https://api.discogs.com/releases/36397609", discogs_reference_kind::release, "36397609"},
        good_case{"api.discogs.com/releases/36819730#view", discogs_reference_kind::release, "36819730"},
        good_case{"https://api.discogs.com/releases/35534398/", discogs_reference_kind::release, "35534398"},
        good_case{"https://api.discogs.com/artists/00012345", discogs_reference_kind::artist, "12345"}
    }) {
        std::string parsed("unchanged");
        assert(parse_discogs_reference(test.text, test.kind, parsed));
        assert(parsed == test.id);
    }
    for (const char* bad : {
        "https://evil.example/release/38012673",
        "https://api.discogs.com.evil.example/releases/38012673",
        "https://api.discogs.com@evil.example/releases/38012673",
        "https://api.discogs.com:443/releases/38012673",
        "http://api.discogs.com/releases/38012673",
        "https://api.discogs.com/release/38012673",
        "https://api.discogs.com/releases/38012673-Injected-Title",
        "https://api.discogs.com/releases/38012673/another/123",
        "https://api.discogs.com/releases/0",
        "https://api.discogs.com/releases/2147483648",
        "//evil.example/release/38012673",
        "https://www.discogs.com.evil.example/release/38012673",
        "https://www.discogs.com@evil.example/release/38012673",
        "https://www.discogs.com:443/release/38012673",
        "https://www.discogs.com/artist/38012673",
        "https://www.discogs.com/release/2147483648-Title",
        "https://www.discogs.com/release/0",
        "https://www.discogs.com/release/38012673/another/999",
        "https://www.discogs.com/release/38012673-",
        "https://www.discogs.com/release/38012673-Title with spaces",
        "https://www.discogs.com/release/abc38012673",
        "https://www.discogs.com/release/38012673/artist/1",
        "https://www.discogs.com/releases/38012673",
        "https://www.discogs.com/release/99999999999999999999",
        "[a38012673]", "[r=0]", "[r2147483648]",
        "38012673", "not a Discogs URL"
    }) {
        std::string parsed("unchanged");
        assert(!parse_discogs_reference(bad, discogs_reference_kind::release, parsed));
        assert(parsed == "unchanged");
    }
    std::string parsed("unchanged");
    assert(!parse_discogs_reference("https://discogs.com/release/38012673",
        discogs_reference_kind::artist, parsed));
    assert(parsed == "unchanged");
    return 0;
}
