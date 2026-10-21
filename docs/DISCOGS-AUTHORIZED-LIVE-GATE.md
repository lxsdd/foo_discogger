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
