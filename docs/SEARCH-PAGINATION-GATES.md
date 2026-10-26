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

## Staged master-version page transaction (staging candidate, not product acceptance)

The legacy `MasterRelease::load_releases` path parsed each page directly into
the visible master and could mutate cached child-release previews before a
later page failed. The staging candidate validates **all child IDs and page
arrays** and parses into a deep-copied master snapshot with no canonical
release-cache access; it commits the entire child list only after the final
abort check. Partial advanced-search versions are matched by ID. A failed
page-2 parse, invalid ID, missing array, or cancellation must preserve the
previous visible master and original child pointers.

`tests/master_page_transaction_test.cpp` covers positive completion,
page-2 validation failure, between-page/final abort, and empty input on
portable C++17 and MSVC x86/x64. The QA-only Jansson fixture
`runner_qa_native_master_page_transaction` invokes the same production
stage function in original Win32/x64 foobar hosts with real JSON objects,
positive two-page commit and negative invalid-ID/abort cases.

This work is intentionally staged **outside** Draft PR #10 while its
`f3ac723a...` product SHA is pinned by owner-approved infrastructure
PR #48. Staging CI is not protected OAuth qualification. No clean private
owner-machine installation, post-I/O physical rollback, or redistribution
permission is established by this change.
