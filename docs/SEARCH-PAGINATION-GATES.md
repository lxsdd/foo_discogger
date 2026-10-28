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

## Version-specific artist-scoped discovery

When an exact edition, named remix or remaster has no plausible original hit,
a solo artist now receives a second targeted `artist + release_title +
type=release` query using the recognized base title, before the broad
`q` query. The original full version is queried first and never discarded.
For collaborations, the existing artist, partner/title and broad phases
remain intact. Track-only requests never enter the release-title index.
The three-request ceiling, UTF-8 percent encoding, manual REVIEW_REQUIRED
classification and original media/tag no-write boundaries remain unchanged.
Portable positive/negative tests and native Win32/x64 foobar host fixtures
exercise the real planner; live OAuth acceptance is still separate.

## Numeric identity aliases and atomic master pages

Discogs IDs are decimal identities, not distinct strings with optional leading
zeros. A URL such as `/release/00038012673-Title` resolves to `38012673`.
Bounded artist search deduplicates padded/canonical IDs before ranking. The
actual native advanced-search tree deduplicates release and master IDs
separately, preserving the first row and search order. Master-version staging
rejects padded/unpadded duplicates across pages before publication; its
parser canonicalizes child IDs before lookup. Invalid, zero, overflowing and
nonnumeric IDs are rejected. Source tags, CUE, media and artwork are untouched.

Portable C++17, MSVC Win32/x64, source-contract and original foobar host
synthetic GUI checks are required on the final exact SHA. None substitutes
for protected authenticated native live-search or owner acceptance.

## Advanced artist/title search output transaction

The real `DiscogsInterface::search_amt_artist` now stages both exact and
alternative artist buckets throughout original and optional release search,
bounded pagination, native preview loading and sorting. The two buckets are
published only after the final cancellation check. Malformed pages, exhausted
HTTP 429 retries, preview errors or user abort cannot expose partially
assembled output arrays to the caller. A completed zero-result search replaces
old rows with empty buckets. This protects the caller-facing result transaction;
it does not authorize tag writes or claim rollback of unrelated external caches.

The same publication barrier is exercised by portable C++17 and MSVC x86/x64
negative/positive tests and by actual original-foobar native Win32/x64 GUI QA
using SDK `pfc::array_t<Artist_ptr>`. Protected OAuth live-search acceptance
remains separate from these synthetic source/host checks.

## Native artist preview identity and typed child union

The owner-data artist list now treats padded and canonical decimal artist IDs
as one search-only preview identity. It retains the first row, search role and
existing cached pointers while merging genuinely new release and master child
IDs from later original/fallback phases. Release and master namespaces remain
distinct; padded child aliases do not duplicate rows. Invalid artist IDs are
rejected before exact-only visibility or selection. The existing synthetic
aggregate ID "0" is preserved solely for the multi-release preview, never as
a confirmed Discogs artist/cache identity.

Portable C++17, MSVC Win32/x64 and original foobar native GUI tests cover
positive/negative IDs, padded aliases, duplicate/invalid phases, stable roles,
partial catalog ownership and repeat no-ops. This remains review-only and
does not prove authenticated live search or authorize media/tag/CUE writes.

Repeated append callbacks now reuse the previous search-only Artist projection
pointer when the cumulative provider result adds no new typed child identity.
A newly discovered release or master still creates a fresh isolated union.
This avoids invalidating native owner-data selection and roles on duplicate
pagination/fallback callbacks while leaving canonical Artist caches untouched.

### Native advanced-search stale-tree barrier (2026-10-28)
A successful new advanced query with no matching release/master versions clears
its previous selectable release tree, even when the provider returns a valid
artist identity. Empty incremental callbacks and explicit artist selections
retain their current tree. The pure C++17 transition table and the original
Win32/x64 foobar GUI host fixture exercise clearing and restoring five known
synthetic release IDs. This is not authenticated Discogs live-search acceptance
or permission to write user media.


## Native artist identity and tracer preflight (2026-10-28)
The actual Win32/x64 artist-list callbacks, selection, profile completion,
tracer icon and context-menu refresh/navigation use a bounded positive
Discogs numeric ID parser instead of `atoi`. Padded IDs retain canonical
numeric identity; malformed, partial, negative and overflow IDs cannot
alias artist 0 or an unrelated selection. The synthetic aggregate search
row `0` may still display its combined preview but is never installed
as a physical artist tracer or opened as a Discogs artist URL.
Portable C++17/MSVC and original foobar2000 native GUI regression gates
cover positive and negative identities. No tags/media/DB/artwork writes.

## Exact official Discogs URL and API-reference lookup

The native Find Release artist/release fields accept the existing bracketed IDs,
canonical website links and exact official `https://api.discogs.com/releases/ID`
or `/artists/ID` references. Scheme, host and resource-kind case variants and
protocol-relative website share links are parsed as lookup hints; the ID is
validated as a positive signed-32-bit Discogs identity and leading zeroes are
canonicalized. API URLs must use the plural resource path and may not carry
a release-title slug. Credentials, ports, lookalike domains, mismatched kinds,
overflow, zero IDs and nested paths are rejected without changing the caller's
previous ID. Query and fragment text never supply an identity. All five known
release IDs have portable and original Win32/x64 native-host positive/negative
regressions. These URL tests do **not** establish OAuth live search, metadata
writes, edition matching or owner acceptance; every suggestion stays review-only.
