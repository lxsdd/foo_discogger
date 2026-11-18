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
