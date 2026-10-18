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

The four SQLite include/library references now consistently use 'discogger libs'.
Release x64 also enables /GS stack buffer security like Release Win32.
These changes remove configuration drift, but the dependency paths remain
unvalidated on a clean runner until a full component build passes.

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

## Official SDK C++ language gate

The foobar2000 SDK 2026-09-16 change log explicitly requires a C++20-capable
compiler and Visual Studio 2022 or newer; SDK 2026-10-01 inherits that
requirement. The four Discogger component configurations now use MSBuild
`stdcpp20` instead of `stdcpp17`. This is a necessary configuration
correction, **not** evidence that the original source compiles under C++20.

Official reference: https://www.foobar2000.org/changelog-sdk (2026-09-16).
Portable unit tests remain C++17 to preserve their separate minimum-standard
contract; only actual SDK-consuming component builds require C++20.
