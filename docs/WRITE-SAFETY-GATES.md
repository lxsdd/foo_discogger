# Discogger write-safety gates (verified 2026-10-25)

This is a **Draft** development checkpoint, not permission to write to a personal
music library, merge, release, or redistribute a binary. The exact tested product
branch head is `076dba0cb4fedc9a798917839ffddf7ff5effcd9`; the full
[Windows CI run](https://github.com/lxsdd/foo_discogger/actions/runs/40138870404)
completed successfully. The runner built the native Discogger DLL against the
pinned official foobar2000 SDK (2026-10-01), loaded it in isolated foobar2000
2.26.1 Win32 and x64 hosts, and used only disposable synthetic media/profiles.

## Proven on synthetic media in both native hosts

- Physical MP3 and FLAC tag writes, complete SDK readback, exact no-op behavior,
  and preservation of existing custom fields/ReplayGain and synthetic embedded
  FLAC CUE data.
- Rejection of virtual/duplicate targets, mixed-batch preflight, stale disk
  state, and unapproved multivalue removals. An injected partial-write case
  verifies that the second file is not incorrectly treated as committed.
- Sidecar artwork: exact replacement/readback, byte- and timestamp-preserving
  no-op, and refusal of empty data.
- Embedded FLAC artwork: identical front-cover no-op; added back cover with
  front/back readback; preserved physical metadata and ReplayGain; repeated
  back-cover no-op; empty-data refusal.
- GUI startup/layout/persistence and read-only MP3, FLAC embedded-CUE,
  multifile-CUE, and external-CUE context checks.

These are scoped tests, not a blanket claim that every format, host version,
failure mode, or user workflow is safe. The runner-only QA commands are enabled
only in the disposable CI build, not in normal component builds.

## Still blocking release or user-library write acceptance

- A tag write and related artwork operations are **not one transaction**.
  Readback/fail-closed checks do not roll back a file already changed by a true
  I/O failure, cancellation, process crash, or later artwork failure. A safe
  continuation/rollback design and native fault-injection evidence remain
  necessary before advertising atomic writes.
- Virtual CUE/subsong writes are deliberately refused; there is no validated
  embedded-CUE editor in Discogger. Preserving CUE data through tested physical
  FLAC writes is not equivalent to editing CUE metadata.
- The protected authenticated Discogs QA run
  [40131171370](https://github.com/lxsdd/dev-infrastructure-public/actions/runs/40131171370)
  passed on a **separate instrumented QA branch**. It does not establish that
  this uninstrumented product SHA ran the protected OAuth search harness or
  that all end-user search/cache interactions are accepted.
- No test in this document touched the user's personal files or installed the
  current Draft component on the user's PC. Native runner evidence and final
  on-device acceptance are distinct gates.
- Upstream rights for redistribution remain unproven
  ([issue #7](https://github.com/lxsdd/foo_discogger/issues/7)).
  Do not merge, tag, publish a component package/DLL, or claim release readiness
  from green CI alone.
