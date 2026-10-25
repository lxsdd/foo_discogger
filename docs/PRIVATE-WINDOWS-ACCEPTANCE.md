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
