# Discogger modernization: baseline and acceptance gates

Source: `ghDaYuYu/foo_discogger` v1.0.22.3 (`25a38b7c1ea54235ef7fefaf93bf778de01685fa`).
Maintained as a GitHub fork, no changes to master before an approved integration.

## Scope
- A: Fresh artist-release lists, zero extra clicks after refresh, robust query fallback for multiple artists and DJ versions.
- B: Predictable UI state, accurate track matching, no-op tagging/artwork writes, physical vs virtual subsong safety.
- C: Windows Win32/x64 builds with pinned SDK and dependencies; static, unit, integration and real-host acceptance tests.

## Regression IDs (must all be resolvable)
- 38012673 Estiva - Via Balearia
- 37332711 Kyau & Albert x Maywave - 1000 Years
- 36397609 Matt Fax - Only You
- 36819730 Matt Fax - Till You Let Go
- 35534398 Quivver - Consciousness EP

## Gates

1. Static source checks: `python tests/source_contracts.py`.
2. Portable C++ parsing unit tests: `g++ -std=c++17 tests/http_status_parser_test.cpp -o /tmp/http-test && /tmp/http-test`.
3. **FULL_BUILD_PENDING**: Original `foo_discogger.sln` has out-of-tree Windows dependencies:
   `../SDK`, `../../pfc`, `../helpers`, `../../libPPUI`, `../discogger libs` / `../disgogger libs`, and WTL.
   No official Windows component build may be called PASS until Win32 and x64 compile on CI with pinned dependencies.
   Use the known SDK 2026-10-01 pin in `lxsdd/foobar2000_component_template`, subject to compatibility tests.
4. **HOST_PENDING**: real foobar2000 on a separate profile with duplicate files; confirm all five releases, cache freshness,
   filter persistence, non-destructive ID3/FLAC and CUE cases, empty-diff no-op timestamps, artwork byte equality.
5. **LEGAL_PENDING**: Upstream does not declare a license in repository metadata. GitHub fork visibility does not automatically
   confer rights to distribute modified binaries. Do not publish a GitHub Release or upload a component artifact until rights are clarified.

## First changes (not yet host-qualified)
- Parse HTTP statuses before applying search-context exceptions so HTTP 429 enters the retry path.
- After refreshing an artist cache, immediately re-dispatch loading for the selected artist, instead of requiring repeated clicks.

Tests here verify limited contracts only; they are not evidence of a full plugin build or end-user acceptance.
