#include "../discogger_cache_persist.h"
#include "../discogger_artist_profile_transaction.h"
#include <cassert>
#include <new>
#include <stdexcept>
#include <string>
struct profile_fixture { std::string id, name, images; };
struct canceled : std::runtime_error {
    canceled() : std::runtime_error("canceled") {}
};
int main() {
    using discogger_cache::persist_optional_cache;
    using discogger_cache::release_cache_artist_id;
    // A release with no parsed artist must still load, but cannot create an
    // ambiguous disk-cache directory. A valid supplied ID takes precedence.
    assert(release_cache_artist_id("194", "355") == "194");
    assert(release_cache_artist_id("", "355") == "355");
    assert(release_cache_artist_id("0", "37332711") == "37332711");
    assert(release_cache_artist_id("999999999999999999", "38012673") == "38012673");
    assert(release_cache_artist_id("-1", "") == "");
    assert(release_cache_artist_id("", "") == "");
    assert(release_cache_artist_id("12oops", "bad") == "");
    bool abort = false;
    int checks = 0;
    const auto check = [&] { ++checks; if (abort) throw canceled{}; };
    std::string visible = "last good release list";
    assert(persist_optional_cache<canceled>([] { return true; }, check));
    assert(checks == 2 && visible == "last good release list");
    assert(!persist_optional_cache<canceled>([] { return false; }, check));
    assert(!persist_optional_cache<canceled>([]() -> bool {
        throw std::runtime_error("disk unavailable");
    }, check));
    // A complete online result can still be committed when the disk fails.
    visible = "new complete online release list";
    assert(visible == "new complete online release list");
    visible = "last good release list";
    abort = true;
    bool entered = false;
    try {
        persist_optional_cache<canceled>([&] { entered = true; return true; }, check);
        visible = "invalid new result";
        assert(false);
    } catch (const canceled&) {}
    assert(!entered && visible == "last good release list");
    abort = false;
    try {
        persist_optional_cache<canceled>([]() -> bool { throw canceled{}; }, check);
        visible = "invalid new result";
        assert(false);
    } catch (const canceled&) {}
    assert(visible == "last good release list");
    try {
        persist_optional_cache<canceled>([&] { abort = true; return true; }, check);
        visible = "invalid new result";
        assert(false);
    } catch (const canceled&) {}
    assert(visible == "last good release list");
    abort = false;
    try {
        persist_optional_cache<canceled>([]() -> bool { throw std::bad_alloc{}; }, check);
        assert(false);
    } catch (const std::bad_alloc&) {}
    assert(visible == "last good release list");
    abort = false;
    try {
        persist_optional_cache<canceled>([&]() -> bool {
            abort = true;
            throw std::runtime_error("write error while canceled");
        }, check);
        assert(false);
    } catch (const canceled&) {}
    assert(visible == "last good release list");    // The real artist profile parser now stages profile fields before
    // publishing. Failed identity, partial JSON, abort or cache I/O must
    // never replace the last visible name and artwork.
    using discogger_cache::same_artist_profile_identity;
    using discogger_cache::stage_artist_profile;
    for (const auto id : {"38012673", "37332711", "36397609", "36819730", "35534398"})
        assert(same_artist_profile_identity(id, id));
    assert(!same_artist_profile_identity("38012673", "37332711"));
    for (const auto bad : {"", "0", "-1", "abc", "2147483648"})
        assert(!same_artist_profile_identity("38012673", bad));
    profile_fixture visible_profile{"38012673", "last good profile", "last good artwork"};
    const auto commit_profile = [&](profile_fixture& staged) {
        visible_profile.name = std::move(staged.name);
        visible_profile.images = std::move(staged.images);
    };
    const auto parse_profile = [&](profile_fixture& staged) {
        staged.name = "Unicode – artist";
        staged.images = "new artwork";
        if (!same_artist_profile_identity(visible_profile.id, staged.id))
            throw std::runtime_error("mismatched artist");
    };
    int profile_checks = 0;
    bool profile_abort = false;
    const auto profile_check = [&] {
        ++profile_checks;
        if (profile_abort) throw canceled{};
    };
    profile_fixture complete_profile{visible_profile.id, "", ""};
    stage_artist_profile(complete_profile, parse_profile, [&] {
        assert(!persist_optional_cache<canceled>([]() -> bool {
            throw std::runtime_error("optional cache unavailable");
        }, profile_check));
    }, profile_check, commit_profile);
    assert(profile_checks == 5);
    assert(visible_profile.name == "Unicode – artist" && visible_profile.images == "new artwork");
    for (int failure : {0, 1, 2, 3}) {
        visible_profile.name = "last good profile";
        visible_profile.images = "last good artwork";
        profile_fixture partial{failure == 0 ? "37332711" : visible_profile.id, "", ""};
        int local_checks = 0;
        bool rejected = false;
        try {
            stage_artist_profile(partial, [&](profile_fixture& target) {
                target.name = "partial name";
                target.images = "partial artwork";
                if (failure == 1) throw std::runtime_error("invalid JSON");
                if (!same_artist_profile_identity(visible_profile.id, target.id))
                    throw std::runtime_error("wrong ID");
            }, [&] {
                if (failure == 3) throw std::runtime_error("disk stage failure");
            }, [&] {
                if (++local_checks == 2 && failure == 2) throw canceled{};
            }, commit_profile);
        } catch (const std::exception&) { rejected = true; }
        assert(rejected);
        assert(visible_profile.name == "last good profile" &&
               visible_profile.images == "last good artwork");
    }
    profile_fixture aborted_profile{visible_profile.id, "", ""};
    profile_abort = false;
    bool aborted_before_commit = false;
    try {
        stage_artist_profile(aborted_profile, parse_profile,
            [&] { profile_abort = true; }, profile_check, commit_profile);
    } catch (const canceled&) { aborted_before_commit = true; }
    assert(aborted_before_commit && visible_profile.name == "last good profile" &&
           visible_profile.images == "last good artwork");

}
