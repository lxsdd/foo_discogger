# Clean Windows build dependency audit (2026-10-18)

Status: BLOCKED. Portable C++ tests and SDK integrity checks do not
establish that the complete Win32/x64 Discogger component compiles.

The current solution/project expects these directories outside the
repository checkout:

- foobar2000 SDK: ../SDK, ../helpers, ../foobar2000_component_client
- pfc: ../../pfc
- libPPUI: ../../libPPUI
- WTL: ../../../WTL10_9163/Include
- zlib: ../discogger libs/zlib-1.2.11
- Jansson: ../discogger libs/jansson-2.14 - win32
- OAuth C++: ../discogger libs/oauth/liboauthcpp-master
- SQLite: ../discogger libs/sqlite-amal-336
- Shared static library: ../shared/shared-$(Platform).lib

An inconsistency remains: Debug Win32, Release Win32 and Debug x64
refer to 'disgogger libs' for SQLite, but Release x64 refers to
'discogger libs'. Neither path has been validated on a clean runner.

The project no longer runs an automatic post-build DLL copy, and
both property sheets now omit developer-specific host paths.

Next acceptance gates:
1. Establish exact dependency versions, provenance, licenses and hashes.
2. Bootstrap a pinned clean-runner dependency tree.
3. Build real Win32 and x64 DLLs using MSBuild and inspect the logs.
4. Record commit, PE architecture and SHA-256 without publishing
   modified binaries before upstream redistribution rights are settled.
5. Validate in an isolated foobar2000 profile with copied test media.

This document is an actionable BLOCKED inventory, not a successful build
or authorization to distribute the component. See issues #2, #7 and #8.
