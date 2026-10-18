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
- SQLite: ../discogger libs/sqlite-amal-3401 (SQLite 3.40.1)
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

## SDK library compile diagnostic (2026-10-18)

The first real SDK compilation CI attempt (run 39127187269, SHA
`a3df926b858213aa895d5c6b6efb5a2ca8337357`) successfully compiled
`pfc`, `foobar2000_SDK`, and `foobar2000_component_client` for Win32,
then stopped at `libPPUI` with MSVC C1083: missing `atlapp.h`.
This is a missing **WTL header dependency**, not a Discogger compiler error.
The follow-up stages WTL 10.0.9163 from its fixed-version NuGet package,
checks its NuGet signature and records its SHA-256 for an independently
verified immutable pin. The WTL SHA pin and full Discogger DLL build remain
separate release gates; no binary is uploaded.

### SDK core library acceptance (SHA 05b890e6, run 39127345690)

The official SDK 2026-10-01 project builds passed for both Win32 and x64:
`pfc`, `foobar2000_SDK`, `foobar2000_component_client`, `libPPUI`.
WTL 10.0.9163 was signature-verified from NuGet and measured as
SHA-256 `0ae7fd90a938d97da7532eae98e824450861c1cd88c6a2a4e7ab9cbe5ec6bedb`.
The next run enforces that SHA-256 and adds the SDK helpers project required
by Discogger's solution. These are SDK-library results, **not** a Discogger DLL
or any host/write-safety acceptance.

## SQLite source mismatch identified (2026-10-18)

The checked-in `sqlite3.h` wrapper already references `sqlite-amal-3401` (SQLite 3.40.1), but all four project configurations referenced `sqlite-amal-336` (SQLite 3.36). The project now uses `sqlite-amal-3401` consistently. This is source-path alignment only: a verified SQLite 3.40.1 static library, correct symbol renaming via `sqlite3ren.h`, real DLL linking and host tests are still REQUIRED. The official SQLite 3.40.1 amalgamation ZIP SHA-256 is `49112cc7328392aa4e3e5dae0b2f6736d0153430143d21f69327788ff4efe734` (source: historical Conan Center manifest; must be checked against the downloaded official archive). No third-party source or binaries are committed.
