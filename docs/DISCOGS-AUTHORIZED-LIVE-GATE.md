# Authorized Discogs live acceptance

The normal pull-request CI has no Discogs secrets. The existing workflow has a separate
manual-only job, restricted to the Discogger draft branch and workflow_dispatch.

To run: GitHub Actions > Discogger modernization validation > Run workflow.
Select branch work/portable-build-properties-20261018 and Run workflow.

It looks up existing Actions secrets only by known candidate names:
FB2K_DISCOGS_QA_TOKEN, DISCOGS_QA_TOKEN, DISCOGS_TOKEN, DISCOGS_API_TOKEN,
DISCOGS_ACCESS_TOKEN. A missing secret yields BLOCKED without printing a token.
No secret is passed to an executable, PR checkout, output artifact or logs.

Official read-only API requests check the four release IDs and bounded strict,
collaborator and title search. Exact and fallback results are distinguished.
This does not replace native Discogger host GUI, cache, 429, rollback or legal gates.

## Five known missing-release regression cases

The read-only API acceptance set now includes all five IDs required by the
project gate: Estiva — Via Balearia (38012673), Kyau & Albert x Maywave —
1000 Years (37332711), Matt Fax — Only You (36397609), Matt Fax — Till
You Let Go (36819730), and Quivver — Consciousness EP (35534398).

The test first checks each exact release ID, title and nonempty tracklist,
then attempts bounded structured search followed by a reviewed partner/title
fallback. A partner or title-only result is **not** an exact artist/version
match and must never be tagged automatically without reviewing its identity.

The previous authorized manual run on `980737760de` found the first four
(4/4) via the live API, with a partner-review fallback for 1000 Years. The
fifth test was **added later**: it must not be called live-verified before a
new authorized manual run passes `5/5` on the new exact SHA. Ordinary PR or
push CI runs skip credentialed live lookups by design. The full Windows host
QA uses disposable media and synthetic release rows rather than secrets in
the executable. Neither this test nor the Windows fixtures prove successful
user-library acceptance or all live authenticated GUI flows.
