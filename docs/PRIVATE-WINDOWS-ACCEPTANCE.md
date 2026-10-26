# Discogger private Windows acceptance — copied-fixture audit (draft)

This is read-only owner-only acceptance preparation, not an installation,
release, or permission to write to a personal music library. Always use
the actual current Draft PR #10 source SHA, not a historical CI pin.

## Hard gates

1. Public CI DLLs are built with DISCOGGER_RUNNER_MEDIA_QA test hooks.
   Never install or redistribute those runner-only DLLs. A separately
   qualified clean non-QA build, provenance/PE verification, and an isolated
   owner-machine installation route are required.
2. Modified-binary distribution rights remain unresolved (issue #7).
   No public DLL/ZIP/Actions artifact/release/tag is authorized.
3. Use a separate portable foobar2000 2.x profile for each architecture,
   not the owner's regular components, profile, database or library.
4. Copy a small MP3/FLAC/CUE/multifile/artwork sample into a NEW DIRECT
   CHILD directory of the current Windows user's TEMP. Never write to
   original media. No symlinks or junctions.

## Read-only acceptance after a clean private candidate exists

Run in PowerShell from the exact checked-out source commit:

    $sha = (git rev-parse HEAD).Trim()
    $copy = Join-Path $env:TEMP 'discogger-owner-fixture-copy'
    $manifest = Join-Path $env:TEMP 'discogger-owner-fixture-baseline.json'
    ./scripts/private-readonly-acceptance.ps1 -Mode Baseline -FixtureRoot $copy -ManifestPath $manifest -SourceSha $sha

With the isolated host, exercise search, direct ID, selection, repeat
search, refresh, offline/429/cancel feedback, double-click, preview and
cancel on copied media. DO NOT invoke Write tags or apply artwork.
Repeat on the other architecture only after verifying its PE/profile.

    ./scripts/private-readonly-acceptance.ps1 -Mode Verify -FixtureRoot $copy -ManifestPath $manifest -SourceSha $sha

A PASS establishes identical SHA-256, size and UTC mtime of all copied
media, embedded tags, CUE and artwork, with no added/removed files.
It does NOT prove post-I/O rollback, actual write correctness or
production-library safety. Never upload the private filename manifest.

Later write-on-copies-only acceptance requires separate express approval
and fault injection. SDK preflight atomicity is not physical rollback.

## CI qualification

Windows baseline CI runs tests/private-readonly-acceptance-test.ps1 on
synthetic TEMP files, including same-length content mutation with restored
mtime, extra file, source mismatch, baseline overwrite and non-TEMP
rejection. This qualifies only the audit script, not an owner-PC test.

## Exact-SHA qualification matrix (record actual run IDs; do not inherit old PASS)

| Gate | Required evidence | Status until current run proves it |
| --- | --- | --- |
| Portable search/TTL/abort/HTTP 429, JSON rollback and positive/negative fixtures | Exact source SHA; C++ and Python jobs | PENDING per new HEAD |
| MSVC Win32/x64 units | Both architecture jobs on the same SHA | PENDING per new HEAD |
| QA-only native DLL, original foobar Win32/x64 GUI, search, synthetic media and no-op | Original host logs; GUI, MP3, FLAC, CUE, artwork, stale, batch and readback markers | PENDING per new HEAD |
| Clean **non-QA** DLLs, original foobar Win32/x64 host loads | Rebuild after QA with `-CleanNonQa`; different hashes, matching PE machines, runner-only strings absent; both host smoke tests | PENDING; first qualifying CI required |
| Protected authenticated Discogs native GUI | Explicitly approved protected infra SHA pin; individual Win32/x64 cases, API identities and 0 artifacts | BLOCKED for new source until separately approved |
| Public binary redistribution / installation package | Upstream license provenance and explicit rights (issue #7) | BLOCKED |
| Private owner-PC install in isolated foobar profiles | Non-QA binary provenance, private owner-controlled transfer, exact SHA/hash comparison, independent owner consent | NOT_TESTED |
| Owner copied-media read-only audit | Baseline/Verify with exact SHA, SHA-256, size, UTC mtime and no added/removed files | NOT_TESTED |
| Physical post-I/O rollback / write-on-copies | Explicit approval, fault injection and actual rollback readback | BLOCKED; preflight is not rollback |

A GitHub job marked SUCCESS is not enough: check each required job, original
native log and artifact list against the **same** immutable product source SHA.
An old authenticated live result never qualifies a newer product commit.
Do not collapse PASS, NOT_TESTED and BLOCKED into a single green result.

## Private non-QA delivery route — preparation, not authorization

The CI rebuild gate compiles the same source twice in one ephemeral Windows
job: first a runner-only QA DLL for disposable native tests, then a **fresh
MSBuild Rebuild without** `DISCOGGER_RUNNER_MEDIA_QA`. It checks that the
clean DLL is different from the QA DLL, rejects runner-only marker strings,
checks PE architecture and loads the clean DLL in original Win32/x64 foobar.
**Neither binary is uploaded, published or installed on the owner PC.**

A future owner-controlled private route must rebuild from a verified exact
checkout and pinned dependencies in an isolated owner-controlled Windows
environment, without the QA macro; independently compare source commit,
toolchain/dependency provenance, PE machine, DLL SHA-256 and component-load
results before any copied-fixture read-only test. Use a distinct portable
foobar profile per architecture. Never overwrite a normal foobar installation,
copy a public runner-only DLL or infer permission to redistribute from the
ability to compile. This route is **not yet operationally qualified** and is
not a request to perform owner-machine steps now.
