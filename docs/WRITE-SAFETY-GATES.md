# Write safety: virtual CUE/subsong preflight (2026-10-18)

The current work branch rejects any tagging batch containing a nonzero subsong index
before tag mutation, and repeats the check immediately before scheduling the
asynchronous physical writer. A mixed physical/virtual batch fails closed rather
than writing only its physical members. No selected media are changed by these checks.

The completion callback now reports an asynchronous tag-write failure instead of
silently ignoring it. This is **not** a transaction/rollback implementation:
the existing workflow can start artwork independently of the eventual tag-write
completion. It may therefore partially succeed. A full continuation/rollback
design and host evidence are still required before release.

Open gates: no-op detection, comparison against complete physical tags, preservation
of custom fields/ReplayGain/embedded and external cues, explicit removal preview,
atomicity/rollback of tags and artwork, real SDK compile and isolated foobar host tests.
This guard deliberately refuses virtual subsongs; it does not provide a validated
embedded-CUE editor. Do not advertise full CUE support on this basis.

Evidence: `tests/write_safety_test.cpp` exercises physical, virtual, mixed, empty
and early-reject cases in portable C++17 and Windows MSVC x86/x64. Source contracts
check both call sites and the callback. These are **not** full component/host tests.
