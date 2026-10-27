# Discogs online search pagination integrity

The online Discogs API may return valid-looking pagination metadata without
providing the corresponding number of items in its actual JSON array.
An incomplete response can masquerade as an empty/missing release and can
also poison cached artist-release and master-version lists.

The draft Discogger branch checks the actual `results`, `releases` or
`versions` JSON array in **both** online page-fetching overloads. The
helper `discogger_cache::valid_online_payload_count` verifies the exact item
count for each page, including the last partial page and both valid forms
of zero-result pagination (`pages=0` and `pages=1`).

Malformed or incomplete results throw a descriptive exception before
publishing the staged page list. They are not interpreted as successful
no-result matches; the caller can retry after a temporary server failure.

## Regression qualification

- `tests/cache_pages_test.cpp` includes missing/short/excess pages, final
  partial pages and empty-result cases.
- `tests/source_contracts.py` verifies checks in **both** actual online
  fetching overloads and guards against accidental removal.
- Linux C++17 and MSVC Win32/x64 unit tests are part of the ordinary Draft
  PR workflow, followed by the existing full DLL/real host/GUI hard gate.

## Conservative original-result confidence

The original search may contain a complete but unrelated title (for example,
"Only You & Me" while requesting "Only You") or a different named remix.
Such results must **not** suppress the bounded review-only fallback. The
production search helper now compares the full normalized title, or the full
title following the *first* Discogs artist/title separator, and rejects
conflicting explicit version suffixes. An unsuffixed master remains a
possible review-only hit; no heuristic can authorize a tag write or confirm
a release identity. Positive/negative native and portable regression cases
cover artist credits, Unicode punctuation, remixes and master children.

## Limits

These tests do not prove that all four user-reported missing Discogs releases
are found through authenticated live queries. They also do not prove safe
metadata/artwork/replaygain/CUE writes to disposable MP3 or FLAC fixtures.
Until independent tests and upstream rights review are complete, no merged
or distributed binary may be treated as approved.

## SHA-specific native regression evidence

The first CI run containing the page-length tests, `39617180905` on
`8cce334391121e894b735adb9e8ef5db133838fd`, FAILED in the MSVC
unit matrix because the predicate was referenced in the test/callers but
its actual header definition was absent. Commit
`c70ae00ac3b62ca9a0c60961b0d098369ed0a115` adds that definition
and a source contract requiring it. This historical failed run does not
qualify the fix; use only a subsequent CI run at the exact current SHA
for PASS claims.

## Release-title index fallback (review-only)

After an original structured search with no plausible release-title hit,
Discogger now first queries the dedicated Discogs `artist` +
`release_title` + `type=release` index, including for single artists.
Featuring/x/×/vs collaborations use the first credited participant as a
search hint; ampersand-only duos retain the full credit. The optional
partner/title and broad-title phases remain separate, with at most three
requests total. Track-only discovery never uses the release-title filter.
UTF-8 values are encoded as independent fields. No result is auto-approved;
all identities and editions remain REVIEW_REQUIRED. Five known release-ID
plan regressions do not constitute authenticated native live acceptance.
