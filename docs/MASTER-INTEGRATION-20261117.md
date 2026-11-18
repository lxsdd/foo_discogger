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
