# Discogger tag detail preview: original-value recovery and row safety

This note records the non-destructive native-preview regression gates on the
draft development branch. It is **not** authorization to merge, ship DLLs, or
write to any personal media collection.

## Correctness regression

The prior `Copy all to lead-in` handler treated
`pfc::array_t<string_encoded_array>*` as though it were a pointer to the
individual stored values and indexed it by track number. That indexed entire
array *objects*, not elements, and could access memory outside the container.

`discogger_preview::restore_original_values()` now validates the shape and
stages a deep copy of the original values before replacing the current
preview. The UI handler never treats a container pointer as a list of
containers.

A generated detail result may have either **one shared value** for all
active tracks or **one value per displayed mapped track**. Empty original
arrays, zero displayed tracks, and other array sizes are rejected without
changing the pending preview.

## Adjacent edit/read regression

`discogger_preview::safe_value_index()` also gates rendering, difference
display, and direct cell editing. It rejects missing arrays, stale row
indices, or mismatched current/visible-track counts. Editing one track from a
compressed shared-value representation first stages a separate per-track
copy; no index is read before shape validation.

## Automated qualification

- `tests/preview_restore_test.cpp`: shared value, per-track originals,
  multi-value text retention, out-of-range rows, empty and mismatched
  arrays, and non-mutation on rejected restoration
- `tests/source_contracts.py`: native GUI call sites use the guarded helpers
  and no longer contain the pointer-to-container indexing expression
- `scripts/run-msvc-unit-tests.ps1`: Windows MSVC x86 and x64
- `.github/workflows/validation.yml`: Linux C++17 regression and the
  existing exact-SHA Win32/x64 native DLL/host/GUI hard gates

A successful native host boot or read-only dialog is **not** evidence of
physical MP3/FLAC metadata, embedded/external CUE, artwork or ReplayGain
write-and-rollback safety. Those acceptance gates remain open and must be
exercised on wholly disposable fixtures, with no user media.

Upstream redistribution rights and user authorization for any DLL release
remain separate blocked release gates. No merge, tag, release or binary
artifact is implied by these tests.

## Enabled-mapping row alignment and failure isolation (draft safety gate)

Tag generation records one approval row per **enabled** mapping. The full release
mapping list can also include disabled rows. Previously the write loop used the
unfiltered mapping index when dereferencing the compact approval array; this
could target another file's approval decision or access past the array.

The write path now validates nonempty, complete tag-preview row shapes before
any working metadata mutation, refuses duplicate or out-of-range target files,
and advances a separate approval-row index only when a mapping is enabled.
Shared original/current preview values (one item) remain valid alongside
per-enabled-track arrays. Unmapped, truncated, stale, or missing entries fail
closed; HTTP 404 during generation no longer leaves truncated rows behind.

Verification: `tests/result_alignment_test.cpp`, `tests/source_contracts.py`,
and the Linux/MSVC Win32/x64 CI matrix. Tests here do **not** certify actual
MP3/FLAC metadata writes, rollback, embedded CUE editing, or DLL distribution.
Release gates #5, #7 and #8 remain open until separately demonstrated.

### Statistics UI row safety

The tag statistics renderer also refuses missing or malformed generated
result/approval arrays before accessing a row, and preserves statistics vector
alignment. This prevents stale preview dialogs from treating absent tag data
as an in-bounds row. The physical write path remains independently guarded.
