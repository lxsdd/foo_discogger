# Master comparison and selective integration

Baseline `40040d5358020bda24efa68c0951d361dd6b7640`; compared master through
`65d99aa` (PR24). Seven divergent commits are not seven missing features.

- `3201b24`: master-page transaction implementation and original native regression
  are already present; retain newer strict IDs/cache isolation.
- `16c71bb`, `10f8830`: optional fetch recovery already exists with newer bounded
  HTTP/cancellation/provenance logic. Missing all-failed/no-candidate distinction
  is ported and extended to discarded parse/preview results. Original exact and
  review-only buckets stay separate; existing results survive a failed search.
- `c5a5ec3`, `ef845ed`, `46de6aa`: historical workflow fixes do not add product
  behavior; current PR checks and immutable dependency/publication holds remain.
- `65d99aa`: integration merge ancestry, not an additional source feature.

No donor branch merge or writer activation. Portable regression and source
contracts cover empty success versus failed empty, surviving complete candidates
and publication ordering. New native x86/x64 CI on the resulting SHA is required;
old green CI is not qualification of this change.

## ArtistList selection completion (18 November 2026)

The original `ArtistList` callback previously accepted a completed artist with no selected row and a cleared pending loading ID, allowing a late result to reselect a discarded artist. The explicit row-selection path now requires completed, selected and pending IDs to agree in both dialog and list callbacks. The `Undef` initialization and `ArtistSearchAT` preview paths remain untouched. A portable positive/negative selection-completion regression is wired into portable-core; native x86/x64 and official SDK qualification must run on the combined commit.

## Track mapping row alignment (18 November 2026)

Issue #6: the original track-mapping dialog retained the prior file index when the Discogs track list outlasted the file list. The original production loop now resets the file index for each row and marks rows without a valid file mapping disabled. A portable positive/negative row regression covers shortened, reordered, invalid and overflow indices. No media/tag writes are performed by this regression. The combined source still requires exact-SHA native CI; live OAuth/GUI and license acceptance remain separate.

## Artwork preview completion counter (18 November 2026)

Issue #6 original track-matching dialog decremented the outstanding artwork-preview
counter without bounds checking and read it again after releasing the mutex. A
late or excess completion could wrap the unsigned count and leave the dialog
hidden; the unlocked read also raced with count updates. The production callback
now uses a bounded transition-to-zero helper under the existing mutex, and
only that transition reveals and invalidates the dialog. The pending-job increment
uses the same mutex. Portable regression covers partial completion, exact finish,
zero callbacks, excess callbacks, and duplicate callbacks after completion.
The existing eager artwork scheduling and actual owner OAuth/GUI acceptance remain
open; this does not claim lazy fetching or complete duplicate-job attribution.


## Artwork preview dispatch mutex boundary (18 November 2026)

The original dialog held the nonrecursive pending-preview mutex while calling
add_pending_previews() in both download-refresh and extra-preview paths. Since
that helper locks the same mutex, either path could deadlock before dispatch.
Initial preview dispatch also held the completion mutex while starting tasks,
which is unsafe if start() completes synchronously. The original dialog now
reserves pending completions before dispatch and never holds that mutex across
request_file_preview()/request_preview(). The counter's own increment and
completion remain mutex-protected. Source-contract regression checks all three
original dispatch paths and ordering; existing native/portable completion
regressions remain in place. Live GUI/OAuth is still separate.

## Artwork refresh duplicate-row completion ownership (18 November 2026)

The original refresh path dispatched one file-art preview per distinct index
through a bitmask, but incremented pending completions for every matching
Discogs row. Duplicate rows therefore over-reserved completions and could
leave the dialog hidden indefinitely. The production loop now reserves only
on the first occurrence of each file-art index. Source regression verifies
that the guard surrounds both bitmask update and counter increment, with
positive/negative duplicate, empty and unique-row examples. Native exact-SHA
CI remains the qualification gate; live OAuth/GUI acceptance is separate.

## ArtistList previous-row redraw alignment (19 November 2026)

The original selection-refresh path found the previous artist by its stable role
but converted the visible zero-based ListView index to `w + 1`. This redrew
the following row, and the last row could request an out-of-range redraw.
The production path now retains the zero-based index, uses an invalid sentinel
when the role disappeared, and bounds-checks the previous row before native
redraw. Initial selection, explicit refresh and completion ownership are
unchanged. The existing selection-completion native regression now also covers
first/middle/last, removed and empty previous rows and is explicitly wired
into the x86/x64 MSVC unit-test runner. This is a source/native regression,
not a substitute for owner interactive OAuth/GUI acceptance.

## Artist role lookup fallback (20 November 2026)

When the current preview pointer existed but belonged to another artist,
`get_artist_role_pos` skipped searching visible rows because the fallback was
an `else if`. The original list now searches visible artists after a
nonmatching current preview, preserving the current fast path and invalid-ID
rejection. The existing native artist-selection regression covers identity
matches/mismatches and source contracts verify the actual original-dialog
fallback wiring. This does not replace interactive owner GUI/OAuth acceptance.


## Track-mapping empty-selection caption invalidation (20 November 2026)

The original presenter now clears a previously displayed release caption when a
refresh has no selected physical files or no Discogs tracks. This complements
the already qualified first-file dereference guard: an empty selection must not
show metadata from the preceding selection, while repopulation with physical
files continues through the existing first-track release-label path. Positive
and negative source-contract cases cover empty files, empty tracks and resumed
nonempty selections. This is a production UI change, not a completed interactive
original-host empty-selection test; exact-SHA native CI and owner GUI/OAuth
acceptance remain distinct. No media or metadata writes are involved.

## Track-mapping populated-to-empty refresh gate (20 November 2026)

The existing empty-selection label guard was unreachable when both original
track presenters remained marked populated: `populate_track_ui_mode` returned
before inspecting the current selection. The production early-return now
requires nonempty files and Discogs tracks plus matching cached file-row count.
Thus the original Show/populate path rebuilds rows and clears the label when
files disappear, and rebuilds again after selection resumes. Unchanged
nonempty selections retain their prior row ordering. Source-contract cases
exercise populated, emptied and restored selection predicates. This does not
claim an executed interactive original-host selection-transition regression;
that remains separate from exact-SHA native CI and live OAuth/GUI acceptance.
