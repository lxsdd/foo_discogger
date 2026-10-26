# Optional Discogs fallback failures: complete-result recovery

Evidence: protected native Windows GUI QA on 26 Oct 2026, run 40333703915,
infrastructure SHA 3c641fb97f448c3861ccf538f0f0212db3309b9b,
product source SHA be98ccc13d2ffd8a2c4206017e5bebfdc5167a01.

Eleven Win32 native searches passed, but the fixed public
sun-moon-ampersands case returned worker error=1, abort=0, pages=3,
items=60, phase=3, fallback_attempts=3, preview_attempts=0,
server_retries=0. The optional third discovery fetch failed before the
preview phase. The failing request's exact exception family is not
known; this change is a *recovery policy*, not proof that Discogs is
always available. The independent API cohort was 14 nonempty queries
but 13 first-page expected IDs, reflecting provider-index variability.

## Recovery contract

- The primary structured query is mandatory. Any failed or changing
  primary response remains a failed search.
- Up to three optional fallback requests can contribute review-only
  suggestions; each request uses the existing atomic full-page fetch.
- A failed optional fetch cannot publish any of its unfinished pages.
  Continue to remaining bounded fallback plans without retry loops.
- Only provider transport/pagination errors and selected transient HTTP
  500/502/503/504 may be skipped. Cancellation, HTTP auth/client errors,
  missing data and exhausted rate-limit 429 remain hard errors.
- If at least one complete primary/earlier fallback candidate exists,
  preserve it and force low-confidence/manual-review presentation as
  appropriate; never auto-approve tags or modify media. If all optional
  fetches failed and no completed candidates exist, report an explicit
  failure instead of falsely claiming no results.
- A runner-only numeric FALLBACK_FETCH_ERRORS counter records skips.
  No OAuth values, query text, URLs, bodies or user media enter logs.
- Standalone C++17 positive/negative fallback-policy tests and Python
  source contracts cover the recovery decision, remaining request cap,
  status whitelist, and abort/primary boundary. Full original foobar
  Win32/x64 native host/GUI and no-publish CI remain required.

This branch is a Draft, not a release or protected OAuth signoff. A
separate explicitly approved infrastructure pin and protected run
must prove actual authenticated Win32 *and* x64 behavior at the exact
candidate SHA before it can be called a live fix.
