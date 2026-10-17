# Development governance for the Discogger modernization fork

This is an upstream-linked fork of `ghDaYuYu/foo_discogger`. The development branch is deliberately independent of stable `master`. These rules govern modernization work, not original upstream authorship or licensing.

## P0 / P1 / P2 work register

| Priority | GitHub issue | Release gate |
| --- | --- | --- |
| P0 | [#2 Windows x86/x64 build](../issues/2) | Both clean CI builds compile with pinned dependencies |
| P0 | [#7 Licensing and provenance](../issues/7) | No third-party rights ambiguity for distributing modified components |
| P0 | [#8 End-to-end acceptance](../issues/8) | Real foobar2000 test profile + fixture matrix PASS |
| P1 | [#3 Cache and refresh](../issues/3) | Latest known artist releases visible without manual cache-clearing sequence |
| P1 | [#4 Search fallback](../issues/4) | Correct candidate suggestions with no silent incorrect release selection |
| P1 | [#5 Physical tag and artwork write safety](../issues/5) | Zero writes on no-op, no cue/subsong damage or unexpected tag deletions |
| P2 | [#6 Search UI and matching](../issues/6) | Predictable window/selection state, efficient artwork preview, reliable matching |

## Change control
- All implementation changes on `work/*` or `feature/*` branches; use draft pull requests.
- Keep stable `master` unchanged until the change is reviewed and applicable release gates pass.
- Branch protection blocks deleting or force-pushing the default branch; it does **not** currently enforce all acceptance gates. Treat these as explicit process requirements.
- Do not enable auto-merge or publish GitHub Releases during this pre-release modernization.
- Never claim that static tests, unit tests or SDK archive verification mean that the DLL built and worked in foobar2000.
- A CI failure must remain visible with an actionable explanation; do not replace real builds with echo-only placeholder jobs.

## CI and dependency trust
- Read-only GitHub Actions token by default. No secrets in PR jobs or runtime artifacts.
- Pin downloaded SDK and third-party sources to checked, attributable origins and hashes.
- Fetch dependencies only on a CI runner; do not add proprietary, unverified, or user-local assets to Git.
- Separate portable pure-C++ unit tests from real Windows Win32 and x64 compiler tests.
- Do not upload a compiled `.fb2k-component`, `.dll` or binary release until issue #7 is resolved.
- Do not silently update installed foobar components or touch real users' music collections.

## Public privacy / issue hygiene
- No Discogs OAuth tokens, credentials, playlist exports, local usernames, personal library metadata, or unreduced logs in public Issues or pull requests.
- Prefer synthetic fixture names and copied disposable media.
- When real host evidence is necessary, remove personal paths, accounts, and secrets before sharing.
- Screenshot and fixture data must be reviewed before adding to the public fork.

## Acceptance status
The first C++ parsing, source-wiring, Windows availability and official SDK-provenance checks have passed in draft PR #1. The **full Discogger Win32/x64 build**, live network behavior, real foobar2000 test, and modified-binary distribution rights are still **PENDING/BLOCKED**.
