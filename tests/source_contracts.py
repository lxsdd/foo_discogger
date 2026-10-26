#!/usr/bin/env python3
"""Static wiring/regression guards. They do not replace a foobar2000 runtime test."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]

# Reject unselected web/utility commands before reading item zero or dispatching.
# This is a source guard; the actual Win32/x64 component compile and host checks
# remain independent CI gates.
mainmenu = (root / "mainmenu.cpp").read_text(encoding="utf-8-sig")
web_menu = mainmenu.split("class contextmenu_discogs_web", 1)[1].split("class contextmenu_discogs :", 1)[0]
utility_menu = mainmenu.split("class contextmenu_discogs_utilities", 1)[1]
for section in (web_menu, utility_menu):
    guard = "if (!g_discogs || p_data.get_count() == 0) {"
    assert guard in section
    assert section.index(guard) < section.index("switch (p_index)", section.index("bool context_get_display("))
    assert "context_get_display(p_index, p_data, out, displayflags, p_caller);" in section
    assert "if (displayflags & FLAG_GRAYED)" in section
dialog_header = (root / "find_release_dialog.h").read_text(encoding="utf-8-sig")
assert "if (vsplit_title.size() > 1 && tmp_artist.equals(vsplit_title[0])) {" in dialog_header
assert "if (tmp_artist.equals(vsplit_title[0])) {" not in dialog_header
print("PASS: empty web/utility selections are disabled; unsplit track titles never read a second token.")

artist = (root / "find_artist_list.cpp").read_text(encoding="utf-8-sig")
# Artist list drawing is a hot path: never leak GDI bitmaps or dereference
# a stale owner-data row from an asynchronous artist-list replacement.
artist_display = artist[artist.index("bool CArtistList::OnDisplayCellImage("):
                        artist.index("size_t CArtistList::get_artist_id(")]
assert artist_display.count("discogger_ui::valid_artist_list_row(") >= 2
assert artist_display.count("!m_find_release_artists[item]") >= 2
paint = artist[artist.index("void CArtistList::RenderCellImage("):
               artist.index("// open artist profile panel")]
assert "auto res = GetIconInfo(" not in paint
assert "ICONINFO ii;" not in paint and "GetObject(" not in paint
assert "SM_CXSMICON" in paint and "SM_CYSMICON" in paint
assert "if (!g_hIcon_quian || item >=" in paint
assert "std::numeric_limits<int>::max" in artist
print("PASS: artist list rendering bounds checks and no allocating GDI bitmap probes.")
# Modal context actions require the original selected artist identity.
artist_action = artist.split("void CArtistList::Default_Action()", 1)[1].split("// CListControlOwnerData overrides", 1)[0]
assert "if (!artist || !m_idtracer_p) return;" in artist_action
artist_context = artist.split("void CArtistList::context_menu(", 1)[1]
assert "discogger_ui::valid_artist_context_target(" in artist_context
assert artist_context.index("discogger_ui::valid_artist_context_target(") < artist_context.index("switch (cmd)")
assert "valid_artist_context_target(std::size_t menu_row" in (root / "discogger_artist_activation.h").read_text(encoding="utf-8-sig")
print("PASS: null artist rows and stale modal context selection reject actions.")

# Streaming artist worker callbacks deliver cumulative lists. Appending the
# complete alternative bucket twice duplicatively displayed old rows and
# using the already combined visible count lost new exact rows.
artist_projection = artist.split("void CArtistList::switch_find_releases(", 1)[1].split(
    "void CArtistList::set_image_list(", 1)[0]
assert "m_artist_exact_matches.get_count() - base" not in artist_projection
assert "m_find_release_artists.resize(base + cadd)" not in artist_projection
assert "m_find_release_artists = m_artist_exact_matches;" in artist_projection
assert "m_find_release_artists.append(m_artist_other_matches);" in artist_projection
assert "std::unordered_map<std::string, size_t> prior_roles;" in artist_projection
assert "prior_roles.emplace(previous_artist->id.get_ptr()," in artist_projection
assert "current_artist->search_role_list_pos =" in artist_projection
assert "previous != prior_roles.end() && previous->second" in artist_projection
assert "if (append && (op == 0 || op == 3))" in artist_projection
assert "else if (op == 0 || op == 3)" in artist_projection
native_qa = (root / "find_release_dialog.cpp").read_text(encoding="utf-8-sig")
native_stream = native_qa.split("CFindReleaseDialog::runner_qa_populated_search_rows(", 1)[1].split(
    "void CFindReleaseDialog::ShowArtistSearchResults(", 1)[0]
for required in (
    'on_search_artist_done(two_artists, initial_others, false);',
    'on_search_artist_done(late_exact, exact, true);',
    'on_search_artist_done(exact, late_other, true);',
    'on_search_artist_done(exact, pfc::array_t<Artist_ptr>(), true);',
    'appended_rows.get_count() != 5',
    'appended_rows[3]->search_role_list_pos != original_other_role',
    'on_search_artist_done(exact_late, exact, true);',
    'exact_rows.get_count() != 3',
    'exact_rows[2]->search_role_list_pos == exact_role_one',
    'exact_rows[2]->search_role_list_pos == exact_role_two',
    'CheckDlgButton(IDC_CHK_ONLY_EXACT_MATCHES, BST_UNCHECKED);',
):
    assert required in native_stream
print("PASS: cumulative exact/alternative artist append preserves roles and rejects old duplicate/lost-row algorithm.")

# A partial callback may contain an already-known Discogs ID or a null
# pointer. Native owner-data rows must remain non-null, unique by nonempty
# ID, with the first (exact) match retaining precedence.
artist_header = (root / "find_artist_list.h").read_text(encoding="utf-8-sig")
assert "Get_Exact_Artist_Count() const" in artist_header
assert "Get_Other_Artist_Count() const" in artist_header
assert "std::unordered_map<std::string, size_t> visible_positions;" in artist_projection
assert "if (!candidate) continue;" in artist_projection
assert "if (!candidate->id.get_length()) continue;" in artist_projection
assert "union_search_artist_payload(unique_artists[index], candidate)" in artist_projection
assert "m_find_release_artists = unique_artists;" in artist_projection
# The original Artist/cache objects must not be extended in place by a
# later provider page with the same ID; search-only overlays retain roles.
artist_union = artist.split("Artist_ptr union_search_artist_payload(", 1)[1].split(
    "} // namespace", 1
)[0]
for required in (
    "if (!adds_new) return retained;",
    "std::make_shared<Artist>(retained->id.get_ptr())",
    "merged->loaded_releases = false;",
    "merged->search_role_list_pos = retained->search_role_list_pos;",
    "merged->releases.append_single(release);",
    "merged->master_releases.append_single(master);",
    "emitted_releases.emplace(release->id.get_ptr()).second",
    "emitted_masters.emplace(master->id.get_ptr()).second",
):
    assert required in artist_union
assert "retained->releases.append_single" not in artist_union
assert "delta->releases.append_single" not in artist_union
for required in (
    "Native same-ID disjoint release/master union lost payload, role or cache isolation.",
    "Native repeated same-ID payload duplicated rows or releases.",
    "Native manually selected same-ID artist lost a late release or master.",
    "Native exact-only same-ID union lost visibility or release tree.",
    "Native new search did not reset previous same-ID union.",
):
    assert required in native_stream
print("PASS: same-ID disjoint release/master union is non-mutating and covered by native positive/negative GUI regressions.")
assert "get_size() > pos && m_find_release_artists[pos]" in artist
assert "m_find_release_artists[walk] &&" in artist
assert "m_find_release_artists[w] &&" in artist
for required in (
    "duplicate_exact.append_single(added_exact);",
    "duplicate_other.append_single(other_artist);",
    "null_exact.append_single(Artist_ptr());",
    "on_search_artist_done(duplicate_exact, duplicate_other, true);",
    "on_search_artist_done(null_exact, exact, true);",
    "Native duplicate/null search updates created rows or changed roles.",
    "Native late exact search results were not automatically added to the release tree.",
    "Native late alternative results were not automatically added to the release tree.",
    "Native late alternatives disabled an existing exact-only filter.",
    "Native null-only exact result incorrectly hid valid alternatives.",
    "Native null-only search retained stale visible results.",
    "on_search_artist_done(null_exact, alternatives, false);",
    "on_search_artist_done(null_exact, exact, false);",
):
    assert required in native_stream
print("PASS: null/duplicate artist worker deltas and late GUI tree/checkbox regressions are wired.")





# Do not retain dialog or artist-list objects in queued main-thread redraw work.
assert "fb2k::inMainThread([this" not in artist
assert "fb2k::inMainThread([dlg" not in artist
assert "void queue_artist_redraw(HWND window)" in artist
assert "GetDlgCtrlID(window) == IDC_ARTIST_LIST" in artist
dialog_redraw = (root / "find_release_dialog.cpp").read_text(encoding="utf-8-sig")
assert "fb2k::inMainThread([this" not in dialog_redraw
assert "::IsWindow(status_window)" in dialog_redraw
assert "IDC_STATIC_FIND_REL_SEARCH_STATS" in dialog_redraw
assert dialog_redraw.count("::IsWindow(tree_window)") >= 2
print("PASS: asynchronous search/tree/artist redraw work does not capture destroyed UI objects.")
# The modal/async artist worker must own any data sourced from the Find
# Releases dialog and reject completions from closed or replacement windows.
native_tasks_h = (root / "tasks.h").read_text(encoding="utf-8-sig")
native_tasks_cpp = (root / "tasks.cpp").read_text(encoding="utf-8-sig")
assert "std::vector<size_t> m_artist_ids;" in native_tasks_h
assert "const std::vector<size_t> & m_artist_ids;" not in native_tasks_h
assert "pfc::string8 m_artist_name;" in native_tasks_h
assert "const char* m_artist_name;" not in native_tasks_h
assert native_tasks_h.count("HWND m_owner_window = nullptr;") >= 2
assert native_tasks_cpp.count("m_owner_window = parent;") >= 2
assert native_tasks_cpp.count("g_discogs->find_release_dialog->m_hWnd != m_owner_window") >= 3
assert "if (::IsWindow(m_owner_window) && g_discogs" in native_tasks_cpp
print("PASS: artist async task owns ID/name data and rejects stale-dialog callbacks.")
# Duplicate requests must not unregister another task's in-flight artist ID.
# A successful request that returned no artist must still release its own ID.
artist_worker = native_tasks_cpp.split("bool get_artist_process_callback::check_in()", 1)[1].split(
    "void get_multi_artists_process_callback::start", 1
)[0]
artist_checkout = artist_worker.split("void get_artist_process_callback::check_out()", 1)[1].split(
    "void get_artist_process_callback::safe_run", 1
)[0]
artist_success = artist_worker.split("void get_artist_process_callback::on_success", 1)[1].split(
    "void get_artist_process_callback::on_abort", 1
)[0]
assert "bool m_artist_load_registered = false;" in native_tasks_h
assert "m_artist_load_registered = check_in();" in artist_worker
assert "if (!m_artist_load_registered) {" in artist_worker
assert "if (!m_artist_load_registered) return;" in artist_checkout
assert "m_artist_load_registered = false;" in artist_checkout
assert artist_success.index("check_out();") < artist_success.index("if (m_artist)")
print("PASS: duplicate artist loads cannot clear another worker's marker; null results release ownership.")
# The multi-artist callback must release only its own in-flight registrations,
# and a later duplicate callback must never clear a new worker's marker.
multi_worker = native_tasks_cpp.split("bool get_multi_artists_process_callback::check_in()", 1)[1].split(
    "void get_multi_artists_process_callback::safe_run", 1
)[0]
assert 'return !m_checked_in_artist_ids.empty();' in multi_worker
assert 'discogger_async::release_owned_ids(' in multi_worker
assert 'discogger_async::unique_artist_ids(artist_ids)' in native_tasks_h
assert "discogger_async::owns_id(m_checked_in_artist_ids, registry_id" in native_tasks_cpp
assert "discogger_async::should_load_owned_releases(" in native_tasks_cpp
assert "std::to_string(artist_id).c_str(), fetch_releases, p_status, p_abort" in native_tasks_cpp
multi_safe_run = native_tasks_cpp.split("void get_multi_artists_process_callback::safe_run(", 1)[1].split(
    "void get_multi_artists_process_callback::on_success(", 1
)[0]
assert "if (m_bload_releases) check_in();" in multi_safe_run
assert "if (!check_in()) {" not in multi_safe_run
assert "for (auto artist_id : m_artist_ids)" in multi_safe_run
assert "m_artists.add_item(std::move(artist));" in multi_safe_run
assert 'm_checked_in_artist_ids, g_discogs->vArtistLoadReleasesTasks' in multi_worker
assert 'for (auto artist_id : m_artist_ids)' not in multi_worker.split(
    "void get_multi_artists_process_callback::check_out()", 1)[1]
assert '#include "discogger_artist_load_ownership.h"' in native_tasks_cpp
for test_runner in (".github/workflows/validation.yml", "scripts/run-msvc-unit-tests.ps1"):
    assert "artist_load_ownership_test.cpp" in (root / test_runner).read_text(encoding="utf-8-sig")
print("PASS: multi-artist in-flight markers are owned and released once.")




dialog = (root / "find_release_dialog.cpp").read_text(encoding="utf-8-sig")
fetcher = (root / "fetcher.cpp").read_text(encoding="utf-8-sig")

assert '"&Refresh artist releases (online)"' in artist
assert "cached_artist->release_refresh.request();" in artist
assert "dlg->convey_artist_list_selection(refresh);" in artist
assert "discogs_interface->delete_artist_cache(artist->id)" not in artist
assert "artist->loaded_releases = false;" not in artist
assert "GetSubItemText(isel, 0, out);" in artist
assert "if (pos >= current_artists.get_count() || !current_artists[pos]) return;" in dialog
artist_load = (root / "discogs.cpp").read_text(encoding="utf-8-sig")
artist_load = artist_load[artist_load.index("void Discogs::Artist::load_releases("):artist_load.index("void Discogs::MasterRelease::load_releases(")]
assert "const bool explicit_refresh = release_refresh.take();" in artist_load
# The canonical explicit online refresh must not use the search-result fast path.
assert '#include "discogger_artist_refresh.h"' in dialog
assert "canonical_artist->release_refresh.pending()" in dialog
assert "discogger_cache::should_fetch_artist(" in dialog
assert dialog.index("discogger_cache::should_fetch_artist(") < dialog.index("//'auto load on artist change'")
assert "should_fetch_artist(bool needs_data" in (root / "discogger_artist_refresh.h").read_text(encoding="utf-8-sig")
# Generation-based coalescing must not silently discard a newer explicit
# artist-release refresh requested while another worker was finishing.
assert "skip_coalesced_artist_load(" in artist_load
assert "release_refresh.pending()))" in artist_load
assert "skip_coalesced_artist_load(bool loaded" in (root / "discogger_artist_refresh.h").read_text(encoding="utf-8-sig")

assert artist_load.index("const bool explicit_refresh = release_refresh.take();") < artist_load.index("    try {") < artist_load.index("bool offline_can_read = ol::can_read();")
assert "|| explicit_refresh" in artist_load
assert artist_load.count("release_refresh.retry_after_failure(explicit_refresh);") == 3
tasks_callbacks = (root / "tasks.cpp").read_text(encoding="utf-8-sig")
assert "m_artist->release_refresh.pending()" in tasks_callbacks
# Modeless worker must propagate real SDK cancellation all the way to
# threaded_process so on_done selects on_abort rather than a fatal error.
worker_run = tasks_callbacks[tasks_callbacks.index("void foo_discogs_threaded_process_callback::run("):
                             tasks_callbacks.index("void foo_discogs_threaded_process_callback::on_done(")]
assert worker_run.index("catch (const exception_aborted&) {") < worker_run.index("catch (foo_discogs_exception &e)")
assert "throw;" in worker_run[worker_run.index("catch (const exception_aborted&) {"):
                              worker_run.index("catch (foo_discogs_exception &e)")]

assert "dlg->m_alist.Get_Selected_Id().equals(m_artist->id)" in tasks_callbacks
for name in (".github/workflows/validation.yml", "scripts/run-msvc-unit-tests.ps1"):
    assert "artist_refresh_test.cpp" in (root / name).read_text(encoding="utf-8-sig")
assert "if (need_data || cupdsrc == updRelSrc::UndefFast || cupdsrc == updRelSrc::ArtistList)" in dialog
assert "discogger_net::parse_http_status_code(status.c_str())" in fetcher
assert "throw http_429_exception();" in fetcher
# Both fetch entry points must limit compressed stream growth and reject
# malicious gzip ISIZE before allocating a decoded buffer.
assert fetcher.count("discogger_net::next_http_buffer_capacity(") == 2
assert fetcher.count("discogger_net::acceptable_http_decoded_size(") == 2
simple_log = fetcher[fetcher.index("void Fetcher::fetch_html_simple_log("):fetcher.index("void Fetcher::set_oauth(")]
assert 'html = "";' in simple_log[:simple_log.index("try {")]
assert "catch (exception_aborted) {\n        buffer.set_size(0);" in simple_log
assert "catch (foo_discogs_exception&) {\n        buffer.set_size(0);" in simple_log
assert "catch (const std::exception&) {\n        buffer.set_size(0);" in simple_log
assert simple_log.index("buffer.set_size(0);") < simple_log.index("html = pfc::string8((char*)buffer.get_ptr()")
# Do not read HTTP error/partial bodies as successful artwork/preview data.
assert "usable_preview_http_response(status.c_str())" in simple_log
assert 'find_first("200")' not in simple_log
status_guard = simple_log[simple_log.index("if (!discogger_net::usable_preview_http_response("):
                         simple_log.index("pfc::string8 reply_content_type;")]
assert "buffer.set_size(0);" in status_guard
assert "break;" in status_guard
assert "usable_preview_http_response" in (root / "http_status_util.h").read_text(encoding="utf-8-sig")

assert "out.set_size(out.get_size() << 1)" not in fetcher
assert "buffer.set_size(buffer.get_size() << 1)" not in fetcher
assert '#include "http_status_util.h"' in fetcher
assert "max_http_response_bytes = 64u * 1024u * 1024u" in (root / "http_status_util.h").read_text(encoding="utf-8-sig")
# Reject invalid gzip payloads in both fetch entry points in Release builds.
# The best-effort path must discard its compressed buffer on any inflate error.
assert fetcher.count("state != Z_OK || destLen != unzipped.get_size()") == 2
assert "PFC_ASSERT(destLen == unzipped.get_size())" not in fetcher
main_zip = fetcher[fetcher.index("int state = myUncompress("):fetcher.index("void Fetcher::fetch_html_simple_log(")]
assert 'throw network_exception("Invalid or truncated gzip response.");' in main_zip
simple_zip = fetcher[fetcher.index("void Fetcher::fetch_html_simple_log("):fetcher.index("void Fetcher::set_oauth(")]
bad_inflate = simple_zip[simple_zip.index("if (state != Z_OK || destLen != unzipped.get_size())"):
                         simple_zip.index("buffer = unzipped;")]
assert "buffer.set_size(0);" in bad_inflate
assert "break;" in bad_inflate

# The image-rate-limit classification must recognize real image hosts,
# exclude small previews and reject lookalike URLs.
# A URL substring (including user-supplied resource URLs) must never
# authorize the OAuth Authorization header for a foreign origin.
assert "discogger_net::is_discogs_api_url(url.c_str())" in fetcher
assert 'url.find_first("api")' not in fetcher
assert "use_oauth &= use_api;" in fetcher
assert "if (use_oauth) {" in fetcher
assert "is_discogs_api_url(std::string_view url)" in (root / "http_status_util.h").read_text(encoding="utf-8-sig")
assert "discogger_net::should_throttle_discogs_image(url.c_str())" in fetcher
assert '!std::string(url.c_str()).find("h:150")' not in fetcher
assert "bool isImageUrl = url.has_prefix(" not in fetcher
assert "should_throttle_discogs_image(std::string_view url)" in (root / "http_status_util.h").read_text(encoding="utf-8-sig")

# Malformed API rate headers may not set zero/negative/overflowed unsigned
# counters, and image consumption must not underflow at zero.
assert "discogger_net::parse_discogs_rate_header(" in fetcher
assert "discogger_net::discogs_rate_threshold(parsed)" in fetcher
assert "discogger_net::consume_discogs_rate(m_ratelimit_remaining)" in fetcher
assert "discogger_net::discogs_rate_interval(m_ratelimit_max)" in fetcher
assert "m_ratelimit_max = atoi(rate_header_buffer)" not in fetcher
assert "m_ratelimit_remaining = atoi(rate_header_buffer)" not in fetcher
assert "--m_ratelimit_remaining" not in fetcher
assert "error_code = 4011" not in fetcher
# OAuth tokens and signed query strings must not enter debug logs, caught
# exception messages, or errors passed on to users. A leaked secret cannot
# be made safe by hiding only the first URL log.
assert 'log_msg(clean_url)' not in fetcher
assert 'urldecode2(decoded, clean_url)' not in fetcher
assert '<< clean_url' not in fetcher
assert 'e.what()' not in fetcher
assert 'log_msg(e.what())' not in fetcher
assert 'msg << html' not in fetcher
assert 'OAuth request response:' not in fetcher
assert 'OAuth generated response:' not in fetcher
assert 'OAuth request-token response received (redacted).' in fetcher
assert 'OAuth access-token response received (redacted).' in fetcher
assert fetcher.count('log_msg("Discogs HTTP request (URL and query redacted).");') == 2
network_fetch = fetcher[fetcher.index("void Fetcher::fetch_url("):fetcher.index("void Fetcher::fetch_html_simple_log(")]
assert 'catch (const std::exception&) {' in network_fetch
assert 'out.set_size(0);' in network_fetch[network_fetch.index("catch (const std::exception&) {"):]
assert 'throw network_exception("Discogs request failed; response discarded.");' in network_fetch
# An already-aborted request must not pretend success with a reused output
# array; the normal modeless cancellation path must be reached.
preflight = network_fetch[:network_fetch.index("pfc::string8 msg_average;")]
assert "out.set_size(0);" in preflight
assert preflight.index("out.set_size(0);") < preflight.index("p_abort.check();")
assert "catch (exception_aborted)" not in preflight
assert "return;" not in preflight



search = (root / "discogs_interface.cpp").read_text(encoding="utf-8-sig")
assert "inline std::string trailing_collaborator(" in (root / "discogger_search_fallback.h").read_text(encoding="utf-8-sig")
assert "discogger_search::build_fallback_plan(" in search
# A modeless search cancelled during its original page fetch must propagate
# exception_aborted instead of being mislabeled as an unknown search failure.
amt_search = search[search.index("rppair_t DiscogsInterface::search_amt_artist("):
                    search.index("pfc::array_t<JSONParser_ptr> DiscogsInterface::get_all_pages(")]
search_fetch = amt_search[:amt_search.index("const size_t cpages = pages.get_count();")]
assert "catch (const exception_aborted&) {" in search_fetch
assert search_fetch.index("catch (const exception_aborted&) {") < search_fetch.index("catch (foo_discogs_exception& e)")
assert "throw;" in search_fetch[search_fetch.index("catch (const exception_aborted&) {"):
                               search_fetch.index("catch (foo_discogs_exception& e)")]

assert "for (const auto& candidate : plans)" in search
assert "get_all_pages(fallback_url, fallback_params" in search
assert "discogger_search::encode_fallback_query(candidate)" in search
assert "search_query::TextToMap(fallback_expression, fallback_map)" not in search
assert "fallback_params(fallback_query.c_str())" in search
assert "percent_encode_search_value" in (root / "discogger_search_fallback.h").read_text(encoding="utf-8-sig")
assert "if (fallback_max > max_to_abort)" in search
assert 'presentation_query["artist="].first.clear();' in search
assert "used_broad_fallback = true;" in search
assert "p_abort.check();" in search
# A nonempty partner/guest result may be unrelated. The next bounded
# fallback must still run, while an explicit provider limit stops requests.
fallback_loop = amt_search.split("for (const auto& candidate : plans) {", 1)[1].split(
    "va_res_cap.first = ", 1)[0]
# Windows foobar includes may define min as a function-like macro. The
# original SDK DLL compile (unlike portable/MSVC isolated units) exposed it.
assert "std::min(" not in (root / "discogger_search_fallback.h").read_text(encoding="utf-8-sig")
assert "discogger_search::fallback_query_budget budget(plans.size());" in amt_search
assert "if (!budget.begin()) break;" in fallback_loop
assert "budget.stop();" in fallback_loop
assert "budget.observe(have_new_candidates);" in fallback_loop
assert "used_broad_fallback = true;" in fallback_loop
assert "const bool original_has_candidates =" in amt_search
assert "discogger_search::plausible_release_title(" in amt_search
assert "discogger_search::plausible_release_group_title(" in amt_search
assert "master->sub_releases" in amt_search
assert "Native grouped version original-result fallback gate failed." in (
    root / "find_release_dialog.cpp").read_text(encoding="utf-8-sig")
assert "if (!original_has_candidates || !original_title_plausible)" in amt_search
assert "fake_artist->master_releases.get_count() > prior_masters" in fallback_loop
assert "fake_artist->releases.get_count() > prior_releases" in fallback_loop
assert "if (have_new_candidates)" in fallback_loop
nonempty_result = fallback_loop.split("if (have_new_candidates) {", 1)[1].split(
    "            }", 1)[0]
assert "break;" not in nonempty_result
assert "review release/version before tagging" in nonempty_result
native_fallback = (root / "find_release_dialog.cpp").read_text(encoding="utf-8-sig")
assert "Native fallback stopped on an unrelated first result." in native_fallback
assert "Native structured-title fallback confidence gate failed." in native_fallback
assert "Native later fallback phase lost a review-only result." in native_fallback
parsed_releases = (root / "discogs.cpp").read_text(encoding="utf-8-sig")
release_section = parsed_releases[parsed_releases.index("void Discogs::parseArtistReleases("):
                                  parsed_releases.index("void Discogs::parseMasterVersions(")]
assert release_section.count("if (!present)") >= 2
# A repeated Discogs result must not demote a Main release or append
# duplicate role labels. The actual parser is exercised in native host QA.
assert "static void merge_search_result_role(" in parsed_releases
assert "if (!seen) target.search_roles.add_item(incoming);" in parsed_releases
assert '(incoming.equals("Main") && !target.search_role.equals("Main"))' in parsed_releases
assert "merge_search_result_role(*release, JSONAttributeString(rel, \"role\"));" in parsed_releases
assert "merge_search_result_role(*walk_master_release," in release_section
assert "merge_search_result_role(*walk_release," in release_section
assert 'if (!walk_release->id.equals("Main"))' not in release_section
# External Discogs format arrays may contain null/number/object entries;
# do not construct strings from null, and retain descriptor order/dedup.
assert "if (!json_is_string(name)) continue;" in parsed_releases
assert "if (!json_is_string(desc)) continue;" in parsed_releases
assert "seen_minors.emplace(value).second" in parsed_releases
assert "std::vector<std::string> ordered_minors;" in parsed_releases
assert "(--sminors.end())" not in parsed_releases

plan = (root / "discogger_search_fallback.h").read_text(encoding="utf-8-sig")
cache_pages_contract = (root / "discogger_cache_pages.h").read_text(encoding="utf-8-sig")
assert "pages != 1 + (items - 1) / per_page" in cache_pages_contract
artist_release_loader = (root / "discogs.cpp").read_text(encoding="utf-8-sig")
artist_release_loader = artist_release_loader[
    artist_release_loader.index("void Discogs::Artist::load_releases("):
    artist_release_loader.index("void Discogs::MasterRelease::load_releases(")
]
assert "json_object_get(page->root, \"releases\")" in artist_release_loader
assert "json_is_array(entries)" in artist_release_loader
assert "discogger_cache::valid_cache_payload_count(" in artist_release_loader
assert artist_release_loader.index("valid_cache_payload_count(") < artist_release_loader.index("Artist staged(id.c_str());")

assert "discogger_cache::valid_online_page(" in search
assert "without_terminal_mix_suffix" in plan
assert "build_fallback_plan(" in plan
assert "if (key.size() < 3) return plan;" in plan
assert "title-only discovery (review required)" in plan
# Real authenticated Discogs probe for release 2783977 established a strict
# artist+release_title+type query where broad "Sun & Moon" pages 1-2 failed.
# Only explicit featuring credits produce this extra review-only fallback,
# and independent UTF-8 values must be percent-encoded as fields.
assert "primary_artist_before_featuring(" in plan
assert 'const std::string primary = primary_artist_before_featuring(initial_artist);' in plan
assert '"&release_title="' in plan and '"&type=release"' in plan
assert "percent_encode_search_value(candidate.artist_filter)" in plan
assert "percent_encode_search_value(candidate.release_title_filter)" in plan
assert 'if (!candidate.q.empty() || !candidate.title.empty() ||' in plan
assert 'candidate.release_title_filter.empty()) return {};' in plan
assert "if (!using_track_title) {" in plan
assert 'plan.push_back({"featured primary artist and release title (review required)",' in plan
assert plan.index('plan.push_back({"featured primary artist and release title (review required)",') < plan.index('plan.push_back({"collaboration partner and title",')
# A guest/title query can return an unrelated release and stop the fallback.
# Therefore the stricter, authenticated primary-artist query comes FIRST.
assert "if (!original_has_candidates || !original_title_plausible)" in search
assert 'presentation_query["artist="].first.clear();' in search
print("PASS: safe, bounded featured-primary fallback follows empty or implausible original release-title results.")
print("PASS: artist refresh, HTTP rate-limit routing and collaboration fallback are wired.")
print("NOTE: full component compilation and live Discogs/foobar testing are separate gates.")

# Static build safety: neither platform may install a DLL as a build side effect.
# This does not assert that the complete component can be built.
import xml.etree.ElementTree as ET

msbuild_ns = "{http://schemas.microsoft.com/developer/msbuild/2003}"
project = ET.parse(root / "foo_discogger.vcxproj").getroot()
configs = {node.attrib["Include"] for node in project.iter(msbuild_ns + "ProjectConfiguration")}
assert configs == {"Debug|Win32", "Release|Win32", "Debug|x64", "Release|x64"}
assert not list(project.iter(msbuild_ns + "PostBuildEvent"))
imports = {node.attrib.get("Project") for node in project.iter(msbuild_ns + "Import")}
assert {"PlatformPropertySheet.props", "PlatformPropertySheet_64.props"} <= imports

for name in ("PlatformPropertySheet.props", "PlatformPropertySheet_64.props"):
    prop = ET.parse(root / name).getroot()
    assert not list(prop.iter(msbuild_ns + "PostBuildEvent")), name
    source = (root / name).read_text(encoding="utf-8-sig").lower()
    for forbidden in ("c:", "foobar2000.exe", "foo_cmd_dir", "foo_comp_wd"):
        assert forbidden not in source, f"{name}: forbidden build-time host setting {forbidden}"
print("PASS: MSBuild Win32/x64 properties have no implicit host deployment.")

# Static SQLite include/link consistency across all four configurations.
# This does not claim that the libraries exist or that the DLL compiles.
checked = set()
for item in project.iter(msbuild_ns + "ItemDefinitionGroup"):
    condition = item.attrib.get("Condition", "")
    config = next((name for name in configs if name in condition), None)
    if config is None:
        continue
    checked.add(config)
    include = item.find(msbuild_ns + "ClCompile/" + msbuild_ns + "AdditionalIncludeDirectories")
    dependencies = item.find(msbuild_ns + "Link/" + msbuild_ns + "AdditionalDependencies")
    assert include is not None and dependencies is not None, config
    assert "../discogger libs/sqlite-amal-3401/Include" in (include.text or ""), config
    assert "discogger libs" in (dependencies.text or ""), config
    assert "sqlite-amal-3401" in (dependencies.text or ""), config
    if config.startswith("Release|"):
        security = item.find(msbuild_ns + "ClCompile/" + msbuild_ns + "BufferSecurityCheck")
        assert security is not None and security.text == "true", config
assert checked == configs
assert "disgogger libs" not in (root / "foo_discogger.vcxproj").read_text(encoding="utf-8-sig")
print("PASS: all four SQLite paths and both Release stack protections are consistent.")

# Both preflight boundaries must reject virtual handles before invoking a writer.
# These source checks do not establish real foobar2000 host behavior.
tasks = (root / "tasks.cpp").read_text(encoding="utf-8-sig")
manager = (root / "file_info_manager.cpp").read_text(encoding="utf-8-sig")
assert tasks.index("discogger_require_physical_targets(m_tag_writer->m_finfo_manager->items);") < tasks.index("m_tag_writer->write_tags();")
assert manager.index("discogger_require_physical_targets(items);") < manager.index("api->update_info_async_simple(")
assert "discogger_physical_targets.h" in tasks and "discogger_physical_targets.h" in manager
assert manager.index("discogger_inspect_file_targets(items, false)") < manager.index("api->load_info_multi(")
assert "Writing tags failed; dependent artwork was not started." in manager
# Failed/aborted tag generation must revoke metadata and preview authorization,
# even if physical metadata restoration itself raises an exception.
error_cleanup = tasks[tasks.index("void write_tags_task::on_error("):tasks.index("class att_vcmp")]
assert "cleanup_failed_preview(" in error_cleanup
assert "discard_pending_changes();" in error_cleanup
assert "m_tag_writer->ResetMask();" in error_cleanup
assert "if (cleanup_failures)" in error_cleanup
abort_cleanup = tasks[tasks.index("void write_tags_task::on_abort("):tasks.index("void write_tags_task::on_error(")]
assert "on_error(p_wnd);" in abort_cleanup
assert "cleanup_failed_preview(" in (root / "discogger_write_safety.h").read_text(encoding="utf-8-sig")
assert "display_errors();" in manager[manager.index("void write_tags_completion::on_completion"):manager.index("void read_tags_completion::on_completion")]
assert "discogger_require_physical_targets(m_tag_writer->m_finfo_manager->items);" in tasks
assert "discogger_inspect_file_targets(m_items, false)" in tasks
assert tasks.index("discogger_require_physical_targets(m_tag_writer->m_finfo_manager->items);", tasks.index("void write_tags_task::on_success")) < tasks.index("inspect_tag_deletions(")
adapter = (root / "discogger_physical_targets.h").read_text(encoding="utf-8-sig")
pure_targets = (root / "discogger_write_safety.h").read_text(encoding="utf-8-sig")
assert "inspect_target_batch(" in adapter and "pfc::stringCompareCaseInsensitive" in adapter
for outcome in ("reject_empty_selection", "reject_null_handle", "reject_empty_path",
                "reject_virtual_subsong", "reject_duplicate_path"):
    assert "target_batch_decision::" + outcome in adapter
    assert outcome in pure_targets
print("PASS: physical tag/artwork target preflight rejects null, empty, virtual and duplicate paths; preview remains read-only.")

# SDK 2026-09-16+ requires C++20; portable helper tests may remain C++17.
for item in project.iter(msbuild_ns + "ItemDefinitionGroup"):
    condition = item.attrib.get("Condition", "")
    config = next((name for name in configs if name in condition), None)
    if config is None:
        continue
    standard = item.find(msbuild_ns + "ClCompile/" + msbuild_ns + "LanguageStandard")
    assert standard is not None and standard.text == "stdcpp20", config
print("PASS: all four actual component configurations request SDK-required C++20.")

# Dependent artwork must be callback-driven, not launched while tag I/O is pending.
header = (root / "file_info_manager.h").read_text(encoding="utf-8-sig")
assert "write_infos(std::function<void()> after_success" in header
assert "write_completion_gate m_gate" in header
assert "stable_write_copies<file_info_impl> staged_infos" in header
assert "std::shared_ptr<discogger_async_tag_payload> m_write_payload;" in header
assert "m_write_payload(std::move(write_payload))" in header
assert "payload->infos_to_write[outptr] = payload->staged_infos.append_copy(wrapper->info);" in manager
assert "std::move(keep_alive), payload);" in manager
assert ">(payload->items_to_update, outptr)" in manager
assert ">(payload->infos_to_write, outptr)" in manager
assert manager.index("payload->staged_infos.append_copy(wrapper->info)") < manager.index("api->update_info_async_simple(")
assert "m_gate.finish(success" in manager
assert "m_tag_writer->m_finfo_manager->write_infos(std::move(artwork_after_tags), m_write_lease, tag_deletions_approved)" in tasks
assert "artwork_after_tags = [release_id, physical_items, bfile_match, artwork_lease]" in tasks
assert tasks.index("artwork_after_tags = [release_id") < tasks.index("m_tag_writer->m_finfo_manager->write_infos(std::move(artwork_after_tags), m_write_lease, tag_deletions_approved)")
assert "new service_impl_t<download_art_task>" in tasks[tasks.index("artwork_after_tags = [release_id"):tasks.index("m_tag_writer->m_finfo_manager->write_infos(std::move(artwork_after_tags), m_write_lease, tag_deletions_approved)")]
print("PASS: dependent artwork is gated by asynchronous physical tag completion; positive/negative portable gate tests are separate.")

# Real physical metadata no-op: compare the pre-edit snapshot including ReplayGain.
# Artwork no-op and actual host byte/timestamp preservation remain separate gates.
manager_header = (root / "file_info_manager.h").read_text(encoding="utf-8-sig")
writer = (root / "tag_writer.cpp").read_text(encoding="utf-8-sig")
assert "m_finfo_manager->invalidate_all();" in writer
assert "initial_metadata_read m_initial_read" in manager_header
assert "m_initial_read.begin();" in manager_header
assert "begin_physical_read();" in manager
assert "m_initial_read.finish(true, m, loaded_count)" in manager
assert manager.index("begin_physical_read();") < manager.index("api->load_info_multi(")
assert "revoke_snapshot_batch(info_wrappers.get_count()" in manager_header
assert "wrapper->original_captured = false;" in manager_header
assert "if (!complete) begin_physical_read();" in manager
assert manager_header.index("begin_physical_read();", manager_header.index("void discard_pending_changes()")) < manager_header.index("inline void invalidate_all()")
assert "if (!info_wrappers[p_index] || !info_wrappers[p_index]->mask)" in manager_header
assert manager_header.index("if (!info_wrappers[p_index] || !info_wrappers[p_index]->mask)") < manager_header.index("wrapper->original_info = wrapper->info;")
assert manager_header.index("if (!m_initial_read.ready())") < manager_header.index("wrapper->original_info = wrapper->info;")
assert "if (items.get_count() != info_wrappers.get_count())" in manager_header
assert "wrapper->original_info = wrapper->info;" in manager_header
assert "wrapper->original_captured = true;" in manager_header
assert "original_captured" in manager
assert "file_info::g_is_meta_equal" in manager
assert "replaygain_info::g_equal" in manager
assert manager.index("plan_metadata_write(") < manager.index("api->update_info_async_simple(")
assert "metadata_write_decision::skip" in manager
print("PASS: physical metadata no-op snapshot and ReplayGain comparison are wired; artwork and host gates remain open.")

# Reject accidental literal backslash-t indentation in C++ source edits.
for name in ("file_info_manager.cpp", "file_info_manager.h"):
    lines = (root / name).read_text(encoding="utf-8-sig").splitlines()
    assert not any(line.startswith("\\t") for line in lines), name

# Official SDK core compilation is a real MSBuild gate, not a Discogger DLL gate.
workflow = (root / ".github/workflows/validation.yml").read_text(encoding="utf-8-sig")
sdk_build = (root / "scripts/build-official-sdk-libs.ps1").read_text(encoding="utf-8-sig")
assert "run: ./scripts/probe-official-sdk.ps1" in workflow
assert "run: ./scripts/build-official-sdk-libs.ps1" in workflow
assert "microsoft/setup-msbuild@v2" in workflow
for required in ("pfc.vcxproj", "foobar2000_SDK.vcxproj", "foobar2000_component_client.vcxproj", "foobar2000_sdk_helpers.vcxproj", "libPPUI.vcxproj", "'Win32', 'x64'", "/t:Build", "Release", "LASTEXITCODE"):
    assert required in sdk_build, required
assert "10.0.9163" in sdk_build
assert "0ae7fd90a938d97da7532eae98e824450861c1cd88c6a2a4e7ab9cbe5ec6bedb" in sdk_build
assert "dotnet nuget verify --all" in sdk_build
assert "atlapp.h" in sdk_build
assert "upload-artifact" not in workflow
print("PASS: official SDK Win32/x64 compilation is wired without publishing binaries.")

# SQLite wrapper and linker must resolve the same source release.
wrapper = (root / 'sqlite3.h').read_text(encoding='utf-8-sig')
assert 'sqlite-amal-3401' in wrapper
assert 'sqlite-amal-336' not in (root / 'foo_discogger.vcxproj').read_text(encoding='utf-8-sig')
print('PASS: SQLite wrapper/include/link version consistency (3.40.1).')

# All long network backoff sites must be abort-aware, not raw Sleep().
assert '#include "interruptible_wait.h"' in fetcher
assert fetcher.count("discogger_net::interruptible_wait(") == 4
assert "Sleep(dw);" not in fetcher
assert "Sleep(2000 * tries);" not in fetcher
assert "Sleep(2000 * (tries > 1 ? 2 : 1));" not in fetcher
assert "tests/interruptible_wait_test.cpp" in workflow
print("PASS: all four throttle/retry backoffs are cancellation-aware and tested.")

# Require ASLR in all four Windows component configurations.
for item in project.iter(msbuild_ns + "ItemDefinitionGroup"):
    condition = item.attrib.get("Condition", "")
    config = next((name for name in configs if name in condition), None)
    if config is None:
        continue
    aslr = item.find(msbuild_ns + "Link/" + msbuild_ns + "RandomizedBaseAddress")
    assert aslr is not None and aslr.text == "true", config
print("PASS: Win32/x64 Debug/Release DLL configurations enable ASLR.")

# SDK C++20 migration: no char8_t stream insertions or dangling references.
assert 'u8" \\u03BCs)"' not in fetcher
assert '"\\xCE\\xBCs)"' in fetcher
assert 'auto & part = sub_array[i].print_raw();' not in (root / 'string_encoded_array.cpp').read_text(encoding='utf-8-sig')
assert 'out.first = sanitized.get_ptr();' in (root / 'querydefmap.cpp').read_text(encoding='utf-8-sig')
assert 'out.second = scaped.get_ptr();' in (root / 'querydefmap.cpp').read_text(encoding='utf-8-sig')
for name in ('tags.h', 'tags.cpp', 'tag_writer.cpp', 'foo_discogs.cpp', 'tag_mappings_dialog.cpp'):
    source = (root / name).read_text(encoding='utf-8-sig')
    assert 'discogger_is_multivalue_meta(' in source, name
assert 'discogger_multivalue::contains(CONF.multivalue_fields.get_ptr(), field.get_ptr())' in (root / 'tags.h').read_text(encoding='utf-8-sig')
assert 'multivalue_fields_test.cpp' in workflow
assert 'multivalue_fields_test.cpp' in (root / 'scripts/run-msvc-unit-tests.ps1').read_text(encoding='utf-8-sig')
print('PASS: C++20 compatibility fixes and exact-token configurable multivalue mapping are wired.')

# Pinned third-party diagnostic staging is not a DLL/link success claim.
probe = (root / 'scripts/diagnose-component-compile.ps1').read_text(encoding='utf-8-sig')
for expected in ('684e18c927e89615c2d501737e90018f4930d6c5',
                 '49112cc7328392aa4e3e5dae0b2f6736d0153430143d21f69327788ff4efe734',
                 'sqlite-amalgamation-3400100.zip', 'Get-FileHash',
                 'jansson_config.h', 'JANSSON_WITHOUT_TESTS=ON',
                 'Jansson revision mismatch', 'SQLite source SHA-256 mismatch'):
    assert expected in probe, expected
assert 'exit 0' in probe  # Nonblocking compiler diagnostic remains explicitly non-release.
print('PASS: Jansson and SQLite header staging pins and fail-closed integrity checks are wired.')

# Historical db_fetcher.h is absent upstream; only opaque pointers are used.
opaque = (root / 'db_fetcher.h').read_text(encoding='utf-8-sig')
assert 'class db_fetcher;' in opaque
assert 'class db_fetcher {' not in opaque
assert '#include "db_fetcher.h"' in (root / 'discogs.h').read_text(encoding='utf-8-sig')
assert 'console::print((PFC_string_formatter() << "foo_discogger: " << msg).c_str());' in (root / 'utils.h').read_text(encoding='utf-8-sig')
print('PASS: legacy DB pointer header is opaque and SDK console formatter is explicit.')

assert 'cache_lru_test.cpp' in workflow
assert 'cache_lru_test.cpp' in (root / 'scripts/run-msvc-unit-tests.ps1').read_text(encoding='utf-8-sig')
assert 'mp_result(nullptr)/' not in (root / 'prompt_dialog.h').read_text(encoding='utf-8-sig')
cache_src = (root / 'cache.h').read_text(encoding='utf-8-sig')
assert 'bool try_get(' in cache_src and 'catch (...)' in cache_src
print('PASS: qualified LRU cache regressions and prompt constructor are wired.')

# Repair root SDK compile errors without removing exposed metadata tags.
discogs_header = (root / 'discogs.h').read_text(encoding='utf-8-sig')
assert 'string_encoded_array get_query_major_formats_qty() const' in discogs_header
assert 'm["QUERY_MAJOR_FORMATS_QTY"]' in discogs_header
assert 'std::pair<size_t, size_t> ignored{0, 0};' in discogs_header
assert discogs_header.index('namespace Discogs { class missing_data_exception') < discogs_header.index('#include "exposetags.h"')
assert 'auto it = exposed_tags.find(tag_name.get_ptr());' in (root / 'exposetags.h').read_text(encoding='utf-8-sig')
print('PASS: exposed metadata methods, exception declaration and legacy parser overload are preserved.')

# Verify the targeted MSVC C++20 header repairs remain present.
release_utils = (root / 'find_release_utils.h').read_text(encoding='utf-8-sig')
assert 'mounted_param::' not in release_utils
assert 'id_tracer::' not in release_utils
assert 'auto& it = cache.find(' not in release_utils
assert release_utils.count('auto it = cache.find(') == 4
artwork_types = (root / 'track_matching_utils.h').read_text(encoding='utf-8-sig')
assert 'inline bool operator==(const uartwork& rhs) const {' in artwork_types
print('PASS: in-class qualifiers, temporary iterators and C++20 artwork comparison repairs.')

# Compile-blocker regressions: no undeclared cache path variable, explicit
# callback pointer declaration, and SDK-safe path string construction.
cache_header = (root / 'ol_cache.h').read_text(encoding='utf-8-sig')
assert 'bfolder_exists' not in cache_header
assert 'fs::create_directories(os_path, ec);' in cache_header
assert 'fs::is_directory(os_path, ec)' in cache_header
reader = (root / 'discogs_interface.cpp').read_text(encoding='utf-8-sig')
assert 'complete_cache_page_sequence(indexes)' in reader
assert 'valid_cache_pagination(page.first, ordered_pages.size()' in reader
assert 'valid_cache_payload_count(page.first, ordered_pages.size()' in reader
assert 'get_all_pages(id, params, p_abort, msg, p_status)' not in reader
discogs_source = (root / 'discogs.cpp').read_text(encoding='utf-8-sig')
assert 'Refreshing invalid offline artist cache...' in discogs_source
assert 'Refreshing invalid offline master versions cache...' in discogs_source
assert 'class foo_discogs_threaded_locked_process_callback;' in (root / 'foo_discogs.h').read_text(encoding='utf-8-sig')
assert 'return p ? p->id : pfc::string8();' in (root / 'find_artist_list.h').read_text(encoding='utf-8-sig')
path_header = (root / 'utils_path.h').read_text(encoding='utf-8-sig')
assert 'path << (core_api::pathInProfile(' not in path_header
assert 'db_path << core_api::pathInProfile(' not in path_header
assert path_header.count('path.add_string(core_api::get_my_file_name());') == 2
print('PASS: cache folder, callback declaration, artist ID and SDK path header regressions.')

# Keep the native matching-mode enum legal in C++20 and references consistent.
for name in ('track_matching_dialog_presenter.h', 'track_matching_dialog.h', 'track_matching_dialog.cpp'):
    source = (root / name).read_text(encoding='utf-8-sig')
    assert 'lsmode::default' not in source.replace('lsmode::default_mode', '')
assert 'unknown, default_mode = 0' in (root / 'track_matching_dialog_presenter.h').read_text(encoding='utf-8-sig')

# String literals are immutable in C++20; Windows MENUITEMINFO still expects
# a mutable LPWSTR buffer, not a const_cast of a string literal.
history = (root / 'history_oplog.cpp').read_text(encoding='utf-8-sig')
assert 'const char* const qry_tmpl[]' in history
assert 'wchar_t submenu_add[]' in history
assert 'LPWSTR submenus_data[3]' in history
assert 'char* qry_tmpl[]' not in history

# Artist load state must refer to the actual member on every path.
for name in ('tasks.h', 'tasks.cpp'):
    source = (root / name).read_text(encoding='utf-8-sig')
    assert 'bload_releases |=' not in source.replace('m_bload_releases |=', '')
assert 'get_artist(m_artist_id, m_bload_releases,' in (root / 'tasks.cpp').read_text(encoding='utf-8-sig')
print('PASS: matching-mode keyword, Win32 menu buffers and artist load-state references.')

# Entire physical batch must be preflighted before dereferencing wrappers or queuing I/O.
assert "discogger_write_safety::plan_snapshot_batch(" in manager
assert "count, info_wrappers.get_count()" in manager
assert manager.index("discogger_write_safety::plan_snapshot_batch(") < manager.index("payload->items_to_update.set_size(count)")
assert manager.index("discogger_write_safety::plan_snapshot_batch(") < manager.index("api->update_info_async_simple(")
for outcome in ("reject_count_mismatch", "reject_missing_wrapper", "reject_missing_snapshot"):
    assert "snapshot_batch_decision::" + outcome in manager
assert "scope.Open(core_api::get_main_window());" in artist
assert "scope.Open(core_api::get_main_window(), true);" not in artist
dialog_header = (root / "find_release_dialog.h").read_text(encoding="utf-8-sig")
assert "NOTIFY_HANDLER(IDC_RELEASE_TREE, TVN_DELETEITEM, OnDeleteTreeItem)" not in dialog_header
assert "NOTIFY_HANDLER(IDC_RELEASE_TREE, NM_DBLCLK, OnReleaseTreeDoubleClickRelease)" in (root / "find_release_tree.h").read_text(encoding="utf-8-sig")
print("PASS: batch snapshot preflight and native clipboard/tree callback API contracts.")

# Artist/title/release composite-search declarations must match their
# unconditional definitions; a stale SEARCH_AT guard hid the entire parser.
interface_header = (root / "discogs_interface.h").read_text(encoding="utf-8-sig")
interface_impl = (root / "discogs_interface.cpp").read_text(encoding="utf-8-sig")
assert "#ifdef SEARCH_AT" not in interface_header
assert "struct parse_amt_info" in interface_header
assert "void process_amt_parsed_fake_artist(" in interface_header
assert "void parse_amt_page(" in interface_header
assert "void DiscogsInterface::parse_amt_page(" not in interface_header
assert "void DiscogsInterface::process_amt_parsed_fake_artist(" in interface_impl
assert "void DiscogsInterface::parse_amt_page(" in interface_impl
print("PASS: composite-search parser declarations are visible and C++-qualified correctly.")

# Real authenticated advanced search can retain a parsed release in several
# artist buckets, then read its ID to populate the Discogger cache. Moving
# the loop-local shared_ptr into the FIRST bucket invalidates later reads.
# Never move it into any artist bucket: the owning parsed set, UI and cache
# share lifetime while the worker walks the complete response.
advanced_search = interface_impl.split(
    "void DiscogsInterface::process_amt_parsed_fake_artist(", 1
)[1].split("void DiscogsInterface::parse_amt_page(", 1)[0]
for add in (
    "tmpArtist->master_releases.add_item(mr);",
    "no_checkArtist->master_releases.add_item(mr);",
    "tmpArtist->releases.add_item(r);",
    "no_checkArtist->releases.add_item(r);",
):
    assert add in advanced_search, f"advanced search lost shared ownership: {add}"
for moved in ("std::move(mr)", "std::move(r)"):
    assert moved not in advanced_search, f"advanced search invalidates shared release: {moved}"
assert "encode_mr(atoi(tmpArtist->id), atoi(mr->id))" in advanced_search
assert "encode_mr(atoi(no_checkArtist->id), atoi(mr->id))" in advanced_search
print("PASS: advanced live search safely retains shared master/release ownership.")

# Restore the existing search/filter task base type and serialize its lock
# counter transitions; do not confuse it with the separate async tag-write gate.
task_header = (root / "tasks.h").read_text(encoding="utf-8-sig")
assert "class foo_discogs_threaded_locked_process_callback : public foo_discogs_threaded_process_callback" in task_header
assert "foo_discogs_locked_threaded_process_callback()" not in task_header
assert task_header.count("std::lock_guard<std::mutex> guard(g_discogs->locked_operation_rw_mutex);") == 2
assert "throw locked_task_exception(\"Operation locked\");" in task_header
assert "class write_tags_task : public foo_discogs_threaded_process_callback" in task_header
assert "acquire_physical_write_lease()" in task_header
print("PASS: search/filter lock base and mutex-guarded counter transitions are wired.")

# An advanced search already holds the operation lock while on_success
# starts the presentation callback. That continuation must NOT reacquire it.
assert "class search_artist_process_callback : public foo_discogs_threaded_locked_process_callback" in task_header
assert "class on_search_artist_done_process_callback : public foo_discogs_threaded_process_callback" in task_header
assert "class on_search_artist_done_process_callback : public foo_discogs_threaded_locked_process_callback" not in task_header
tasks_implementation = (root / "tasks.cpp").read_text(encoding="utf-8-sig")
parent_completion = tasks_implementation.split("void search_artist_process_callback::on_success(", 1)[1].split(
    "void search_artist_process_callback::on_abort(", 1)[0]
assert "new service_impl_t<on_search_artist_done_process_callback>(" in parent_completion
print("PASS: search completion can display results without reacquiring its parent's operation lock.")

# Empty-result status is also reached after restored or interrupted queries.
# Never throw at("artist=") on an incomplete map, and never infer a size_t
# limit from sizeof(size_t) (bytes versus bits). A unique advanced-search
# artist with release previews must populate the tree without another click.
completion_dialog = (root / "find_release_dialog.cpp").read_text(encoding="utf-8-sig")
status_start = completion_dialog.index("pfc::string8 search_query_run_message(")
status_end = completion_dialog.index("void CFindReleaseDialog::on_search_artist_done(", status_start)
status_text = completion_dialog[status_start:status_end]
assert 'qdm_search_query.find("artist=")' in status_text
assert 'qdm_search_query.end()' in status_text
assert 'qdm_search_query.at("artist=")' not in status_text
assert 'constexpr size_t uncapped_threshold = SIZE_MAX - 100;' in status_text
assert "pow(2, sv)" not in status_text
result_body = completion_dialog.split("void CFindReleaseDialog::on_search_artist_done(", 1)[1].split(
    "void CFindReleaseDialog::OnTypeFilterTimer(", 1)[0]
assert "m_query_mode & SearchMode::AT" in result_body
assert "artists.get_count() == 1" in result_body
assert "only->releases.get_count() || only->master_releases.get_count()" in result_body
assert "ShowArtistSearchResults(only);" in result_body
assert "artists.get_count() > 1" in result_body
assert "combined_advanced_search_preview(artists)" in result_body
assert "ShowArtistSearchResults(preview);" in result_body
assert "write_infos(" not in result_body
assert "std::unordered_set<std::string> seen_masters, seen_releases;" in completion_dialog
assert "preview->search_order_master.append_single(true);" in completion_dialog
assert "preview->search_order_master.append_single(false);" in completion_dialog
print("PASS: advanced searches auto-preview all artist candidates with ID dedup and no tagging.")

# A deliberate repeat must traverse the real search path (including cache refresh),
# and GUI-only type=all must not be serialized into the Discogs API request.
search_click = completion_dialog.split("LRESULT CFindReleaseDialog::OnButtonSearch(", 1)[1].split(
    "void CFindReleaseDialog::on_get_artist_done(", 1)[0]
for marker in ('if (!exp_pair.first.compare(m_search_expression)) {',
               'if (trot_text.equals(m_search_artist)) {'):
    repeat_block = search_click.split(marker, 1)[1].split("}", 1)[0]
    assert "return FALSE;" not in repeat_block
assert 'search_query::MapToText(m_qdm_search_query, exp_pair, false, false);' in search_click
assert 'search_query::MapToText(m_qdm_search_query, exp_pair, true, false);' not in search_click
print("PASS: explicit repeated searches remain routable and omit GUI-only API type.")


# Preserve conditional Wine light-theme redraw with current libPPUI:
# repaint on initialization and focus transitions, without a paint-loop hook.
edit_header = (root / "my_editwithbuttons.h").read_text(encoding="utf-8-sig")
assert "void SetConditionedInvalidation(std::function<bool()> predicate)" in edit_header
assert "m_conditioned_invalidation = std::move(predicate);" in edit_header
assert "m_conditioned_invalidation && m_conditioned_invalidation()" in edit_header
assert "Invalidate(); // redraw the edit and its embedded libPPUI buttons" in edit_header
assert edit_header.count("RefreshConditionedInvalidation();") >= 3
print("PASS: conditional native edit/button repaint is implemented, not stubbed.")

# Compile-path regressions for Discogs release track routing and native UI.
parser = (root / "discogs.cpp").read_text(encoding="utf-8-sig")
assert "append_single(std::move(track))" not in parser
assert parser.count("append_single(track);") >= 2
assert parser.count("track->disc_track_two_sided = parse_nfo.bformat_incl_sideb;") == 2
assert parser.count("track->disc_track_side = atoi(parse_nfo.side_trk_postfix);") == 2
assert "track->disc_track_in_side" not in parser
assert "} // end new track precond 2" not in parser
assert "else if (!parse_nfo.btrk_alpha && can_advance_subtrack)" in parser
assert "if ((can_advance_continuous_subtracks)" in parser
assert "// ROUTE MERGE TITLES" in parser and "// discard track..." in parser
cfg_dialog = (root / "configuration_dialog.cpp").read_text(encoding="utf-8-sig")
assert "tmpcfg.search_release_format_string;,[%" not in cfg_dialog
assert '<< tmpcfg.search_release_format_string;' in cfg_dialog
release_dialog = (root / "find_release_dialog.cpp").read_text(encoding="utf-8-sig")
assert "m_query_custom_fs->" not in release_dialog
assert "m_query_custom_script->run_hook(" in release_dialog
assert "using foo_discogs_locked_threaded_process_callback = foo_discogs_threaded_locked_process_callback;" in task_header
print("PASS: release routing, shared track lifetime, native query, config syntax and read lock base.")

# C++20 native string/Win32 mutable-buffer and temporary lifetime guards.
native_utils = (root / "utils.cpp").read_text(encoding="utf-8-sig")
assert "std::string tmpstr(in.c_str());" in native_utils
assert "std::string str_suffix(trim(i->suffix().str().c_str()).c_str());" in native_utils
assert "CWindow* cwnd = &CWindow(walk);" not in native_utils
assert "CWindow cwnd(walk);" in native_utils
assert "catch (std::regex_error e) {\n\t\treturn tracks;" in native_utils
history = (root / "history_oplog.cpp").read_text(encoding="utf-8-sig")
assert "frm_artist : pfc::string8()" in history
leading = (root / "preview_leading_tag_dialog.cpp").read_text(encoding="utf-8-sig")
assert "std::string tab_text(m_tab_table[n].m_pszName);" in leading
assert "item.pszText = tab_text.data();" in leading
assert "item.pszText = m_tab_table[n].m_pszName;" not in leading
print("PASS: native string, CWindow lifetime and writable Win32 tab labels.")

# Modern cfg_objList snapshot/update contract: no range-for or writes to by-value operator[].
tag_source = (root / "tags.cpp").read_text(encoding="utf-8-sig")
foo_init = (root / "foo_discogs.cpp").read_text(encoding="utf-8-sig")
assert "auto mappings = cfg_tag_mappings.get();" in tag_source
assert "cfg_tag_mappings.set(std::move(mappings));" in tag_source
assert "cfg_tag_mappings.set(std::move(defaults));" in tag_source
assert "cfg_tag_mappings.set(std::move(snapshot));" in tag_source
assert "std::unique_ptr<tag_mapping_entry>" in tag_source
assert "std::string what(tagname.toLower().get_ptr());" in tag_source
assert "for (auto& entry : cfg_tag_mappings)" not in tag_source
assert "for (auto & tag_entry : cfg_tag_mappings)" not in tag_source
assert "for (auto& tag_entry : cfg_tag_mappings)" not in tag_source
assert "auto tag_mappings = cfg_tag_mappings.get();" in foo_init
assert "cfg_tag_mappings.set(std::move(tag_mappings));" in foo_init
print("PASS: modern cfg_objList snapshot, mutation and one-shot persistence contract.")

# Native tag-mapping menu constness and cfg_objList read API.
mapping_ui = (root / "tag_mappings_dialog.cpp").read_text(encoding="utf-8-sig")
assert "cfg_tag_mappings.get_item(i)" not in mapping_ui
assert "auto saved_mapping = cfg_tag_mappings[i];" in mapping_ui
assert 'wchar_t menu_more_text[] = L"Other defaults...";' in mapping_ui
assert "menu_more_info.dwTypeData = menu_more_text;" in mapping_ui
assert "LPWSTR submenus_data" not in mapping_ui
assert mapping_ui.count("wchar_t submenus_data") == 3
print("PASS: native mapping menu immutable labels and mutable Win32 item buffer.")

# C++20 native UTF-8 filesystem and current libPPUI clipboard API contracts.
cache_source = (root / 'ol_cache.h').read_text(encoding='utf-8-sig')
assert 'const std::string utf8_path(u8str.begin(), u8str.end());' in cache_source
assert 'folders.add_item(utf8_path.c_str());' in cache_source
assert 'folders.add_item(u8str.c_str());' not in cache_source
for native_dialog, count in [('find_release_tree.cpp', 2), ('preview_leading_tag_dialog.cpp', 1)]:
    source = (root / native_dialog).read_text(encoding='utf-8-sig')
    assert source.count('scope.Open(core_api::get_main_window());') == count, native_dialog
    assert 'scope.Open(core_api::get_main_window(), true);' not in source, native_dialog
print('PASS: C++20 UTF-8 cache enumeration and libPPUI clipboard API contracts.')

# Modern foobar SDK cfg_objList migration regression gates.
conf = (root / "conf.cpp").read_text(encoding="utf-8-sig")
for obsolete in (".truncate(", ".remove_by_idx(", ".sort_t(", ".bsearch_t(", ".get_item(", ".replace_item(", ".swap_item_with("):
    assert obsolete not in conf, obsolete
for ident in ("DEPRI_CFG_SKIP_RELEASE_DLG_IF_MATCHED", "DEPRI_CFG_SKIP_FIND_RELEASE_DLG_IF_IDED", "DEPRI_CFG_SKIP_PREVIEW_DIALOG"):
    assert "discogger_cfg::take_by_id(cfg_bool_entries, " + ident in conf
assert "cfg_bool_entries.set(std::move(bool_snapshot))" in conf
assert "cfg_int_entries.set(std::move(int_snapshot))" in conf
assert "cfg_string_entries.set(std::move(string_snapshot))" in conf
for file in (".github/workflows/validation.yml", "scripts/run-msvc-unit-tests.ps1"):
    assert "cfg_snapshot_ops_test.cpp" in (root / file).read_text(encoding="utf-8-sig")
print("PASS: snapshot config persistence, ID migration and MSVC/portable test wiring.")

# Native C++20 compile-cluster regression contracts (not DLL acceptance).
native_tasks = (root / "tasks.h").read_text(encoding="utf-8-sig")
assert "class write_tags_task : public foo_discogs_threaded_process_callback" in native_tasks
assert "acquire_physical_write_lease()" in native_tasks
assert "discogger_crc::crc32(s.begin(), s.end())" in tasks
assert "crc32_test.cpp" in workflow
assert "crc32_test.cpp" in (root / "scripts/run-msvc-unit-tests.ps1").read_text(encoding="utf-8-sig")
assert "const char *url_prefix;" in (root / "foo_discogs.cpp").read_text(encoding="utf-8-sig")
assert "const char *name;" in (root / "tags.h").read_text(encoding="utf-8-sig")
assert "auto& mbmi = done_fetches.emplace" not in (root / "foo_discogs.cpp").read_text(encoding="utf-8-sig")
cfg_cpp = (root / "configuration_dialog.cpp").read_text(encoding="utf-8-sig")
assert "ori_parsing_merge_titles" in cfg_cpp and "original_parsing_merge_titles" not in cfg_cpp
assert "MERGE_TRACKS_DOTTED_TO_HEADER" not in cfg_cpp
assert "ol::CacheFlags::MERGE_TRACKS_TO_HEADER" in cfg_cpp
assert "ol::CacheFlags::MERGE_TRACKS_TO_HEADER" in (root / "discogs.cpp").read_text(encoding="utf-8-sig")
preview_cpp = (root / "preview_dialog.cpp").read_text(encoding="utf-8-sig")
assert "bool is_default = false;" in preview_cpp and ".default" not in preview_cpp
assert "scope.Open(core_api::get_main_window(), true);" not in preview_cpp
assert "scope.Open(core_api::get_main_window(), true);" not in (root / "track_matching_dialog.cpp").read_text(encoding="utf-8-sig")
find_cpp = (root / "find_release_dialog.cpp").read_text(encoding="utf-8-sig")
assert "test_no_artists_artist_in_title" not in find_cpp
# Ignore historical identifiers in explanatory comments; ban executable debug paths.
assert "debug_ok =" not in find_cpp and "if (debug_ok" not in find_cpp
cache_reader = (root / "discogs_interface.cpp").read_text(encoding="utf-8-sig")
assert "fs::u8path(path_container.c_str())" in cache_reader
assert "const auto utf8 = entry.path().filename().u8string();" in cache_reader
assert "const std::string name(utf8.begin(), utf8.end());" in cache_reader
# Avoid stale-dereference on async artwork path completion after the initiating
# Track Matching dialog is closed and another dialog may replace it.
artwork_header = (root / "tasks.h").read_text(encoding="utf-8-sig")
artwork_tasks = (root / "tasks.cpp").read_text(encoding="utf-8-sig")
artwork_class = artwork_header[artwork_header.index("class download_art_paths_task :"):
    artwork_header.index("class find_deleted_releases_task :")]
assert "HWND m_owner_window = nullptr;" in artwork_class
artwork_start = artwork_tasks[artwork_tasks.index("void download_art_paths_task::start()"):
    artwork_tasks.index("void download_art_paths_task::safe_run(")]
assert "g_discogs->track_matching_dialog == m_dialog" in artwork_start
assert "::IsWindow(m_dialog->m_hWnd)" in artwork_start
artwork_complete = artwork_tasks[artwork_tasks.index("void download_art_paths_task::on_success("):
    artwork_tasks.index("find_deleted_releases_task::find_deleted_releases_task(")]
assert "!::IsWindow(m_owner_window)" in artwork_complete
assert "g_discogs->track_matching_dialog != m_dialog" in artwork_complete
assert "g_discogs->track_matching_dialog->m_hWnd != m_owner_window" in artwork_complete
assert "g_discogs->track_matching_dialog->process_download_art_paths_done(" in artwork_complete
assert "IsWindow(m_dialog->m_hWnd)" not in artwork_complete
# Recomputed tag results may shrink or be empty while a detail modal is open.
# Revalidate the selected index before dereferencing and close empty modals.
preview_task_source = (root / "tasks.cpp").read_text(encoding="utf-8-sig")
preview_success = preview_task_source[preview_task_source.index("void generate_tags_task::on_success("):
    preview_task_source.index("void generate_tags_task::on_abort(")]
assert '#include "discogger_preview_selection.h"' in preview_task_source
assert "if (new_res_count == 0)" in preview_success
assert "discogger_ui::choose_preview_detail(" in preview_success
assert "::DestroyWindow(preview_modal_tag_dialog->m_hWnd)" in preview_success
assert "m_tag_writer->tag_results[new_sel]" not in preview_success
assert "curr_sel - 1 < 0" not in preview_success
for filepath in ("scripts/run-msvc-unit-tests.ps1", ".github/workflows/validation.yml"):
    assert "preview_selection_test.cpp" in (root / filepath).read_text(encoding="utf-8-sig")
# The asynchronous tag generation callback can outlive a modeless preview.
# A closed preview must not fall through to Write Tags using a newer dialog.
generation_source = (root / "tasks.cpp").read_text(encoding="utf-8-sig")
generation_header = (root / "tasks.h").read_text(encoding="utf-8-sig")
generation_class = generation_header[generation_header.index("class generate_tags_task :"):
    generation_header.index("class write_tags_task :")]
assert "HWND m_owner_window = nullptr;" in generation_class
assert "bool origin_is_current() const;" in generation_class
assert "bool generate_tags_task::origin_is_current() const" in generation_source
assert "g_discogs->preview_tags_dialog == m_preview_dialog" in generation_source
assert "g_discogs->track_matching_dialog == m_track_matching_dialog" in generation_source
assert "g_discogs->tag_credit_dialog == m_credits_dialog" in generation_source
generation_start = generation_source[generation_source.index("void generate_tags_task::start()"):
    generation_source.index("void generate_tags_task::safe_run(")]
assert "if (!origin_is_current())" in generation_start
generation_success = generation_source[generation_source.index("void generate_tags_task::on_success("):
    generation_source.index("void generate_tags_task::on_abort(")]
assert "if (!origin_is_current()) {\n        on_error(p_wnd);" in generation_success
assert generation_success.index("if (!origin_is_current())") < generation_success.index("if (m_preview_dialog && m_preview_dialog->IsWindow())")
generation_error = generation_source[generation_source.index("void generate_tags_task::on_error("):
    generation_source.index("void write_tags_task::start()")]
assert "if (origin_is_current() && m_preview_dialog)" in generation_error
assert "if (origin_is_current() && m_track_matching_dialog)" in generation_error
print("PASS: native compile-cluster source regressions (tag write lock, C++20, UI and CRC32).")

# C++20 / SDK cfg_objList mapping ownership and pointer lifetime contracts.
writer = (root / "tag_writer.cpp").read_text(encoding="utf-8-sig")
writer_h = (root / "tag_writer.h").read_text(encoding="utf-8-sig")
assert "m_mapping_snapshot.from_config(TAGS)" in writer
assert "m_mapping_snapshot.from_list(*alt_mappings)" in writer
assert "result->tag_entry = &entry" in writer
assert "m_mapping_snapshot.at(wtags)" in writer
assert "owned_snapshot<tag_mapping_entry> m_mapping_snapshot" in writer_h
assert "TAGS.get_item_ref" not in writer and "tag_mapping_list_type* ptags = &TAGS" not in writer
assert "encode_mr(const int a, const pfc::string8& sb)" in (root / "utils.h").read_text(encoding="utf-8-sig")
assert "operator==(const multi_uartwork& rhs) const" in (root / "track_matching_utils.h").read_text(encoding="utf-8-sig")
for name in ("scripts/run-msvc-unit-tests.ps1", ".github/workflows/validation.yml"):
    assert "mapping_snapshot_test.cpp" in (root / name).read_text(encoding="utf-8-sig")
print("PASS: stable tag mapping ownership and native string/const compatibility contracts.")

# Native component acceptance is an enforced (red on failure) full-link gate.
native_workflow = (root / ".github/workflows/validation.yml").read_text(encoding="utf-8-sig")
native_script = (root / "scripts/build-component-dll.ps1").read_text(encoding="utf-8-sig")
assert "HARD GATE - full Win32 x64 Discogger DLL build and link" in native_workflow
assert "./scripts/build-component-dll.ps1" in native_workflow
assert "/t:Build" in native_script and "Assert-DllMachine" in native_script
assert "Get-FileHash" in native_script and "Strict native component build gate failed" in native_script
assert "Copy-UniqueLib" in native_script and "sqlite3.lib" in native_script
assert "oauthcpp.lib" in native_script and "jansson.lib" in native_script
assert "zlibstatic.lib" in native_script and "libPPUI.lib" in native_script
assert "shared.vcxproj" in (root / "scripts/build-official-sdk-libs.ps1").read_text(encoding="utf-8-sig")
assert "const auto url_utf8 = pfc::stringcvt::string_utf8_from_os(wurl);" in (root / "utils.cpp").read_text(encoding="utf-8-sig")
print("PASS: native MSVC x86/x64 strict DLL link gate, pinned libraries, PE SHA and UTF-8 exception safety.")

# Strict dual-architecture zlib generated config contract.
native = (root / "scripts/build-component-dll.ps1").read_text(encoding="utf-8-sig")
assert "$zconf = Join-Path $zlibBuild 'zconf.h'" in native
assert "Generated zlib zconf.h missing for ${platform}" in native
assert "$nativeIncludes = @($zlibBuild, $includes) -join ';'" in native
assert 'AdditionalIncludeDirectories=$($nativeIncludes.Replace' in native
assert 'Copy-Item -LiteralPath $zconf' not in native
print("PASS: per-architecture CMake zconf.h passed to strict native DLL compiler.")

# CMake zlib deliberately relocates the source-tree zconf.h on out-of-source
# builds; global MSBuild AdditionalIncludeDirectories is insufficient.
native_build = (root / "scripts/build-component-dll.ps1").read_text(encoding="utf-8-sig")
assert "$env:CL = '/I\"' + $zlibBuild + '\" ' + $savedCompilerFlags" in native_build
assert "$savedCompilerFlags = $env:CL" in native_build
assert "finally {\n            $env:CL = $savedCompilerFlags" in native_build
assert 'Invoke-Required "Discogger native full build and LINK $platform"' in native_build
print("PASS: native Win32/x64 zlib CMake generated headers use isolated MSVC CL search path.")
# Isolated host tests also open a disposable external CUE through the real
# Discogger context route and hash both the CUE and its synthetic WAV.
host_gui = (root / "scripts/host-discogger-gui.ps1").read_text(encoding="utf-8-sig")
assert "synthetic-album.cue" in host_gui
assert 'FILE "synthetic-silence.wav" WAVE' in host_gui
assert 'DISCOGGER_GUI_EXTERNAL_CUE_CONTEXT_NO_WRITE_$Architecture=PASS' in host_gui
assert 'The external CUE was changed by a read-only context/dialog operation.' in host_gui
assert '$cueHash' in host_gui and '$cueTicks' in host_gui
assert 'MEDIA_WRITE_SAFETY_$Architecture=NOT_TESTED' in host_gui
print("PASS: native host external CUE context and no-write evidence is enforced.")


# Native Windows resource compiler gate: every named control in .rc must have
# an assigned ID. Missing IDs compile silently in ClCompile but fail in RC.exe.
import re

resource_file = (root / "foo_discogger.rc").read_text(encoding="utf-8-sig")
resource_header = (root / "resource.h").read_text(encoding="utf-8-sig")

def missing_resource_ids(resource_text: str, declarations_text: str) -> set[str]:
    used = set(re.findall(r"\b(?:IDC|IDD|IDI|IDB|IDR|IDS|IDM|IDP|ID)_[A-Z0-9_]+\b", resource_text))
    declared = set(re.findall(r"^\s*#define\s+([A-Z_][A-Z_0-9]*)\b", declarations_text, re.M))
    return used - declared - {"IDC_STATIC"}

assert not missing_resource_ids(resource_file, resource_header), missing_resource_ids(resource_file, resource_header)
assert "IDC_STATIC_CFG_HIST_SIZE2" in resource_file
assert not missing_resource_ids("LTEXT IDC_STATIC_CFG_HIST_SIZE2", resource_header)
assert missing_resource_ids("LTEXT IDC_STATIC_CFG_HIST_SIZE2", resource_header.replace("#define IDC_STATIC_CFG_HIST_SIZE2       2530", "")) == {"IDC_STATIC_CFG_HIST_SIZE2"}
assigned_ids = [
    (name, int(value))
    for name, value in re.findall(r"^\s*#define\s+(IDC_[A-Z0-9_]+)\s+([0-9]+)\s*$", resource_header, re.M)
]
assert [(name, val) for name, val in assigned_ids if val == 2530] == [("IDC_STATIC_CFG_HIST_SIZE2", 2530)]
assert [(name, val) for name, val in assigned_ids if val in (2531, 2532)] == [
    ("IDC_EDIT_CFG_ARTIST_CACHE_HOURS", 2531),
    ("IDC_STATIC_CFG_ARTIST_CACHE_HOURS", 2532),
]
assert re.search(r"^\s*#define\s+_APS_NEXT_CONTROL_VALUE\s+2533\s*$", resource_header, re.M)
print("PASS: all named native RC control IDs resolve; new history label ID is collision-free.")

# RC resources: real PNG signatures may be stored in historical .bmp paths.
resource_script = (root / "scripts/normalize-native-bitmaps.ps1").read_text(encoding="utf-8-sig")
native_script = (root / "scripts/build-component-dll.ps1").read_text(encoding="utf-8-sig")
windows_workflow = (root / ".github/workflows/validation.yml").read_text(encoding="utf-8-sig")
assert "Get-DiscoggerBitmapFormat" in resource_script
assert "resourceNames" in resource_script and "bitmap_tracer_ext_drk.bmp" in resource_script
assert "BITMAPINFOHEADER" not in resource_script or "[UInt32]40" in resource_script
assert "[UInt32]40" in resource_script and "[UInt32]54" in resource_script
assert "Copy-Item -LiteralPath $stagedFile -Destination $Path -Force" in resource_script
assert ". (Join-Path $PSScriptRoot 'normalize-native-bitmaps.ps1')" in native_script
assert "tests/native-bitmap-resource-test.ps1" in windows_workflow
assert "scripts/normalize-native-bitmaps.ps1" in windows_workflow
print("PASS: strict signature-based RC normalization and Windows positive/negative fixtures.")


# Historical bitmap has exactly two erroneous UTF-8 BOM prefixes; do not
# substitute pixel re-encoding for a lossless six-byte prefix cleanup.
normalizer = (root / "scripts/normalize-native-bitmaps.ps1").read_text(encoding="utf-8-sig")
resource_test = (root / "tests/native-bitmap-resource-test.ps1").read_text(encoding="utf-8-sig")
assert "RC_RESOURCE_BOM_CLEANED=" in normalizer
assert "Original BMP length mismatch after BOM stripping" in normalizer
assert "EF-BB-BF-EF-BB-BF-42-4D" in resource_test
assert "Removing the BOM modified historic BMP pixel/palette bytes" in resource_test
assert "BOM-prefixed invalid content was accepted" in resource_test
print("PASS: historic double UTF-8 BOM artifact cleaned losslessly, negative BOM fixtures guarded.")

# Actual dual-architecture LNK1120 regressions: use the repo's existing
# sqlite3ren.h namespace for the C amalgamation, not the vanilla sqlite3.lib.
native = (root / "scripts/build-component-dll.ps1").read_text(encoding="utf-8-sig")
staging = (root / "scripts/diagnose-component-compile.ps1").read_text(encoding="utf-8-sig")
workflow = (root / ".github/workflows/validation.yml").read_text(encoding="utf-8-sig")
renamer = (root / "sqlite3ren.h").read_text(encoding="utf-8-sig")
for name in ("sqlite3_close", "sqlite3_exec", "sqlite3_prepare_v2", "sqlite3_bind_text",
             "sqlite3_initialize", "sqlite3_open_v2", "sqlite3_busy_timeout"):
    assert f"#define {name} discogger_{name}" in renamer
assert 'target_compile_options(sqlite3 PRIVATE' in native
assert "/FI$sqliteRenameCmake" in native
assert "$sqliteRenameHeader = Join-Path $component 'sqlite3ren.h'" in native
assert "'sqlite_symbol_namespace_test.cpp'" in (root / "scripts/run-msvc-unit-tests.ps1").read_text(encoding="utf-8-sig")
assert "tests/sqlite_symbol_namespace_test.cpp" in workflow
assert "[switch] $StageOnly" in staging and "if ($StageOnly) { continue }" in staging
assert "./scripts/diagnose-component-compile.ps1 -StageOnly" in workflow
assert "HARD GATE - full Win32 x64 Discogger DLL build and link" in workflow

# Missing method and WM_CONTEXTMENU linker regressions: preserve native tree
# context menu and the full-cache policy without providing dummy UI callbacks.
conf = (root / "conf.cpp").read_text(encoding="utf-8-sig")
dialog = (root / "find_release_dialog.h").read_text(encoding="utf-8-sig")
tree = (root / "find_release_tree.h").read_text(encoding="utf-8-sig")
assert "bool CConf::auto_load_releases_on_select_ready() const" in conf
assert "Offline::OC_READ | Offline::OC_WRITE" in conf
assert "return auto_rel_load_on_select &&" in conf
assert "MESSAGE_HANDLER(WM_CONTEXTMENU, OnContextMenu)" not in dialog
assert "CHAIN_MSG_MAP_MEMBER(m_dctree)" in dialog
assert "MESSAGE_HANDLER(WM_CONTEXTMENU, OnContextMenu)" in tree
print("PASS: namespace-correct SQLite link, native cache policy and chained tree context menu.")

# Async exclusive metadata-write lease spans worker completion and optional artwork.
lease = (root / "discogger_async_write_lease.h").read_text(encoding="utf-8-sig")
assert "compare_exchange_strong" in lease and "std::shared_ptr<void>" in lease
assert "m_write_lease" in (root / "file_info_manager.h").read_text(encoding="utf-8-sig")
assert "metadb_io::update_info_success" in manager
assert "artwork_lease" in tasks
for path in ("scripts/run-msvc-unit-tests.ps1", ".github/workflows/validation.yml"):
    assert "async_write_lease_test.cpp" in (root / path).read_text(encoding="utf-8-sig")
print("PASS: async write/physical artwork operation lease contracts.")
# A successful async physical tag commit invalidates prior file_info snapshots
# before optional artwork, and even artwork-only writes revoke stale snapshots.
write_manager = (root / "file_info_manager.cpp").read_text(encoding="utf-8-sig")
write_callback = write_manager.split("void write_tags_completion::on_completion(unsigned p_code)", 1)[1].split(
    "void read_tags_completion::on_completion", 1
)[0]
assert write_callback.index("if (m_finfo_manager) m_finfo_manager->begin_physical_read();") < write_callback.index("if (m_after_success)")
assert "fully_requested(std::size_t submitted)" in (root / "discogger_write_safety.h").read_text(encoding="utf-8-sig")
assert "discogger_audit_failed_write(*m_write_payload, submitted)" in write_callback
assert "audit.fully_requested(submitted)" in write_callback
assert write_callback.index("audit.fully_requested(submitted)") < write_callback.index("action();")
# A success code with no configured artwork must still audit the complete
# physical batch and surface an actionable error for partial/unreadable writes.
success_callback = write_callback.split("m_gate.finish(success, [this]() {", 1)[1].split("}, [this]() {", 1)[0]
assert success_callback.index("discogger_audit_failed_write(*m_write_payload, submitted)") < success_callback.index("if (m_after_success)")
assert success_callback.index("audit.fully_requested(submitted)") < success_callback.index("if (m_after_success)")
assert "verification or dependent callback failed" in write_callback
artwork_only = write_manager.split("else if (after_success) {", 1)[1].split("\n    }", 1)[0]
assert artwork_only.index("begin_physical_read();") < artwork_only.index("after_success();")
print("PASS: successful metadata and artwork-only writes revoke stale physical snapshots.")


# Real artwork idempotence and atomic sidecar overwrite contracts.
artwork_source = (root / "foo_discogs.cpp").read_text(encoding="utf-8-sig")
sidecar = artwork_source[artwork_source.index("void foo_discogs::write_image("):artwork_source.index("void foo_discogs::embed_image(")]
embedded = artwork_source[artwork_source.index("void foo_discogs::embed_image("):artwork_source.index("void foo_discogs::ui_v2_config_callback")]
assert "artwork_stream_matches(" in sidecar
assert "rewrite_file(full_path.get_ptr()" in sidecar
assert "g_open_write_new" not in sidecar and "g_remove(" not in sidecar
assert "editor->query(embed_guid, p_abort)" in embedded
assert "artwork_bytes_equal(" in embedded
assert embedded.index("artwork_bytes_equal(") < embedded.index("editor->remove(embed_guid)")
assert embedded.index("artwork_bytes_equal(") < embedded.index("editor->commit(p_abort)")
for workflow_file in ("scripts/run-msvc-unit-tests.ps1", ".github/workflows/validation.yml"):
    assert "artwork_noop_test.cpp" in (root / workflow_file).read_text(encoding="utf-8-sig")
print("PASS: embedded/external artwork exact-byte no-op and staged atomic overwrite contracts.")

# Manual artwork button must share the tag/artwork operation gate and not write virtual cues.
art_tasks = (root / "tasks.cpp").read_text(encoding="utf-8-sig")
art_header = (root / "tasks.h").read_text(encoding="utf-8-sig")
part = art_tasks[art_tasks.index("void download_art_paths_task::safe_run"):art_tasks.index("void download_art_paths_task::on_success")]
assert "m_write_lease" in art_header
assert "acquire_physical_write_lease()" in art_tasks[art_tasks.index("download_art_paths_task::download_art_paths_task"):art_tasks.index("void download_art_paths_task::start")]
assert "if (!m_to_path_only)" in part and "if (!m_write_lease)" in part
assert "discogger_require_physical_targets(m_items);" in part
assert part.index("discogger_require_physical_targets(m_items);") < part.index("get_release(lkey")
assert part.count(", m_to_path_only, {}, bfile_match") == 4
assert ", false, {}, bfile_match" not in part
print("PASS: manual artwork operations acquire shared write lease, reject cue subsongs and honor preview-only mode.")

# Artwork previews must never create directories; failed/empty paths cannot
# trigger a write, including custom multi-artwork branches.
art_source = (root / "foo_discogs.cpp").read_text(encoding="utf-8-sig")
album_write = art_source[art_source.index("void foo_discogs::save_album_art("):art_source.index("void foo_discogs::save_artist_art(")]
artist_write = art_source[art_source.index("void foo_discogs::save_artist_art(pfc::array_t<Artist_ptr>&"):art_source.index("void foo_discogs::fetch_image(")]
for artwork_branch in (album_write, artist_write):
    assert artwork_branch.count("ensure_directory_exists(directory);") == 1
    assert artwork_branch.index("if (ada.to_path_only)") < artwork_branch.index("ensure_directory_exists(directory);")
    assert artwork_branch.index("ensure_directory_exists(directory);") < artwork_branch.index("g_discogs->write_image(buffer, path, p_abort);")
    assert "directory.get_length() != 0" in artwork_branch
    assert "bool write_this = vwrite_it[i] && ada.write_it;" in artwork_branch
    assert "Cannot write empty artwork data." in artwork_branch
chained_art = (root / "tasks.cpp").read_text(encoding="utf-8-sig")
chained_art = chained_art[chained_art.index("void download_art_task::safe_run"):chained_art.index("download_art_paths_task::download_art_paths_task")]
assert chained_art.index("discogger_require_physical_targets(items);") < chained_art.index("get_release(lkey")
print("PASS: deferred artwork directory creation, empty-path guards, preview-only and chained CUE safety.")

# Physical metadata deletion/cue mutation cannot silently bypass explicit approval.
preflight = (root / "tag_deletion_preflight.h").read_text(encoding="utf-8-sig")
manager = (root / "file_info_manager.cpp").read_text(encoding="utf-8-sig")
file_manager_h = (root / "file_info_manager.h").read_text(encoding="utf-8-sig")
tasks = (root / "tasks.cpp").read_text(encoding="utf-8-sig")
assert "cuesheet_modified" in preflight and 'meta_find("CUESHEET")' in preflight
assert "std::vector<bool> matched(next_count, false)" in preflight
assert "std::strcmp(old_text, new_text) == 0" in preflight
assert "result.removed_values += previous_count - next_count;" not in preflight
assert "if (previous_count <= 1)" in preflight
assert "removed_fields" in preflight and "removed_values" in preflight
assert "discard_pending_changes()" in file_manager_h and "original_info" in file_manager_h
# Failed callbacks revoke all physical snapshots BEFORE any fallible
# preview restoration, even when the first file_info assignment throws.
discard_body = file_manager_h.split("void discard_pending_changes() {", 1)[1].split(
    "inline void invalidate_all()", 1
)[0]
assert "discogger_write_safety::revoke_before_restore(" in discard_body
assert discard_body.index("begin_physical_read();") < discard_body.index("wrapper->info = wrapper->original_info;")
assert "if (wrapper->original_captured)" not in discard_body
assert "void revoke_before_restore(Revoke&& revoke, Restore&& restore)" in (
    root / "discogger_write_safety.h"
).read_text(encoding="utf-8-sig")

assert "tag_deletions_approved = false" in file_manager_h
assert 'if (!tag_deletions_approved && (report.removed_fields || report.removed_values))' in manager
assert 'MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2' in tasks
assert tasks.index("inspect_tag_deletions(") < tasks.index("write_infos(std::move(artwork_after_tags)")
for cfg in (".github/workflows/validation.yml", "scripts/run-msvc-unit-tests.ps1"):
    assert "tag_deletion_preflight_test.cpp" in (root / cfg).read_text(encoding="utf-8-sig")
print("PASS: explicit physical-tag removal approval, protected embedded CUE and restore-on-cancel.")

# Prevent accidental zero-length approval scans; avoid duplicate expensive CI.
approval_writer = (root / "tag_writer.cpp").read_text(encoding="utf-8-sig")
approval_helper = (root / "tag_deletion_preflight.h").read_text(encoding="utf-8-sig")
assert "has_any_user_approval(wr->r_usr_approved)" in approval_writer
assert "r_usr_approved.size() < wr->r_usr_approved.size()" not in approval_writer
assert "approvals.find_first(true, 0, count) < count" in approval_helper
workflow_content = (root / ".github/workflows/validation.yml").read_text(encoding="utf-8-sig")
assert "  pull_request:" in workflow_content and "  push:" in workflow_content
assert "      - master" in workflow_content and "      - 'work/**'" not in workflow_content
print("PASS: user staging approvals and one validation run per draft PR SHA.")

# Async callback lifetime and failure cleanup: source-level guard, not host acceptance.
manager_header = (root / "file_info_manager.h").read_text(encoding="utf-8-sig")
manager = (root / "file_info_manager.cpp").read_text(encoding="utf-8-sig")
safety_header = (root / "discogger_write_safety.h").read_text(encoding="utf-8-sig")
assert "std::enable_shared_from_this<file_info_manager>" in manager_header
assert "std::shared_ptr<file_info_manager> m_finfo_manager" in manager_header
assert "auto keep_alive = shared_from_this();" in manager
assert "std::move(keep_alive)" in manager
assert "if (m_gate.completed()) return;" in manager
assert "m_finfo_manager->discard_pending_changes();" in manager
assert "discard_pending_changes();\n            throw;" in manager
assert "FailureAction&& failure_action" in safety_header
assert "discogger_audit_failed_write(*m_write_payload)" in manager
assert "payload->original_infos[outptr] = payload->original_copies.append_copy(wrapper->original_info);" in manager
assert "payload->submitted_count = outptr;" in manager
assert "read_limit = 16" in manager_header
assert "No rollback or retry was attempted." in manager
# Failed SDK completion must still audit physical files if preview cleanup throws.
# A premature return in either catch used to suppress partial-write warnings.
callback = manager.split("void write_tags_completion::on_completion(unsigned p_code) {", 1)[1].split(
    "void read_tags_completion::on_completion", 1)[0]
assert "std::string callback_cleanup_error;" in callback
assert "Callback cleanup also failed:" in callback
assert "Physical tag commit reported success, but verification or dependent callback failed:" in callback
cleanup_catches = callback.split("catch (const std::exception& e)", 1)[1].split("if (!success) {", 1)[0]
assert "return;" not in cleanup_catches
assert callback.index("if (!success) {") < callback.index("discogger_audit_failed_write(*m_write_payload)")
qa_partial = (root / "tests/native-media-writer-qa.inc").read_text(encoding="utf-8-sig")
host_partial = (root / "scripts/host-discogger-gui.ps1").read_text(encoding="utf-8-sig")
assert "run_disposable_partial_write_audit_qa()" in qa_partial
assert "discogger_audit_failed_write(payload)" in qa_partial
assert "manager->discard_pending_changes();" in qa_partial
assert "DISCOGGER_PHYSICAL_INJECTED_PARTIAL_READBACK_" in host_partial
assert "record_disposable_partial_qa_error" in qa_partial
assert "qa-injected-partial-error.txt" in host_partial
assert "failed_write_file_state" in safety_header
print("PASS: callback-owned metadata lifetime, single-shot async failure reset and synchronous scheduling cleanup.")

# Safe disk-marker TTL, independent artist freshness and legacy config migration.
cache = (root / "ol_cache.h").read_text(encoding="utf-8-sig")
config = (root / "configuration_dialog.cpp").read_text(encoding="utf-8-sig")
conf_code = (root / "conf.cpp").read_text(encoding="utf-8-sig")
assert "fs::last_write_time(complete, ec)" in cache
assert "check_download(out_relative_path, expired, gfFrom)" in cache
# The offline reader must reject a stamped but incomplete page directory,
# including the no-rollback case where recovery correctly leaves it in place.
read_gate = cache.split("bool static check_download(", 1)[1].split(
    "static pfc::string8 staged_cache_json_path(", 1)[0]
assert "discogger_cache::cache_has_marker(root)" in read_gate
assert read_gate.index("discogger_cache::cache_has_marker(root)") < read_gate.index(
    "fs::last_write_time(complete, ec)")
assert "discogger_cache::ttl(" in cache
assert "CACHE_EXPIRATION" not in cache and "CACHE_EXPIRATION" not in config
assert "_read(jf, buffer, src_length_checked)" not in cache
assert "IDC_EDIT_CFG_ARTIST_CACHE_HOURS" in config
assert "IDC_EDIT_CFG_ARTIST_CACHE_HOURS" in (root / "foo_discogger.rc").read_text(encoding="utf-8-sig")
assert "v215short{ &vec_specs, 27, 54, 20 }" in conf_code
assert "v216{ &vec_specs, 27, 56, 20 }" in conf_code
assert "if (vLoad == v215short)" in conf_code
save_ints = conf_code[conf_code.index("std::vector<conf_int_entry> int_snapshot"):conf_code.index("cfg_int_entries.set(std::move(int_snapshot))")]
assert save_ints.count("int_snapshot.emplace_back(make_conf_entry(") == 56
assert "CFG_DISK_CACHE_EXP, disk_cache_exp" in save_ints
assert "CFG_ARTIST_RELEASE_CACHE_HOURS" in save_ints
cache_policy = (root / "discogger_cache_expiry.h").read_text(encoding="utf-8-sig")
assert cache_policy.count("(std::chrono::seconds::max)()") == 2
assert "std::chrono::seconds::max()" not in cache_policy
assert "#define max(a, b) forbidden_legacy_windows_max_macro" in (root / "tests/cache_expiry_test.cpp").read_text(encoding="utf-8-sig")
assert "discogger_cache::artist_hours(item.value)" in conf_code
for name in (".github/workflows/validation.yml", "scripts/run-msvc-unit-tests.ps1"):
    assert "cache_expiry_test.cpp" in (root / name).read_text(encoding="utf-8-sig")
print("PASS: safe cache-marker TTL, independent artist hours and configuration migration.")

# Offline JSON cache: no overwritten placeholder refs, leaked json_dumps buffers,
# or stale artist/release JSON on failed or empty cache reads.
offline_cache_source = (root / "ol_cache.h").read_text(encoding="utf-8-sig")
iface_source = (root / "discogs_interface.cpp").read_text(encoding="utf-8-sig")
cache_read = offline_cache_source[offline_cache_source.index("json_t* Read_JSON("):offline_cache_source.index("// serves offline_cache_save")]
assert "json_t* root = load_cache_json(os_ol, &json_error);" in cache_read
assert "json_t* root = json_array();" not in cache_read
assert "return root ? root : json_array();" in cache_read
offline_entity = iface_source[iface_source.index("void DiscogsInterface::get_entity_offline_cache("):iface_source.index("pfc::string8 DiscogsInterface::get_username(")]
assert 'html = "";' in offline_entity
assert "std::unique_ptr<json_t, decltype(json_cleanup)> js_obj(" in offline_entity
assert "json_decref(ptr)" in offline_entity
assert "json_is_object(js_obj.get())" in offline_entity
assert "json_object_size(js_obj.get()) == 0" in offline_entity
assert "std::unique_ptr<char, decltype(text_cleanup)> serialized(" in offline_entity
assert "std::free(ptr)" in offline_entity
assert "if (serialized) html = serialized.get();" in offline_entity
assert "pfc::string8(json_dumps(" not in offline_entity
print("PASS: offline cache JSON references and serialized buffers are released; invalid results clear stale output.")

# Live Discogs pagination must read each parser before moving it, reject changed
# cursors, and propagate cancellation rather than returning partial results.
online = (root / "discogs_interface.cpp").read_text(encoding="utf-8-sig")
start = online.index("pfc::array_t<JSONParser_ptr> DiscogsInterface::get_all_pages(")
end = online.index("namespace fs =std::filesystem;", start)
pager = online[start:end]
assert pager.count("results.append_single(std::move(jp));") == 2
assert pager.count("discogger_cache::valid_online_page(") == 2
assert pager.count("p_abort.check();") >= 6
assert pager.count("const size_t returned_page = jp->get_object_int") == 2
for fn in pager.split("pfc::array_t<JSONParser_ptr> DiscogsInterface::get_all_pages(")[1:]:
    if "results.append_single(std::move(jp));" in fn:
        assert fn.index('get_object_int("pagination", "pages")') < fn.index("results.append_single(std::move(jp));")
        assert fn.index("discogger_cache::valid_online_page(") < fn.index("results.append_single(std::move(jp));")
assert "std::free(ptr)" in online
assert "json_free(ptr)" not in online
assert "#include <cstdlib>" in online
assert "valid_online_page(" in (root / "discogger_cache_pages.h").read_text(encoding="utf-8-sig")
print("PASS: online cursor bounds, moved-parser lifetime, abort safety and Jansson free contract.")

# Expensive SDK/whole-DLL builds must not run after a known cheap regression
# failure, avoiding wasted GitHub Actions time on non-release PRs.
wf_gate = (root / ".github/workflows/validation.yml").read_text(encoding="utf-8-sig")
assert "  official-sdk-probe:\n    # Fail cheap/static and native unit gates before launching the costly SDK" in wf_gate
assert "    needs: [portable-core, windows-baseline, windows-native-unit]" in wf_gate
assert "    runs-on: windows-2022" in wf_gate
print("PASS: strict DLL gate depends on portable, Windows source and x86/x64 unit test successes.")

# JSON cache replacement must not truncate a valid destination on failure.
atomic_header = (root / "discogger_atomic_cache_file.h").read_text(encoding="utf-8-sig")
assert "MoveFileExW(" in atomic_header
assert "MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH" in atomic_header
assert "if (!written)" in atomic_header and "fs::remove(temporary, ec)" in atomic_header
cache_atomic = (root / "ol_cache.h").read_text(encoding="utf-8-sig")
assert '#include "discogger_atomic_cache_file.h"' in cache_atomic
assert "discogger_cache::atomic_cache_replace(destination," in cache_atomic
assert "_O_CREAT | _O_EXCL | _O_WRONLY | _O_BINARY" in cache_atomic
assert "const bool synced = serialized && _commit(jf) == 0;" in cache_atomic
assert "fs::remove_all(os_ol.remove_filename())" not in cache_atomic
assert "json_decref(parsed);" in cache_atomic
assert "atomic_cache_file_test.cpp" in (root / "scripts/run-msvc-unit-tests.ps1").read_text(encoding="utf-8-sig")
print("PASS: atomic JSON cache publication and non-destructive parse failure.")


# Full cache directory publication uses isolated staging, a completed marker,
# two-phase rename, recovery, and negative portable/MSVC tests.
txn = (root / "discogger_directory_cache.h").read_text(encoding="utf-8-sig")
assert "recover_cache_unlocked(active_, validator_)" in txn
# Recovery is only allowed to retire a valid rollback after a real JSON
# decoder has checked all entity or paginated payloads, not mere nonempty files.
assert "if (!validate || !cache_has_marker(backup, validate)) return false;" in txn
assert "cache_has_marker(active, validate)" in txn
assert 'cache_payload_file(it->path() / "root.json", validate)' in txn
assert "cache_payload_file(single, validate)" in txn
# The native CRT descriptor must not cross the Jansson static-library boundary.
# Recovery validation and ordinary cache reads share bounded JSON byte parsing.
assert "json_loadb(bytes.data(), bytes.size(), JSON_DECODE_ANY, error)" in cache_atomic
assert "max_cache_json_bytes = 128ull * 1024ull * 1024ull" in cache_atomic
assert "json_t* root = load_cache_json(os_ol, &json_error);" in cache_atomic
assert "json_loadfd(" not in cache_atomic
assert "json_dumpfd(" not in cache_atomic
assert "json_dump_callback(root," in cache_atomic
assert "const int written = _write(fd, data, chunk);" in cache_atomic
assert "cache_payload_is_valid_json" in cache_atomic
assert "recover_cache_directory(fs::u8path(out_relative_path.c_str())," in cache_atomic
assert "assert(!discogger_cache::recover_cache_directory(malformed));" in (
    root / "tests/directory_cache_transaction_test.cpp").read_text(encoding="utf-8-sig")
assert "fs::rename(active_, backup_, ec);" in txn
assert "fs::rename(stage_, active_, ec);" in txn
assert "fs::rename(backup_, active_, restore_ec);" in txn
assert "cache_has_marker(stage_, validator_)" in txn
assert "cache_json_validator validator_;" in txn
assert "validator_(std::move(validate))" in txn
assert "std::unique_lock<std::mutex> lock_" in txn
assert "directory_cache_transaction_test.cpp" in (root / "scripts/run-msvc-unit-tests.ps1").read_text(encoding="utf-8-sig")
# Original Win32/x64 host QA must execute Jansson itself on disposable JSON
# fixtures, not rely only on a portable synthetic validator callback.
native_dlg = (root / "find_release_dialog.cpp").read_text(encoding="utf-8-sig")
assert "runner_qa_native_cache_json_rollback();" in native_dlg
assert "DISCOGGER_NATIVE_JSON_ROLLBACK_QA=PASS" in native_dlg
assert 'write(payload, R"({"broken":)");' in native_dlg
assert "recover_cache_directory(active, ol::cache_payload_is_valid_json)" in native_dlg
assert "Native Jansson published malformed staged JSON." in native_dlg
assert "Native Jansson rejected valid staged JSON." in native_dlg
assert "Native offline cache reader rejected valid JSON." in native_dlg
assert "Native offline reader accepted truncated JSON." in native_dlg
assert "Native atomic JSON serializer did not roundtrip." in native_dlg
assert "real_reader.Dump_JSON(serialized_utf8.c_str(), generated)" in native_dlg
assert "Native malformed staged JSON changed valid active cache." in native_dlg
assert "qa-populated-gui-error.txt" in (root / "tests/native-media-writer-qa.inc").read_text(encoding="utf-8-sig")
assert "Native populated foobar GUI assertion failed:" in (root / "scripts/host-discogger-gui.ps1").read_text(encoding="utf-8-sig")

core = (root / "discogs.cpp").read_text(encoding="utf-8-sig")
assert core.count("ol::cache_payload_is_valid_json);") == 4
assert core.count("discogger_cache::directory_cache_transaction tx(") == 4
assert core.count("ol::staged_cache_json_path(tx") == 4
assert "ol::stamp_download(" not in core
assert "fs::remove_all(os_file)" not in offline_cache_source
assert "discogger_cache::recover_cache_directory(" in offline_cache_source
print("PASS: full cache directory transaction, rollback and recovery contracts.")


# The dialog must not compute the list filter before unchecking a stale
# exact-only preference when it has only alternative candidates.
visibility = (root / "discogger_search_visibility.h").read_text(encoding="utf-8-sig")
searchdlg = (root / "find_release_dialog.cpp").read_text(encoding="utf-8-sig")
frag = searchdlg[searchdlg.index("void CFindReleaseDialog::on_search_artist_done("):
                 searchdlg.index("void CFindReleaseDialog::OnTypeFilterTimer(")]
assert "resolve_result_filter(" in visibility
assert "discogger_search::resolve_result_filter(" in frag
assert "const bool b_chk_exact_matches = decision.exact_only;" in frag
assert "CheckDlgButton(IDC_CHK_ONLY_EXACT_MATCHES, BST_UNCHECKED);" in frag
assert "alternative releases require review" in frag
assert "m_alist.fill_artist_list(b_chk_exact_matches" in frag
assert "const size_t incoming_exact = valid_count(valid_exact);" in frag
assert "const size_t incoming_other = valid_count(valid_other);" in frag
# Invalid allocated artist IDs must be discarded before exact-only ranking,
# cumulative append and native list/tree presentation. The runner exercises
# empty-ID exact fallback plus an empty-ID late append against real Win32 UI.
assert "const auto valid_rows =" in frag
assert "if (artist && artist->id.get_length())" in frag
assert "const auto valid_exact = valid_rows(p_artist_exact_matches);" in frag
assert "const auto valid_other = valid_rows(p_artist_other_matches);" in frag
assert frag.count("m_alist.set_artists(b_chk_exact_matches, append, nullptr, valid_exact, valid_other);") == 2
assert 'empty_id_exact.append_single(std::make_shared<Artist>(""));' in searchdlg
assert "Native empty-ID exact artist hid valid alternatives." in searchdlg
assert "Native empty-ID append polluted rows or preview." in searchdlg
assert "const size_t total_exact = incoming_exact +" in frag
assert "const size_t total_other = incoming_other +" in frag
assert "const bool incoming_results = incoming_exact || incoming_other;" in frag
assert "const bool incoming_results = p_artist_exact_matches.get_count()" not in frag
assert "return count_valid_artists(m_artist_exact_matches);" in artist_header
assert "return count_valid_artists(m_artist_other_matches);" in artist_header
assert "append ? m_alist.Get_Exact_Artist_Count() : 0" in frag
assert "append ? m_alist.Get_Other_Artist_Count() : 0" in frag
assert "exact_only_requested, total_exact, total_other" in frag
assert "const bool no_matches = !(total_exact || total_other);" in frag
assert "const bool incoming_results =" in frag
assert "(!append || (incoming_results && !m_alist.Get_Selected_Id().get_length()))" in frag
assert "if (update_preview)" in frag
assert "const bool no_matches = !(p_artist_exact_matches.get_count()" not in frag
assert "search_visibility_test.cpp" in (root / "scripts/run-msvc-unit-tests.ps1").read_text(encoding="utf-8-sig")
assert "search_visibility_test.cpp" in wf_gate
print("PASS: alternative search candidates remain visible with matched checkbox state.")


# Double-click on an actual artist list row routes through the same
# release-load action as Enter, never from blank/stale selections.
uih = (root / "find_release_dialog.h").read_text(encoding="utf-8-sig")
uic = (root / "find_release_dialog.cpp").read_text(encoding="utf-8-sig")
artist_list = (root / "find_artist_list.cpp").read_text(encoding="utf-8-sig")
test_script = (root / "scripts/run-msvc-unit-tests.ps1").read_text(encoding="utf-8-sig")
validate = (root / ".github/workflows/validation.yml").read_text(encoding="utf-8-sig")
assert "NOTIFY_HANDLER(IDC_ARTIST_LIST, NM_DBLCLK, OnArtistListDoubleClick)" in uih
assert "LRESULT CFindReleaseDialog::OnArtistListDoubleClick(" in uic
assert "hdr->hwndFrom != m_artist_list" in uic
assert "discogger_ui::should_load_artist_on_double_click(" in uic
assert "m_alist.Default_Action();" in uic[uic.index("LRESULT CFindReleaseDialog::OnArtistListDoubleClick("):
                                       uic.index("//from Enter key hook")]
assert "pos == ~0 || pos >= m_find_release_artists.get_count()" in artist_list
assert "artist_activation_test.cpp" in test_script
assert "artist_activation_test.cpp" in validate
print("PASS: row-only artist double-click wired to guarded Enter/default action.")


# Every direct-ID path enters trigger_release; that boundary must reject
# partial, zero, negative and signed-int-overflow numbers before any callback.
ui = (root / "find_release_dialog.cpp").read_text(encoding="utf-8-sig")
trigger = ui[ui.index("void CFindReleaseDialog::trigger_release("):
             ui.index("void CFindReleaseDialog::", ui.index("void CFindReleaseDialog::trigger_release(") + 1)]
assert "discogger_search::valid_release_id(release_id.c_str())" in trigger
assert "std::atoi(release_id)" not in trigger
assert "release_id_test.cpp" in (root / "scripts/run-msvc-unit-tests.ps1").read_text(encoding="utf-8-sig")
assert "release_id_test.cpp" in (root / ".github/workflows/validation.yml").read_text(encoding="utf-8-sig")
print("PASS: direct release IDs validated before preview/load callbacks.")

# Thumbnail cache reads are untrusted: no unchecked artist index, no signed
# CRT read conversion, no partial/oversized buffers handed to the preview UI.
thumbnail = (root / "discogs_interface.cpp").read_text(encoding="utf-8-sig")
thumbnail_header = (root / "discogs_interface.h").read_text(encoding="utf-8-sig")
thumbnail_func = thumbnail[thumbnail.index("bool DiscogsInterface::get_thumbnail_from_cache("):
                           thumbnail.index("bool DiscogsInterface::delete_artist_cache(")]
assert "discogger_thumbnail::consume_artist_images(count, ndx)" in thumbnail_header
assert "if (!wra || !wra->full_artist) continue;" in thumbnail_header
assert "if (!img_artists_ndx_to_artist(release, img_ndx, artist, local_ndx)" in thumbnail_func
assert "discogger_thumbnail::valid_cache_size(bytes)" in thumbnail_func
assert "discogger_thumbnail::complete_read(input.gcount(), size)" in thumbnail_func
assert "small_art.set_size(0);" in thumbnail_func
assert "_read(jf," not in thumbnail_func
assert "_wopen(" not in thumbnail_func
assert "thumbnail_bounds_test.cpp" in (root / "scripts/run-msvc-unit-tests.ps1").read_text(encoding="utf-8-sig")
assert "thumbnail_bounds_test.cpp" in (root / ".github/workflows/validation.yml").read_text(encoding="utf-8-sig")
print("PASS: cached artwork preview rejects invalid indices, oversized and partial files.")


# Shared CI host smoke must actually launch official x86/x64 foobar hosts
# and inspect the exact built component in the same ephemeral Windows job.
# A source-wiring check is not itself live-host acceptance.
host_wf = (root / ".github/workflows/validation.yml").read_text(encoding="utf-8-sig")
shared_action = "lxsdd/dev-infrastructure-public/.github/actions/foobar-host-smoke@35369054c56f4c6ccb2f802944e9675852fd35ec"
# Two QA host loads plus two clean non-QA host loads, both architectures.
assert host_wf.count("uses: " + shared_action) == 4
assert host_wf.index("run: ./scripts/build-component-dll.ps1") < host_wf.index("HARD GATE - real foobar2000 Win32 component host load")
assert host_wf.index("HARD GATE - real foobar2000 Win32 component host load") < host_wf.index("HARD GATE - real foobar2000 x64 component host load")
assert "architecture: Win32" in host_wf and "architecture: x64" in host_wf
assert "foo_discogger/Win32/Release/foo_discogger.dll" in host_wf
assert "foo_discogger/x64/Release/foo_discogger.dll" in host_wf
assert "actions/upload-artifact" not in host_wf
print("PASS: exact-SHA shared foobar Win32/x64 component host smoke is fail-closed after native linking.")

# Stale Windows tree display callbacks must fail closed for evicted/empty
# rows; partial cached releases must not dereference absent disc pointers.
tree_source = (root / "find_release_tree.cpp").read_text(encoding="utf-8-sig")
tree_header = (root / "find_release_tree.h").read_text(encoding="utf-8-sig")
tree_frag = tree_source[tree_source.index("LRESULT CFindReleaseTree::OnReleaseTreeGetInfo("):
                        tree_source.index("// ..", tree_source.index("LRESULT CFindReleaseTree::OnReleaseTreeGetInfo("))]
assert '#include "discogger_tree_snapshot.h"' in tree_source
assert "hdr == nullptr || hdr->hwndFrom != m_hwndTreeView" in tree_frag
assert "cit == cache_ptr->cache.end() || cit->second.first.col_data_list.empty()" in tree_frag
assert tree_frag.index("cit == cache_ptr->cache.end()") < tree_frag.index("if (pItem->mask & TVIF_TEXT)")
assert tree_frag.count("cache_ptr->cache.find(lparam)") == 1
assert tree_source.count("discogger_tree::count_cached_tracks(release)") == 2
tree_hooks = tree_source[tree_source.index("pfc::string8 CFindReleaseTree::run_hook_columns("):
                         tree_source.index("void CFindReleaseTree::rebuild_treeview()")]
assert "discogger_tree::valid_row_indices(kind" in tree_hooks
assert "if (!m_find_release_artist) return unavailable();" in tree_hooks
assert "if (!master) return unavailable();" in tree_hooks
assert "if (kind == discogger_tree::row_kind::subrelease &&" in tree_hooks
assert "if (kind == discogger_tree::row_kind::standalone &&" in tree_hooks
assert "main_release->id" not in tree_hooks
assert "row_data.id = -1;" in tree_hooks
assert "row_data.col_data_list.emplace_back(0, label);" in tree_hooks
master_selected_frag = tree_source[tree_source.index("void CFindReleaseTree::on_release_selected("):
                                   tree_source.index("LRESULT CFindReleaseTree::apply_filter(")]
assert "myparam.master_ndx >= m_find_release_artist->master_releases.get_count()" in master_selected_frag
assert "MasterRelease_ptr master_release = m_find_release_artist->master_releases[myparam.master_ndx];" in master_selected_frag
assert "m_dlg->call_expand_master_service(master_release, selection_index);" in master_selected_frag
autofill_frag = tree_source[tree_source.index("size_t CFindReleaseTree::get_autofill_release_id("):
                             tree_source.index("size_t CFindReleaseTree::test_getselected(")]
assert "return get_param_id(myparam).get_length() ? some_id" in autofill_frag
assert "if (!master) return ~size_t(0);" in autofill_frag
assert "if (child && is_number(child->id.c_str())" in autofill_frag
assert "master->sub_releases[j]->id" not in autofill_frag
selection_changed_frag = tree_source[tree_source.index("LRESULT CFindReleaseTree::OnReleaseTreeSelChanged("):
                                      tree_source.index("size_t CFindReleaseTree::get_autofill_release_id(")]
assert "!hdr || hdr->hwndFrom != m_hwndTreeView" in selection_changed_frag
assert "if (!TreeView_GetItem(m_hwndTreeView, &pItemMaster)) return FALSE;" in selection_changed_frag

tree_menu = tree_source[tree_source.index("void CFindReleaseTree::context_menu("):
                        tree_source.index("//public serves find-rel dlg")]
assert "Artist_ptr artist_snapshot = m_find_release_artist;" in tree_menu
assert "MasterRelease_ptr master_snapshot;" in tree_menu
assert "Release_ptr release_snapshot;" in tree_menu
assert "myparam.master_ndx < artist_snapshot->master_releases.get_count()" in tree_menu
assert "myparam.release_ndx < master_snapshot->sub_releases.get_count()" in tree_menu
assert "myparam.release_ndx < artist_snapshot->releases.get_count()" in tree_menu
assert "const bool empty_sel =" in tree_menu
assert "context_title = release_snapshot->title;" in tree_menu
assert "if (empty_sel || !context_id.get_length()) break;" in tree_menu
assert "if (empty_sel ||" in tree_menu
assert "delete_artist_cache(artist_id, release_id)" in tree_menu
assert "m_find_release_artist->" not in tree_menu

tree_id = tree_source[tree_source.index("pfc::string8 CFindReleaseTree::get_param_id("):
                      tree_source.index("int release_tree_cache::get_src_param(", tree_source.index("pfc::string8 CFindReleaseTree::get_param_id("))]
assert "if (!m_find_release_artist) return id;" in tree_id
assert "discogger_tree::valid_row_indices(kind" in tree_id
assert "if (!master) return id;" in tree_id
assert "return child ? child->id : id;" in tree_id
assert "return release ? release->id : id;" in tree_id
assert "MasterRelease_ptr dbugmasrel" not in tree_id
assert tree_source.count("if (release_id.get_length())") >= 2
assert "myparam.master_ndx >= m_find_release_artist->master_releases.get_count()" in tree_source
assert "test_getatcursor(cursor, item, true)" in tree_source
assert "(hit_info.flags & TVHT_ONITEM)" in tree_source
assert "bool require_item_body = false" in tree_header
assert "release_tree_snapshot_test.cpp" in (root / "scripts/run-msvc-unit-tests.ps1").read_text(encoding="utf-8-sig")
assert "release_tree_snapshot_test.cpp" in (root / ".github/workflows/validation.yml").read_text(encoding="utf-8-sig")
print("PASS: stale release-tree callback, partial cached release and row hit-test guards.")


# Discogger's native Find Releases window opens from a context menu;
# no release apply/write command may be invoked by the disposable host probe.
# The shared pinned action checks a live HWND, required Win32 control IDs,
# modeless reuse and unchanged synthetic WAV/CUE after launcher dispatch.
resource = (root / "resource.h").read_text(encoding="utf-8-sig")
dialogs = (root / "foo_discogger.rc").read_text(encoding="utf-8-sig")
menu = (root / "mainmenu.cpp").read_text(encoding="utf-8-sig")
for control, value in {
    "IDC_EDIT_SEARCH":1083, "IDC_BTN_SEARCH":1086,
    "IDC_ARTIST_LIST":1080, "IDC_RELEASE_TREE":1018,
    "IDC_EDIT_FILTER":1099, "IDC_CHK_ONLY_EXACT_MATCHES":1107,
    "IDC_RELEASE_URL_TEXT":1092, "IDC_BTN_PROCESS_RELEASE":1198
}.items():
    assert f"#define {control}" in resource
    assert any(line.strip().startswith(f"#define {control} ") and
               line.split()[-1] == str(value) for line in resource.splitlines())
    assert control in dialogs[dialogs.index("IDD_DIALOG_FIND_RELEASE DIALOGEX"):
                              dialogs.index("IDD_DIALOG_FIND_RELEASE_MINI DIALOGEX")]
assert 'CAPTION "Find Releases"' in dialogs
assert 'p_out = "Write tags...";' in menu
ctx = menu[menu.index("case WriteTags:\n\t\tcase WriteTagsAlt: {"):
           menu.index("case EditTagMappings:", menu.index("case WriteTags:\n\t\tcase WriteTagsAlt: {"))]
assert "fb2k::newDialog<CFindReleaseDialog>" in ctx
for forbidden in ("metadb_io", "file_info_writer", "write_tags(", "commit_tags(", "save_cue("):
    assert forbidden not in ctx
# The plain shared smoke may only assert DLL load + disposable WAV/CUE.
# Discogger additionally needs its original SQLite configuration data file
# beside a user-components installation, not a fabricated empty schema.
assert not "context-command:" in host_wf
assert host_wf.count("scripts/host-discogger-gui.ps1 -Architecture ") == 2
assert "host-discogger-gui.ps1" in host_wf.split("Parse strict PowerShell DLL build script")[1].split("Test legacy resource image formats")[0]
gui_smoke = (root / "scripts/host-discogger-gui.ps1").read_text(encoding="utf-8-sig")
assert (root / "foo_discogger.dll.db").read_bytes().startswith(b"SQLite format 3\x00")
assert "foo_discogger.dll.db" in gui_smoke
# Full native GUI reuses only the byte-identical installer that the pinned
# shared host-smoke downloaded in the same ephemeral Windows job.
assert "foobar2000-verified-installer-v1" in gui_smoke
assert "$Architecture-2.26.1-$($spec.Hash.ToLowerInvariant()).exe" in gui_smoke
assert "Test-Path -LiteralPath $installer -PathType Leaf" in gui_smoke
assert "(Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash -ine $spec.Hash" in gui_smoke
assert "DISCOGGER_GUI_INSTALLER_CACHE_MISSING_" in gui_smoke
assert "DISCOGGER_GUI_INSTALLER_HASH_MISMATCH_" in gui_smoke
assert "DISCOGGER_GUI_INSTALLER_SOURCE=VERIFIED_JOB_LOCAL_CACHE" in gui_smoke
assert "Invoke-WebRequest -Uri $spec.Url -OutFile $installer" not in gui_smoke
shared_gui_flow = host_wf[host_wf.index("name: HARD GATE - real foobar2000 Win32 component host load"):]
assert shared_gui_flow.index("name: HARD GATE - real foobar2000 Win32 component host load") < shared_gui_flow.index("name: HARD GATE - real foobar2000 x64 component host load") < shared_gui_flow.index("name: HARD GATE - Discogger first-run SQLite configuration and Win32 native GUI") < shared_gui_flow.index("name: HARD GATE - Discogger first-run SQLite configuration and x64 native GUI")
assert "DISCOGGER_INITIALIZATION_$Architecture=PASS" in gui_smoke
assert "DISCOGGER_INIT_DIAGNOSTIC=no_configuration_database" in gui_smoke
assert "DISCOGGER_FIRST_RUN_APPEARANCE=DISMISS_IN_TEST_ONLY" in gui_smoke
assert "DISCOGGER_RUNNING_PROCESS_IDS" in gui_smoke
assert "DISCOGGER_ALL_VISIBLE_WINDOWS" in gui_smoke
assert "DISCOGGER_CONTEXT_ROUTE=" in gui_smoke
assert "DISCOGGER_CRASH_AFTER_CONTEXT_ROUTE=" in gui_smoke
assert "DISCOGGER_CRASH_EVENT_ID=" in gui_smoke
assert "DISCOGGER_CRASH_REPORTS_FOUND=" in gui_smoke
assert "Write-DiscoggerCrashEvidence" in gui_smoke
assert "foobar2000 has crashed" in gui_smoke
assert "Real foobar2000 crashed during the first matching tagging context dispatch." in gui_smoke
assert "Get-Process -Name foobar2000" in gui_smoke
assert "The actual foobar host exited after context route" in gui_smoke
assert "foo_discogger.dll.db" in gui_smoke
assert "DISCOGGER_GUI_DIALOG_$Architecture=PASS" in gui_smoke
assert "DISCOGGER_GUI_CONTROLS_$Architecture=PASS" in gui_smoke
# Restoring original values from a multi-track detail preview must never
# treat the address of the *whole PFC array* as an array of array objects.
preview_ilo = (root / "preview_leading_tag_ILO.cpp").read_text(encoding="utf-8-sig")
assert '#include "discogger_preview_restore.h"' in preview_ilo
copy_restore = preview_ilo[preview_ilo.index("void ILOD_preview_leading::CopyFromOriginal()"):
                           preview_ilo.index("void ILOD_preview_leading::TriggerAction()")]
assert "discogger_preview::restore_original_values(" in copy_restore
assert "m_ptag_result->value, m_ptag_result->old_value, visible_tracks" in copy_restore
assert "arr_sea_val[w] = cmp_old_sea_val[w]" not in copy_restore
assert "safe_value_index(" in (root / "discogger_preview_restore.h").read_text(encoding="utf-8-sig")
assert preview_ilo.count("discogger_preview::safe_value_index(") >= 4
assert "item >= ilo_get_vtracks_desc().size()" in preview_ilo
assert "if (!m_ptag_result || !val || subItem != kDataCol) return;" in preview_ilo
assert "pfc::array_t<string_encoded_array> expanded;" in preview_ilo
assert "values = std::move(expanded);" in preview_ilo
assert "string_encoded_array* value = &(m_ptag_result->value[0]);" not in preview_ilo
assert "UpdateItemsAll();" in copy_restore
for runner_path in ("scripts/run-msvc-unit-tests.ps1", ".github/workflows/validation.yml"):
    assert "preview_restore_test.cpp" in (root / runner_path).read_text(encoding="utf-8-sig")
assert "DISCOGGER_GUI_SYNTHETIC_WAV_NO_WRITE_$Architecture=PASS" in gui_smoke
assert "MEDIA_WRITE_SAFETY_$Architecture=NOT_TESTED" in gui_smoke
assert "GetDlgItem" in gui_smoke and "FindReleaseDialogs" in gui_smoke
assert "FindReleaseDialogs([uint32]$proc.Id)" in gui_smoke
assert "GetDlgItem(h,id)==IntPtr.Zero" in gui_smoke
assert "Find([uint32]$proc.Id,'Find Releases')" not in gui_smoke
# Context menu must refuse zero selected handles; the dialog reads m_items[0].
menu_text = (root / "mainmenu.cpp").read_text(encoding="utf-8-sig")
assert "if (p_data.get_count() == 0) return;" in menu_text
assert "p_data.get_count() != 0 &&" in menu_text
assert "metadb_handle_ptr item = m_items[0];" in (root / "find_release_dialog.cpp").read_text(encoding="utf-8-sig")
assert 'bk_frm_album.get_ptr() : "Find releases"' in (root / "find_release_dialog.cpp").read_text(encoding="utf-8-sig")
assert "Start-Process" in gui_smoke and "Write tags..." in gui_smoke
assert "Test-Path -LiteralPath $dir" in gui_smoke
for unsafe in ("-ExecutionPolicy Bypass", "actions/upload-artifact", "media_file_write", "CommitTags("):
    assert unsafe not in gui_smoke
print("PASS: original Discogger SQLite first-run fixture and independent fail-closed x86/x64 native GUI gates.")


# Real host crash regression: absent PNG resources must not be dereferenced.
icon_source = (root / "icon_map.cpp").read_text(encoding="utf-8-sig")
assert icon_source.count("if (!rec_image.Load(") == 2
assert icon_source.count("!rec_image.m_pBitmap) return nullptr;") == 2
assert icon_source.count("mapped->second.empty()") == 2
assert "HBITMAP h_bitmap = nullptr;" in icon_source
assert "DeleteObject(rec_image);" not in icon_source
assert "rec_image.Empty();" in icon_source
print("PASS: GDI+ icon loader rejects missing resources and null bitmaps before use.")


# Real HWND/DPI-sensitive geometry: prove search field/button alignment, no
# overlap/clipping of major controls and a non-disabled search button in both
# official host architectures, without synthetic media mutation.
gui_source = (root / "scripts/host-discogger-gui.ps1").read_text(encoding="utf-8-sig")
assert "CheckFindReleasesGeometry(IntPtr dialog)" in gui_source
assert "GetWindowRect" in gui_source and "ClientToScreen" in gui_source
assert "Math.Abs(searchMid-buttonMid)>6" in gui_source
assert "search.Right>button.Left" in gui_source
assert "artists.Right>releases.Left" in gui_source
assert "box.Right>right+2 || box.Bottom>bottom+2" in gui_source
assert "DISCOGGER_GUI_LAYOUT_$Architecture=PASS" in gui_source
assert gui_source.index("CheckFindReleasesGeometry($window)") < gui_source.index("$after=Get-Item -LiteralPath $wav")
assert gui_source.index("CheckFindReleasesGeometry($window)") > gui_source.index("if(-not [DiscoggerWindowTest]::Responsive($window))")
print("PASS: read-only native dialog layout, clipping and search alignment geometry wired for both hosts.")


# Exercise the actual modeless foobar Win32/x64 dialog's resize, normal
# close/reopen and temporary profile position/size persistence without
# touching any synthetic WAV bytes or invoking tagging/CUE writer commands.
gui_source = (root / "scripts/host-discogger-gui.ps1").read_text(encoding="utf-8-sig")
dialog_src = (root / "find_release_dialog.cpp").read_text(encoding="utf-8-sig")
assert "ResizeForLayoutTest(IntPtr dialog)" in gui_source
assert "MoveWindow(dialog,x,y,width,height,true)" in gui_source
assert "DISCOGGER_GUI_RESIZE_$Architecture=PASS" in gui_source
assert "PostMessageW($window,0x0010" in gui_source
assert "DISCOGGER_GUI_PERSISTENCE_$Architecture=PASS" in gui_source
assert "Dialog size/position persistence failed" in gui_source
assert "CheckFindReleasesGeometry($reopened)" in gui_source
assert gui_source.index("DISCOGGER_GUI_RESIZE_$Architecture=PASS") < gui_source.index("DISCOGGER_GUI_PERSISTENCE_$Architecture=PASS")
assert gui_source.index("DISCOGGER_GUI_PERSISTENCE_$Architecture=PASS") < gui_source.index("$after=Get-Item -LiteralPath $wav")
assert "cfg_dialog_position_find_release_dlg.AddWindow(m_hWnd);" in dialog_src
assert "cfg_dialog_position_find_release_dlg.RemoveWindow(m_hWnd);" in dialog_src
print("PASS: disposable host resize, close/reopen and GUI position persistence gates.")

# Vendor-independent SDK provenance must be exactly pinned to the
# owner's previously captured source tree. Do not claim an archive SHA256
# for the Git checkout, and never retry a blocked foobar2000.org download.
sdk_probe = (root / "scripts/probe-official-sdk.ps1").read_text(encoding="utf-8-sig")
assert "$SdkVersion = '2026-10-01'" in sdk_probe
assert "$TemplateCommit = '31a35947a7527826c2199491692802e3ce3d8bd7'" in sdk_probe
assert "$TemplateSdkTree = '2ba8caeb73f72633ad7f60ee5f72ed8074a0eb17'" in sdk_probe
assert "$ExpectedSha256 = 'd4c55077336fae81bf8df0259b5b2748fa45ea84132c656ead93eb123cbcdc26'" in sdk_probe
assert "DISCOGGER_SDK_SOURCE_GIT_TREE_VERIFIED=PASS" in sdk_probe
assert "DISCOGGER_SDK_ORIGINAL_ARCHIVE_SHA256=NOT_TESTED_BY_THIS_RUN" in sdk_probe
assert "Invoke-WebRequest" not in sdk_probe and "Start-Sleep" not in sdk_probe
sdk_workflow = (root / ".github/workflows/validation.yml").read_text(encoding="utf-8-sig")
assert "repository: lxsdd/foobar2000_component_template" in sdk_workflow
assert "ref: 31a35947a7527826c2199491692802e3ce3d8bd7" in sdk_workflow
assert "path: external/sdk-template" in sdk_workflow


# Native actor/load path must include the declaration for the same paginated
# payload completeness predicate already covered in portable cache tests.
_native_discogs = (root / "discogs.cpp").read_text(encoding="utf-8-sig")
assert '#include "discogger_cache_pages.h"' in _native_discogs
assert 'discogger_cache::valid_cache_payload_count(' in _native_discogs
print("PASS: release-page completeness guard is declared in the native translation unit.")

# A duration tie or equally good already claimed Discogs track must not be
# silently mapped according to source row order. Route to manual matching.
duration_helper = (root / "discogger_duration_match.h").read_text(encoding="utf-8-sig")
tag_writer = (root / "tag_writer.cpp").read_text(encoding="utf-8-sig")
duration_route = tag_writer[tag_writer.index("int TagWriter::order_tracks_by_duration("):
                            tag_writer.index("int TagWriter::order_tracks_by_number(")]
assert '#include "discogger_duration_match.h"' in tag_writer
assert "unique_nearest_unclaimed(deltas, claimed)" in duration_route
assert "if (selected == static_cast<size_t>(-1)" in duration_route
assert "claimed[selected] = true" in duration_route
assert "return MATCH_FAIL;" in duration_route
assert "deltas[i] == best" in duration_helper
assert "claimed[i] && deltas[i] <= best" in duration_helper
for unit_runner in ("scripts/run-msvc-unit-tests.ps1", ".github/workflows/validation.yml"):
    assert "duration_match_test.cpp" in (root / unit_runner).read_text(encoding="utf-8-sig")
# GitHub may defer synchronization of PR heads after connector Contents
# updates. Keep the direct draft push trigger while sharing cancellation
# concurrency with PR events, rather than relying on an old PR-only rule.
validation_trigger = (root / ".github/workflows/validation.yml").read_text(encoding="utf-8-sig")
assert "  pull_request:" in validation_trigger
assert "  push:" in validation_trigger
assert "- work/portable-build-properties-20261018" in validation_trigger
assert "github.head_ref || github.ref_name" in validation_trigger
assert "github.event_name == 'workflow_dispatch' && 'manual' || 'automatic'" in validation_trigger
assert "cancel-in-progress: true" in validation_trigger
print("PASS: conservative duration tie handling; manual host runs cannot be cancelled by delayed pushes.")

# The generator stores approval rows only for enabled mapped tracks; the
# write path must not index those rows with the unfiltered mapping index.
writer_source = (root / "tag_writer.cpp").read_text(encoding="utf-8-sig")
writer_commit = writer_source[writer_source.index("void TagWriter::write_tags()"):
                              writer_source.index("// single vals", writer_source.index("void TagWriter::write_tags()"))]
assert '#include "discogger_result_alignment.h"' in writer_source
assert "valid_result_shape(" in writer_commit
assert writer_commit.index("valid_result_shape(") < writer_commit.index("invalidate_all();")
assert "const size_t approval_row = enabled_result_index++;" in writer_commit
assert writer_commit.count("process_result(item, &info, approval_row") == 2
assert "process_result(item, &info, i /*ndx_track_mappings*/" not in writer_commit
assert "process_result(item, &info, i, result" not in writer_commit
assert "std::set<size_t> used_file_indices;" in writer_commit
assert "tag_results_mask.size() > tag_results.get_count()" in writer_commit
assert "discogger_write_safety::staging_rollback_guard stage_guard(" in writer_commit
assert writer_commit.index("staging_rollback_guard stage_guard(") < writer_commit.index("invalidate_all();")
assert "manager->discard_pending_changes();" in writer_commit
assert "manager->begin_physical_read();" in writer_commit
assert writer_commit.index("stage_guard.disarm();") > writer_commit.index("}//end tracks loop")
assert "Tag generation stopped: Discogs resource missing (HTTP 404)." in writer_source
for runner in ("scripts/run-msvc-unit-tests.ps1", ".github/workflows/validation.yml"):
    assert "result_alignment_test.cpp" in (root / runner).read_text(encoding="utf-8-sig")
print("PASS: compressed enabled-track preview rows, stale row reject, and HTTP 404 fail-closed.")

# Statistics must never dereference a stale/missing or truncated preview row.
stats_source = (root / "preview_dialog.cpp").read_text(encoding="utf-8-sig")
stats_block = stats_source[stats_source.index("void CPreviewTagsDialog::compute_stats_track_map("):]
assert '#include "discogger_result_alignment.h"' in stats_source
assert "const size_t tk_count = res ? res->r_approved.get_count() : 0;" in stats_block
assert "valid_result_shape(" in stats_block
assert "if (!res || !res->tag_entry ||" in stats_block
assert "m_vstats.emplace_back(preview_stats{});" in stats_block
assert stats_block.count("if (walk_tag >= m_vstats.size()) m_vstats.resize(walk_tag + 1);") >= 2
assert stats_block.index("valid_result_shape(") < stats_block.index("const tag_mapping_entry* entry = res->tag_entry;")
print("PASS: preview statistics reject stale tag rows and retain stable vector indices.")

# Discogs' pagination header is not proof that the JSON response actually
# included the expected number of release/search/collection objects.
network_pages = (root / "discogs_interface.cpp").read_text(encoding="utf-8-sig")
pagination_code = network_pages[network_pages.index("pfc::array_t<JSONParser_ptr> DiscogsInterface::get_all_pages("):
                                network_pages.index("DiscogsInterface::get_all_pages_offline_cache(")]
assert pagination_code.count("discogger_cache::valid_online_payload_count(") == 2
assert pagination_code.count('json_object_get(jp->root, "results")') == 2
assert pagination_code.count('json_object_get(jp->root, "releases")') == 2
assert pagination_code.count('json_object_get(jp->root, "versions")') == 2
assert pagination_code.count("json_is_array(page_entries)") == 2
assert "valid_online_payload_count(" in (root / "tests/cache_pages_test.cpp").read_text(encoding="utf-8-sig")
print("PASS: online search and artist pagination checks validate actual JSON array payload counts.")

# Windows foobar2000 SDK headers may define the max(a,b) macro. Never use
# unparenthesized numeric_limits<T>::max() inside a native component TU.
duration_header = (root / "discogger_duration_match.h").read_text(encoding="utf-8-sig")
assert "int best = (std::numeric_limits<int>::max)();" in duration_header
assert "std::numeric_limits<int>::max();" not in duration_header
print("PASS: duration matcher compiles with Windows SDK max macro.")

# A call-site-only pagination predicate is not sufficient; require the
# actual helper definition in the checked-out SDK-independent header.
cache_header = (root / "discogger_cache_pages.h").read_text(encoding="utf-8-sig")
assert "inline bool valid_online_payload_count(" in cache_header
assert "return valid_cache_payload_count(requested_page - 1, page_count," in cache_header
print("PASS: native/portable online page payload verifier has an actual definition.")


# A generator failure must never publish partial approval rows as ready, and
# neither the preview button nor the physical writer may reuse them.
tagging_code = (root / "tag_writer.cpp").read_text(encoding="utf-8-sig")
generator = tagging_code.split("void TagWriter::generate_tags(", 1)[1].split(
    "struct write_tag_nfo", 1
)[0]
assert generator.count("atm_tag_results_ready = true;") == 1
assert "catch (std::exception e)" not in generator
assert "throw foo_discogs_exception(\"Tag generation stopped: missing selected file handle.\");" in generator
assert 'if (!m_finfo_manager || !ResultsReady())' in tagging_code
preview_code = (root / "preview_dialog.cpp").read_text(encoding="utf-8-sig")
write_button = preview_code.split("bool CPreviewTagsDialog::check_write_tags_status()", 1)[1].split(
    "bool CPreviewTagsDialog::init_other_controls_and_results()", 1
)[0]
assert "if (!m_tag_writer || !m_tag_writer->ResultsReady()) return false;" in write_button
print("PASS: failed tag generation never re-enables stale preview writes.")

# Empty/malformed Discogs artist lists cannot be dereferenced by preview
# context menus, including a direct command without a menu selection.
preview_menu = preview_code.split("bool CPreviewTagsDialog::context_menu_show(", 1)[1].split(
    "bool CPreviewTagsDialog::context_menu_switch(", 1
)[0]
assert "if (!m_tag_writer || !m_tag_writer->GetRelease())" in preview_menu
assert "tw_release->artists.get_count() && tw_release->artists[0]" in preview_menu
assert "is_enabled() && m_tag_writer->ResultsReady()" in preview_menu
preview_commands = preview_code.split("bool CPreviewTagsDialog::context_menu_switch(", 1)[1].split(
    "bool CPreviewTagsDialog::check_write_tags_status()", 1
)[0]
assert "current->artists.get_count()" in preview_commands
assert "static_cast<size_t>(cmd - ID_URL_ARTISTS) < vartists.size()" in preview_commands
print("PASS: empty artists and stale previews do not enable context menu writes.")

# Expanded real Win32/x64 host covers synthetic MP3, embedded FLAC CUE and
# multi-file external CUE. These are READ-ONLY gates, NOT physical writes.
host_test = (root / "scripts/host-discogger-gui.ps1").read_text(encoding="utf-8-sig")
host_workflow = (root / ".github/workflows/validation.yml").read_text(encoding="utf-8-sig")
mp3_fixture = (root / "tests/create-synthetic-mp3.py").read_text(encoding="utf-8-sig")
assert "DISCOGGER_GUI_MP3_FLAC_CUE_READONLY_" in host_test
assert "MEDIA_WRITE_SAFETY_" in host_test and "=NOT_TESTED" in host_test
assert "synthetic-embedded-flac.py" in host_test
assert "synthetic-multifile.cue" in host_test
assert "35369054c56f4c6ccb2f802944e9675852fd35ec" in host_workflow
assert "b430229832fb8a1e7824716b1fefb34e5706ea89593dd73ddf7133c17ae8bec0" in mp3_fixture
print("PASS: pinned synthetic MP3/FLAC/CUE native read-only media matrix wired.")

# Runner-only native SDK writer harness: no production QA command and no
# non-disposable paths; actual writes are independently checked by Python.
qa_source = (root / "tests/native-media-writer-qa.inc").read_text(encoding="utf-8-sig")
plugin_source = (root / "foo_discogs.cpp").read_text(encoding="utf-8-sig")
assert '#if defined(DISCOGGER_RUNNER_MEDIA_QA)' in plugin_source
assert 'CL: /DDISCOGGER_RUNNER_MEDIA_QA' in host_workflow
assert 'DISCOGGER_QA_MEDIA_DIR' in host_test
assert 'DISCOGGER_PHYSICAL_TAG_WRITE_NOOP_' in host_test
for guard in ('GITHUB_ACTIONS', 'RUNNER_TEMP', 'DISCOGGER_QA_MEDIA_DIR',
              'weakly_canonical', 'is_regular_file', 'is_symlink'):
    assert guard in qa_source
assert 'verify-runner-physical-tags.py' in host_test
print("PASS: test-only actual file_info_manager write and no-op harness is wired.")

# Verify native QA uses the real SDK menu header and acknowledges both
# asynchronous writer completions; rejected second commands are not no-ops.
assert '#include "SDK/menu.h"' in qa_source
assert '#include "SDK/mainmenu.h"' not in qa_source
assert "qa-mp3-completed.txt" in qa_source
assert "qa-flac-completed.txt" in qa_source
assert "manager->write_infos([completion, sequence]()" in qa_source
assert "Expected physical tag write AND completion callback" in host_test
assert "identical second tag command lacked success/no-op callback" in host_test
print("PASS: test-only physical commit success and genuine no-op callback gates.")

# Windows MSVC's default C++20 char8_t must not leak into std::string paths.
assert "const std::string utf8path(utf8_units.begin(), utf8_units.end());" in qa_source
assert "const std::string logical = \"file://\" + utf8path;" in qa_source
print("PASS: runner-only native QA accepts MSVC char8_t UTF-8 file paths.")

# Actual native host rejects virtual CUE subsongs and duplicate target paths.
assert "run_disposable_target_rejection_qa()" in qa_source
assert "virtualTrack->get_subsong_index() != 1" in qa_source
assert "duplicates.add_item(physicalTrack);" in qa_source
assert qa_source.count("target->write_infos({}, std::move(lease), false);") == 2
assert "DISCOGGER_PHYSICAL_VIRTUAL_DUPLICATE_DENIAL_" in host_test
assert "qa-unsafe-targets-rejected.txt" in host_test
print("PASS: host preflight rejects virtual CUE and duplicate metadata writes.")

# ID3v2 custom tags are often serialized as UTF-16 TXXX, never use only
# raw ASCII substring searches to accept/reject a physical MP3 write.
media_verifier = (root / "tests/verify-runner-physical-tags.py").read_text(encoding="utf-8-sig")
assert "def id3_txxx_values(content):" in media_verifier
assert "def verify_exact_mp3_runner_marker(content):" in media_verifier
assert "verify_exact_mp3_runner_marker(after)" in media_verifier
assert 'if markers != [("DISCOGGER_QA_RUNNER", "RUNNER-VERIFIED")]:' in media_verifier
assert 'raise AssertionError("Malformed ID3v2 TXXX missing value separator")' in media_verifier
assert "--selftest" in media_verifier
print("PASS: physical MP3 QA rejects malformed TXXX and duplicate markers (UTF-16 included).")

# Avoid mutating file_info at all when tag value did not change. This guards
# against FLAC comment reorder on redundant meta_set and protects mtime.
tag_writer_src = (root / "tag_writer.cpp").read_text(encoding="utf-8-sig")
qa_media_src = (root / "tests/native-media-writer-qa.inc").read_text(encoding="utf-8-sig")
write_header = (root / "discogger_write_safety.h").read_text(encoding="utf-8-sig")
assert "inline bool unchanged_single_value(" in write_header
assert "discogger_write_safety::unchanged_single_value(" in tag_writer_src
assert "discogger_write_safety::unchanged_single_value(" in qa_media_src
assert "const bool unchanged = discogger_write_safety::unchanged_single_value(" in qa_media_src
assert "if (!unchanged) {" in qa_media_src
print("PASS: single-value unchanged metadata does not call meta_set or physical write.")

# Real FLAC no-op failures must identify whether the original readback matches
# the desired single value, and distinguish actual byte rewrites from mtime.
qa_host = (root / "scripts/host-discogger-gui.ps1").read_text(encoding="utf-8-sig")
qa_source = (root / "tests/native-media-writer-qa.inc").read_text(encoding="utf-8-sig")
assert 'qa-flac-stage-evidence.txt' in qa_source
assert 'qa-flac-stage-evidence.txt' in qa_host
assert 'DISCOGGER_QA_NOOP_DIAGNOSTIC=' in qa_host
print("PASS: synthetic FLAC readback and byte/mtime diagnostics are wired.")

# Runner-only SDK diagnostic checks physical FLAC versus embedded-CUE virtual
# projection without logging tag contents or touching user media.
assert "input_entry::g_open_for_info_read(reader, nullptr, logical.c_str(), abort);" in qa_source
assert "qa-flac-input-reader-evidence.txt" in qa_source
assert "reader->get_info(0, raw, abort);" in qa_source
print("PASS: native FLAC physical-versus-CUE read-only diagnostic compiled into runner QA.")

# Physical FLAC assertions use parsed Vorbis key/value pairs. A track-local
# CUE_TRACKNN_DISCOGGER_QA_RUNNER field must not masquerade as the physical
# DISCOGGER_QA_RUNNER field; preserve CUESHEET text bytes and FLAC Type-5.
verification_code = (root / "tests/verify-runner-physical-tags.py").read_text(encoding="utf-8-sig")
assert "def flac_vorbis_fields(block):" in verification_code
assert 'after_fields.get("DISCOGGER_QA_RUNNER") != [b"RUNNER-VERIFIED"]' in verification_code
assert 'before_fields["CUESHEET"] != after_fields.get("CUESHEET")' in verification_code
assert "selftest_flac_vorbis()" in verification_code
print("PASS: FLAC physical metadata preservation checks exact semantic fields.")

# Physical metadata for embedded FLAC cue must be re-read through the SDK
# input_info_reader, never replaced by an old metadb cached track projection.
manager_code = (root / "file_info_manager.cpp").read_text(encoding="utf-8-sig")
host_code = (root / "scripts/host-discogger-gui.ps1").read_text(encoding="utf-8-sig")
assert 'input_entry::g_open_for_info_read(reader, nullptr, path, abort);' in manager_code
assert 'if (val && items[n]->get_subsong_index() == 0)' in manager_code
assert 'reader->get_subsong_count() > 1' not in manager_code
assert 'reader->get_info(0, physical, abort);' in manager_code
assert 'info_wrappers[n]->info = physical;' in manager_code
assert 'DISCOGGER_FLAC_PHYSICAL_SDK_READBACK_' in host_code
assert 'count=1 unchanged=1' in host_code
print("PASS: multi-subsong FLAC physical metadata overrides stale cue projection.")

# Synthetic original tag + binary-artwork acceptance uses no third-party image
# files and compares full original compressed payload and picture bytes.
mp3_gen = (root / "tests/create-synthetic-mp3.py").read_text(encoding="utf-8-sig")
flac_gen = (root / "tests/enrich-synthetic-flac.py").read_text(encoding="utf-8-sig")
phys_verify = (root / "tests/verify-runner-physical-tags.py").read_text(encoding="utf-8-sig")
host_media = (root / "scripts/host-discogger-gui.ps1").read_text(encoding="utf-8-sig")
assert "def with_synthetic_apic(raw):" in mp3_gen
assert "def enrich(path):" in flac_gen
assert 'if os.getenv("GITHUB_ACTIONS") != "true"' in flac_gen
assert "id3_apic_frames(before)" in phys_verify
assert "first.get(6) != second.get(6)" in phys_verify
assert 'verify_original_flac_vorbis(before_fields, after_fields)' in phys_verify
assert 'return {key: Counter(values) for key, values in fields.items()' in phys_verify
assert 'DISCOGGER_FLAC_ALL_PHYSICAL_FIELDS_PRESERVED=PASS' in phys_verify
assert "id3_audio_payload(before) != id3_audio_payload(after)" in phys_verify
assert "tests/enrich-synthetic-flac.py" in host_media
print("PASS: runner actual MP3/FLAC writer preserves embedded pictures, multivalues and audio.")

# Real SDK host must reject an entire two-file write selection with a stale
# original snapshot before touching either physical medium. This does not
# claim rollback if the SDK fails *after* partially completing I/O.
batch_source = (root / "tests/native-media-writer-qa.inc").read_text(encoding="utf-8-sig")
batch_host = (root / "scripts/host-discogger-gui.ps1").read_text(encoding="utf-8-sig")
assert "run_disposable_incomplete_batch_qa()" in batch_source
assert "manager->info_wrappers[1]->original_captured = false;" in batch_source
assert "qa-incomplete-batch-rejected.txt" in batch_source
assert "DISCOGGER_PHYSICAL_BATCH_PREFLIGHT_ATOMIC_" in batch_host
assert "BATCH=REJECTED_BEFORE_IO" in batch_host
print("PASS: actual host two-file write preflight rejection is wired.")

# Only runner-only foobar DLLs may inject synthetic populated search objects.
# The real UI model, exact-only checkbox and release tree must be exercised.
gui_header = (root / "find_release_dialog.h").read_text(encoding="utf-8-sig")
gui_source = (root / "find_release_dialog.cpp").read_text(encoding="utf-8-sig")
runner_commands = (root / "tests/native-media-writer-qa.inc").read_text(encoding="utf-8-sig")
gui_host = (root / "scripts/host-discogger-gui.ps1").read_text(encoding="utf-8-sig")
assert "runner_qa_populated_search_rows()" in gui_header
assert "#if defined(DISCOGGER_RUNNER_MEDIA_QA)" in gui_header
assert "combined_advanced_search_preview(artists)" in gui_source
assert "ShowArtistSearchResults(preview);" in gui_source
assert "runner_qa_live_search_probe(const char* expected_release_id)" in gui_source
assert "run_live_search_probe_qa()" in runner_commands
assert "[switch]$AuthorizedLive" in gui_host
assert "DISCOGGER_AUTHENTICATED_GUI_SEARCH_" in gui_host
assert "DISCOGGER_AUTHENTICATED_EMPTY_RESULT_" in gui_host
assert 'IsDlgButtonChecked(IDC_CHK_ONLY_EXACT_MATCHES) != BST_UNCHECKED' in gui_source
assert "run_populated_search_gui_qa()" in runner_commands
assert "DISCOGGER_GUI_POPULATED_SYNTHETIC_RELEASES_" in gui_host
print("PASS: runner-only real populated GUI artist/release path is wired.")

# An independent physical file edit between preview and asynchronous commit
# must cause the entire selected batch to be rejected before any write.
manager_source = (root / "file_info_manager.cpp").read_text(encoding="utf-8-sig")
qa_source = (root / "tests/native-media-writer-qa.inc").read_text(encoding="utf-8-sig")
qa_host = (root / "scripts/host-discogger-gui.ps1").read_text(encoding="utf-8-sig")
assert "Tagging stopped: physical tags changed since preview" in manager_source
assert "input_entry::g_open_for_info_read(reader, nullptr, path, abort);" in manager_source
assert "run_disposable_stale_disk_qa()" in qa_source
assert 'other_editor->commit(abort);' in qa_source
assert "DISCOGGER_PHYSICAL_STALE_DISK_REJECTION_" in qa_host
print("PASS: stale external physical metadata edits reject pending SDK writes.")

# Runner-only real foobar writer must reject unapproved GENRE removal and
# flat CUESHEET editing while leaving file bytes and mtime unchanged.
native_qa = (root / "tests/native-media-writer-qa.inc").read_text(encoding="utf-8-sig")
native_host = (root / "scripts/host-discogger-gui.ps1").read_text(encoding="utf-8-sig")
assert "run_disposable_unapproved_removal_qa()" in native_qa
assert 'const char* removals[] = {"GENRE", "CUESHEET"};' in native_qa
assert "manager->write_infos({}, std::move(lease), false);" in native_qa
assert "DISCOGGER_PHYSICAL_UNAPPROVED_REMOVAL_DENIAL_" in native_host
print("PASS: real synthetic FLAC unapproved GENRE/CUESHEET removals fail closed.")

# Keep the entire five-release, read-only Discogs acceptance cohort wired.
# Search assertions in this job may run only during an explicitly authorized
# manual dispatch, never in public PR jobs or arbitrary downloaded binaries.
live_workflow = (root / ".github/workflows/validation.yml").read_text(encoding="utf-8-sig")
live_segment = live_workflow.split("  authorized-discogs-live:", 1)[1]
for release_id, artist, title in (
    ("38012673", "Estiva", "Via Balearia"),
    ("37332711", "Kyau & Albert x Maywave", "1000 Years"),
    ("36397609", "Matt Fax", "Only You"),
    ("36819730", "Matt Fax", "Till You Let Go"),
    ("35534398", "Quivver", "Consciousness EP"),
):
    assert f"('{release_id}','{artist}','{title}'" in live_segment
assert "github.event_name == 'workflow_dispatch'" in live_segment
assert "DISCOGS_LIVE_DISCOVERY=PASS: 5/5" in live_segment
assert "if found != len(releases):" in live_segment
assert "conn.request('GET', path, headers=headers)" in live_segment
assert "return {str(x.get('id'))" in live_segment
print("PASS: authorized five-release Discogs live regression set remains read-only.")

# The foobar Win32 list/tree must never be mutated by a modeless worker.
# A completed result is installed only by the SDK main-thread on_success
# callback, and only for the original, still-open release dialog HWND.
async_tasks_header = (root / "tasks.h").read_text(encoding="utf-8-sig")
async_tasks_source = (root / "tasks.cpp").read_text(encoding="utf-8-sig")
for callback_name, ui_function in (
    ("on_get_artist_done_process_callback", "on_get_artist_done("),
    ("on_search_artist_done_process_callback", "on_search_artist_done_if_current("),
):
    begin = async_tasks_source.split(
        f"void {callback_name}::safe_run(", 1)[1]
    background = begin.split(f"void {callback_name}::on_success(", 1)[0]
    foreground = begin.split(f"void {callback_name}::on_success(", 1)[1].split(
        f"void {callback_name}::on_abort(", 1)[0]
    assert ui_function not in background, callback_name
    assert "p_abort.check();" in background, callback_name
    assert "PFC_ASSERT(core_api::is_main_thread());" in foreground, callback_name
    assert "::IsWindow(m_owner_window)" in foreground, callback_name
    assert "g_discogs->find_release_dialog->m_hWnd != m_owner_window" in foreground, callback_name
    assert ui_function in foreground, callback_name
    starter = async_tasks_source.split(f"void {callback_name}::start(", 1)[1].split(
        f"void {callback_name}::safe_run(", 1)[0]
    assert "m_owner_window = parent;" in starter, callback_name
assert async_tasks_header.count("HWND m_owner_window = nullptr;") >= 2
print("PASS: artist and search list/tree updates are guarded main-thread completions.")

# Loading release versions may run off-thread; mutating the TreeView may not.
expand_source = (root / "tasks.cpp").read_text(encoding="utf-8-sig")
expand_header = (root / "tasks.h").read_text(encoding="utf-8-sig")
expand_parts = expand_source.split("void expand_master_release_process_callback::start(", 1)[1].split(
    "void tree_apply_filter_process_callback::start(", 1)[0]
expand_background = expand_parts.split("void expand_master_release_process_callback::safe_run(", 1)[1].split(
    "void expand_master_release_process_callback::on_success(", 1)[0]
expand_foreground = expand_parts.split("void expand_master_release_process_callback::on_success(", 1)[1].split(
    "void expand_master_release_process_callback::on_abort(", 1)[0]
assert "on_expand_master_release_done(" not in expand_background
assert "m_staged_master->load_releases(" in expand_background
assert "m_master_release->load_releases(" not in expand_background
assert "m_master_release->sub_releases" not in expand_background
assert "std::make_shared<Release>(*child)" in expand_parts
assert "m_staged_master->sub_releases.append_single(" in expand_parts
assert "m_staged_master->sub_releases = m_master_release->sub_releases;" not in expand_parts
assert "m_staged_master->loaded_releases = m_master_release->loaded_releases;" in expand_parts
assert "m_staged_master->loaded_releases" in expand_foreground
assert "m_staged_master" in expand_header
assert "p_abort.check();" in expand_background
assert "PFC_ASSERT(core_api::is_main_thread());" in expand_foreground
assert "::IsWindow(m_owner_window)" in expand_foreground
assert "g_discogs->find_release_dialog->m_hWnd != m_owner_window" in expand_foreground
assert "on_expand_master_release_done(" in expand_foreground
assert "fake_threaded_process_status status;" in expand_foreground
assert "m_master_release, m_pos, status, abort, m_staged_master" in expand_foreground
assert "m_owner_window = parent;" in expand_parts
assert "class expand_master_release_process_callback" in expand_header
print("PASS: owner-bound main-thread master expansion, background load-only.")
# The worker must parse into independent Release objects, including AT
# partial matches; no cached Release or master is modified before UI commit.
assert "m_query_mode, true);" in expand_background
master_parser = parsed_releases.split("void Discogs::parseMasterVersions(", 1)[1].split(
    "void Discogs::parseArtist(", 1)[0]
assert "if (isolated_versions)" in master_parser
assert "release = std::make_shared<Release>(release_id.get_ptr());" in master_parser
assert "release = master_release->sub_releases[found_i];" in master_parser
assert "release->master_id = master_release->id;" in master_parser
assert "master = discogs_interface->get_master_release(lkey);" in master_parser
assert "MasterRelease_ptr master;" in master_parser
native_dialog = (root / "find_release_dialog.cpp").read_text(encoding="utf-8-sig")
assert "runner_qa_native_master_child_isolation();" in native_dialog
assert "runner_qa_native_search_preview_fields();" in native_dialog
assert "DISCOGGER_NATIVE_SEARCH_PREVIEW_FIELDS_QA=PASS" in native_dialog
assert "Discogs::parseArtistReleases(root, artist.get(), SearchMode::AT, masters);" in native_dialog
assert "parse_release_search_fields(root, release);" in native_dialog

assert "DISCOGGER_NATIVE_MASTER_CHILD_ISOLATION_QA=PASS" in native_dialog
print("PASS: worker-owned master children, native Jansson partial/full isolation contract.")


# The completion of a master-release HTTP load may race with a tree/filter
# change. Its UI callback must reject stale or invalid source and list rows
# both before mutations and before the subsequent indexed auto-fill lookup.
tree_source = (root / "find_release_tree.cpp").read_text(encoding="utf-8-sig")
master_completion = tree_source.split("void CFindReleaseTree::on_expand_master_release_done(", 1)[1].split(
    "bool CFindReleaseTree::set_node_expanded(", 1)[0]
assert "PFC_ASSERT(core_api::is_main_thread());" in master_completion
assert "!master_release || !m_find_release_artist || list_index < 0" in master_completion
assert "static_cast<size_t>(list_index) >= original_rows->size()" in master_completion
assert master_completion.index("original_rows->size()") < master_completion.index("m_rt_cache.expand_releases(")
assert "staged_master->id != master_release->id" in master_completion
assert "master_release->sub_releases = staged_master->sub_releases;" in master_completion
assert "child->master_release = master_release;" in master_completion
assert "child->master_id = master_release->id;" in master_completion
assert master_completion.index("child->master_release = master_release;") < master_completion.index("master_release->sub_releases = staged_master->sub_releases;")
assert "master_release->loaded_releases = true;" in master_completion
assert master_completion.index("m_find_release_artist->master_releases[selected_row.master_ndx] != master_release") < master_completion.index("master_release->sub_releases = staged_master->sub_releases;")
assert "runner_qa_staged_master_publication()" in tree_source
assert "DISCOGGER_NATIVE_MASTER_STAGED_PUBLICATION_QA=PASS" in (root / "find_release_dialog.cpp").read_text(encoding="utf-8-sig")
assert "DISCOGGER_NATIVE_PARTIAL_MASTER_INDEX_QA=PASS" in (root / "find_release_dialog.cpp").read_text(encoding="utf-8-sig")
assert "discogger_master::find_partial_version_index(" in parsed_releases
assert "master_release->sub_releases[found_i]->id" in parsed_releases
assert "master_release->sub_releases[i]->id" not in parsed_releases[parsed_releases.index("void Discogs::parseMasterVersions("):parsed_releases.index("void Discogs::parseArtist(")]
assert "static_cast<size_t>(list_index) >= vec_items->size()" in master_completion
assert master_completion.index("vec_items->size()") < master_completion.index("vec_items->at(list_index)")
print("PASS: asynchronous master tree updates guard stale artist and row indices.")


# Tree filtering must never call Win32 controls or mutate the live tree from
# the modeless worker. Main-thread completion must reject stale dialog and
# edit/search snapshots; cancellation must not dereference a closed dialog.
filter_parts = async_tasks_source.split(
    "void tree_apply_filter_process_callback::start(", 1)[1].split(
    "void on_search_artist_done_process_callback::start(", 1)[0]
filter_worker = filter_parts.split(
    "void tree_apply_filter_process_callback::safe_run(", 1)[1].split(
    "void tree_apply_filter_process_callback::on_success(", 1)[0]
filter_complete = filter_parts.split(
    "void tree_apply_filter_process_callback::on_success(", 1)[1].split(
    "void tree_apply_filter_process_callback::on_abort(", 1)[0]
filter_abort = filter_parts.split(
    "void tree_apply_filter_process_callback::on_abort(", 1)[1].split(
    "void tree_apply_filter_process_callback::on_error(", 1)[0]
assert "m_owner_window = parent;" in filter_parts
assert "p_abort.check();" in filter_worker
assert "apply_filter(" not in filter_worker
assert "find_release_dialog" not in filter_worker
assert "PFC_ASSERT(core_api::is_main_thread());" in filter_complete
assert "::IsWindow(m_owner_window)" in filter_complete
assert "m_HWnd != m_owner_window" not in filter_complete
assert "->m_hWnd != m_owner_window" in filter_complete
assert "GetSearchMode()" in filter_complete
assert "uGetWindowText(find_dlg->m_edit_filter)" in filter_complete
assert "find_dlg->apply_filter(" in filter_complete
assert "fake_threaded_process_status status;" in filter_complete
assert "PFC_ASSERT(core_api::is_main_thread());" in filter_abort
assert "::IsWindow(m_owner_window)" in filter_abort
assert "HWND m_owner_window = nullptr;" in async_tasks_header
print("PASS: filter tree mutations stay on originating UI thread; stale edit/dialog discarded.")


# A cancelled tag-preparation worker must never reach the UI-thread commit
# callback. The underlying TagWriter preparation does not take an abort
# callback, so the post-preparation check is essential for late cancellation.
write_worker = async_tasks_source.split(
    "void write_tags_task::safe_run(", 1)[1].split(
    "void write_tags_task::on_success(", 1)[0]
write_commit = async_tasks_source.split(
    "void write_tags_task::on_success(", 1)[1].split(
    "void write_tags_task::on_abort(", 1)[0]
prepare_call = write_worker.index("m_tag_writer->write_tags();")
checks = [m.start() for m in __import__("re").finditer(r"p_abort\.check\(\);", write_worker)]
assert len(checks) >= 2 and checks[0] < prepare_call < checks[-1]
assert "discogger_require_physical_targets(" in write_worker
assert "write_infos(" not in write_worker
assert "write_infos(" in write_commit
print("PASS: cancelled tag preparation cannot enter the physical SDK writer.")

# Modeless on_success runs on foobar's UI thread. Ordinary std::exceptions
# (including physical readback and artwork scheduling failures) must be
# reported as fatal rather than escaping the SDK callback.
completion_header = (root / "discogger_task_completion.h").read_text(encoding="utf-8-sig")
completion_body = async_tasks_source.split(
    "void foo_discogs_threaded_process_callback::on_done(", 1)[1].split(
    "generate_tags_task::generate_tags_task(", 1)[0]
assert '#include "discogger_task_completion.h"' in async_tasks_source
assert "run_success_with_error_boundary(" in completion_body
assert "on_success(p_wnd);" in completion_body
assert "add_error(message, true);" in completion_body
assert "catch (const std::exception& e)" in completion_header
assert "catch (...)" in completion_header
print("PASS: UI-thread task completion contains ordinary and unknown exceptions.")

# A missing DISCNUMBER may match tracks on multiple discs. Native numbered
# matching must reject ambiguous candidates before assigning an enabled row;
# otherwise the legacy first-match path could silently tag the wrong track.
numbered = (root / "tag_writer.cpp").read_text(encoding="utf-8-sig")
numbered_body = numbered.split("int TagWriter::order_tracks_by_number(", 1)[1].split(
    "int TagWriter::order_tracks_by_assumption(", 1)[0]
assert '#include "discogger_number_match.h"' in numbered
assert "unique_number_match match;" in numbered_body
assert "match.consider(" in numbered_body
assert "if (match.ambiguous()) return MATCH_FAIL;" in numbered_body
assert numbered_body.index("if (match.ambiguous())") < numbered_body.index("mappings[i].enabled = true;")
print("PASS: ambiguous multidisc track numbers cannot auto-approve the first hit.")

# A failed or ambiguous match must never silently approve positional mappings.
# Previously match_tracks() used "!= MATCH_SUCCESS" and enabled every row even
# after route_discogs_track_order() returned MATCH_FAIL.
matching = (root / "tag_writer.cpp").read_text(encoding="utf-8-sig")
match_entry = matching.split("void TagWriter::match_tracks() {", 1)[1].split(
    "int TagWriter::route_discogs_track_order(", 1)[0]
assert '#include "discogger_match_policy.h"' in matching
assert "discogger_match_policy::for_status(" in match_entry
assert "action::disable" in match_entry and "action::preserve" in match_entry
assert "m_track_mappings[i].enabled = false;" in match_entry
assert match_entry.index("m_track_mappings[i].enabled = false;") < match_entry.index("tm.enabled = true;")
assert "m_match_status != MATCH_SUCCESS" not in match_entry
print("PASS: failed/ambiguous matching cannot silently enable positional assignments.")

# The FIRST native search completion runs after potentially long remote I/O.
# Its HWND guard must precede all dialog mutations, not only the follow-up
# artist presentation callback. A window replacement must not receive stale
# history updates, search text or release results.
search_source = (root / "tasks.cpp").read_text(encoding="utf-8-sig")
search_header = (root / "tasks.h").read_text(encoding="utf-8-sig")
search_header_body = search_header.split("class search_artist_process_callback", 1)[1].split(
    "class expand_master_release_process_callback", 1)[0]
search_start = search_source.split("void search_artist_process_callback::start(", 1)[1].split(
    "void search_artist_process_callback::safe_run(", 1)[0]
search_main = search_source.split("void search_artist_process_callback::on_success(", 1)[1].split(
    "void search_artist_process_callback::on_abort(", 1)[0]
assert "HWND m_owner_window = nullptr;" in search_header_body
assert "m_owner_window = parent;" in search_start
assert "PFC_ASSERT(core_api::is_main_thread());" in search_main
assert search_main.count("::IsWindow(m_owner_window)") >= 2
assert search_main.count("g_discogs->find_release_dialog->m_hWnd != m_owner_window") >= 2
assert search_main.index("::IsWindow(m_owner_window)") < search_main.index("UpdateSearchQueryHasRun(")
assert "task->start(m_owner_window);" in search_main
assert "new service_impl_t<on_search_artist_done_process_callback>" in search_main
print("PASS: modeless online search completion refuses closed/replaced Win32 dialog.")

# All five original missing-release identities require a real native GUI
# path, not just API availability: Estiva has its first-search gate, while
# Kyau/Maywave, both Matt Fax and Quivver live in the 21-case matrix.
native_gui_script = (root / "scripts/host-discogger-gui.ps1").read_text(encoding="utf-8-sig")
assert "expected release 38012673" in native_gui_script
native_matrix_body = native_gui_script.split("$nativeMatrix=@(", 1)[1].split("\n        )", 1)[0]
for release_id in ("37332711", "36397609", "36819730", "35534398"):
    assert "Id='" + release_id + "'" in native_matrix_body
assert "Name='quivver-consciousness-ep';MinFields=3" in native_matrix_body
assert "foreach($item in $nativeMatrix)" in native_gui_script
# Every entry in the real 21-query Win32/x64 GUI matrix must pass the
# runner-only public release selector before probing: an unlisted fifth release
# previously threw before publishing telemetry, falsely resembling a timeout.
# Keep the 14-request API cohort unchanged and reject arbitrary selector IDs.
qa_native_source = (root / "tests/native-media-writer-qa.inc").read_text(encoding="utf-8-sig")
qa_corpus_source = (root / "tests/live-discogs-search-corpus-qa.h").read_text(encoding="utf-8-sig")
qa_selector = qa_native_source.split("void run_live_search_probe_qa() {", 1)[1].split(
    "const auto counts = g_discogs->find_release_dialog->runner_qa_live_search_probe(", 1
)[0]
matrix_ids = re.findall(r"Id='(\d+)'", native_matrix_body)
cohort_ids = set(re.findall(r"\{(\d+),", qa_corpus_source))
gui_only_ids = set(re.findall(r'expected_release_id == "(\d+)"', qa_selector))
assert len(matrix_ids) == 21
assert gui_only_ids == {"38012673", "35534398"}
assert set(matrix_ids).issubset(cohort_ids | gui_only_ids)
assert "if (!allowed)" in qa_selector
assert "Unrecognized public release QA selector." in qa_selector
print("PASS: all 21 native GUI release selectors are whitelisted; arbitrary IDs rejected.")

# Diagnosing a first-page miss must not falsely turn the immutable 14-case
# original API cohort into a 14/14 assertion, nor weaken the native GUI gates.
# Known historical 2026-10-25 bitmap 16381 excludes precisely corpus index 1:
# Above & Beyond feat. Richard Bedford, Sun & Moon, Discogs release 2783977.
corpus_release_ids = [int(v) for v in re.findall(r"\{(\d+),", qa_corpus_source)]
assert len(corpus_release_ids) == 14
assert [v for i, v in enumerate(corpus_release_ids) if not (16381 & (1 << i))] == [2783977]
assert "const auto evidence = probe_search(entry.encoded_search_params, entry.expected_release_id);" in qa_native_source
assert "cohort_known_mask |= (1u << i);" in qa_native_source
assert "result[3] = static_cast<unsigned>(page_count);" in qa_native_source
assert "evidence[3] >= 2 && cohort_page2_requests < 3" in qa_native_source
assert "page_two.compare(page_two.size() - suffix.size()," in qa_native_source
assert 'page_two.replace(page_two.size() - suffix.size(),' in qa_native_source
assert 'if (entry.expected_release_id == 2783977)' in qa_native_source
assert "artist=Above%20%26%20Beyond&release_title=Sun%20%26%20Moon&type=release&per_page=100&page=1" in qa_native_source
for marker in (
    "API_COHORT_PAGE2_ATTEMPT_MASK", "API_COHORT_PAGE2_OK_MASK",
    "API_COHORT_PAGE2_KNOWN_MASK", "API_COHORT_SUNMOON_TARGETED_OK",
    "API_COHORT_SUNMOON_TARGETED_KNOWN",
):
    assert f'"{marker}="' in qa_native_source
    assert f"'{marker}'" in native_gui_script
assert "DISCOGGER_AUTHENTICATED_API_COHORT_FOLLOWUP_" in native_gui_script
# A previous protected run failed because the PowerShell parser matched
# [A-Z_]+, excluding PAGE2 keys. Ensure numeric marker names are accepted,
# and only exact whitelisted nonnegative values can enter the QA counters.
assert "if($line -match '^([A-Z0-9_]+)=(\\d+)$' -and $Matches[1] -in $expected)" in native_gui_script
for qa_line in ("API_COHORT_PAGE2_ATTEMPT_MASK=2",
                "API_COHORT_PAGE2_KNOWN_MASK=2",
                "API_COHORT_KNOWN_MASK=16381"):
    parsed = re.fullmatch(r"([A-Z0-9_]+)=(\d+)", qa_line)
    assert parsed and parsed.group(1) in native_gui_script and int(parsed.group(2)) >= 0
assert re.fullmatch(r"([A-Z0-9_]+)=(\d+)", "API_COHORT_PAGE2_KNOWN_MASK=-1") is None

assert native_gui_script.count("DISCOGGER_AUTHENTICATED_API_COHORT_${Architecture}=") == 1
print("PASS: authenticated API cohort page-two/specificity diagnostics are bounded and separately reported.")

print("PASS: all five mandatory Discogs releases have native GUI search gates.")


# Release-tree navigation must fail closed if a Win32 TVM_GETITEM returns false.
# Otherwise zero-initialized TVITEM.lParam could be decoded as release row 0
# after cache replacement, selecting/tagging the wrong Discogs release.
release_tree = (root / "find_release_tree.cpp").read_text(encoding="utf-8-sig")
selected_reader = release_tree.split(
    "size_t CFindReleaseTree::test_getselected(", 1)[1].split(
    "size_t CFindReleaseTree::test_getatcursor(", 1)[0]
cursor_reader = release_tree.split(
    "size_t CFindReleaseTree::test_getatcursor(", 1)[1].split(
    "void CFindReleaseTree::hit_test_release_proceed(", 1)[0]
navigation = release_tree.split(
    "void CFindReleaseTree::hit_test_release_proceed(", 1)[1].split(
    "// default action for release tree items", 1)[0]
enter_key = release_tree.split(
    "void CFindReleaseTree::vkreturn_test_master_expand_release(", 1)[1].split(
    "// double click on release proceeds to match tracks", 1)[0]
initial_expand = release_tree.split("void CFindReleaseTree::OnInitExpand(", 1)[1]

def checked_treeview_item(body):
    guard = "if (!TreeView_GetItem(m_hwndTreeView, &item) || item.lParam == -1)"
    return (guard in body and
            body.index(guard) < body.index("out = item;") and
            "return ~size_t{0};" in body and
            "return static_cast<size_t>(item.lParam);" in body)

for tree_reader in (selected_reader, cursor_reader):
    assert checked_treeview_item(tree_reader)
    # Negative regression: an accidentally bypassed TVM_GETITEM failure guard
    # must not be accepted merely because the selection-return code remains.
    assert not checked_treeview_item(tree_reader.replace(
        "!TreeView_GetItem(m_hwndTreeView, &item)", "false", 1))
assert "if (!::ScreenToClient(m_hwndTreeView, &hit_info.pt))" in cursor_reader
assert "const size_t lparam = test_getatcursor(cursor, item, true);" in navigation
assert "if (lparam == ~size_t{0})" in navigation
assert "::GetCursorPos(&cursor)" in navigation
assert "if (!TreeView_GetItem(m_hwndTreeView, &tvi)) return;" in enter_key
assert "tvi.stateMask = TVIS_EXPANDED;" in enter_key
assert "if (TreeView_GetItem(m_hwndTreeView, &tvitem) && tvitem.lParam == lparam)" in initial_expand
assert "tvitem.mask = TVIF_PARAM;" in initial_expand
print("PASS: release TreeView failed/stale item retrieval cannot dispatch row zero or navigate invalid selection.")


# Owner-PC copied-media verification is a read-only tool, tested with synthetic
# temporary files on the Windows baseline runner (not the owner's PC).
private_audit = (root / "scripts/private-readonly-acceptance.ps1").read_text(encoding="utf-8-sig")
private_test = (root / "tests/private-readonly-acceptance-test.ps1").read_text(encoding="utf-8-sig")
validation = (root / ".github/workflows/validation.yml").read_text(encoding="utf-8-sig")
assert "Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256" in private_audit
assert "mtime_utc_ticks" in private_audit
assert "REFUSED: fixture must be a direct child" in private_audit
assert "REFUSED: fixture contains a junction or symbolic link." in private_audit
assert "Get-OrdinaryFiles $node.FullName" in private_audit
assert "REFUSED: baseline manifest already exists" in private_audit
# No check-then-truncate race: CreateNew fails atomically when a
# competing process creates a manifest or link at the target path.
assert "[IO.FileMode]::CreateNew" in private_audit
assert "[IO.FileShare]::None" in private_audit
assert "$stream.Flush($true)" in private_audit
assert "WriteAllText($manifest" not in private_audit
assert "REFUSED: baseline manifest must be an ordinary file, not a link." in private_audit
assert "WriteAllBytes" not in private_audit
assert "Remove-Item" not in private_audit
assert "Expect-Rejected" in private_test
assert "Rejected baseline attempt changed the original manifest." in private_test
assert "timestamp-only mutation" in private_test
assert "corrupt baseline must fail closed" in private_test
assert "run: ./tests/private-readonly-acceptance-test.ps1" in validation
print("PASS: owner copied-fixture read-only audit is SHA/mtime checked, fail-closed and Windows CI exercised.")

# The runner-only GUI/media DLL must never be confused with a privately
# acceptable clean component. The actual MSVC rebuild and both original
# foobar host loads are independent runtime gates in validation.yml.
clean_build = (root / "scripts/build-component-dll.ps1").read_text(encoding="utf-8-sig")
validation = (root / ".github/workflows/validation.yml").read_text(encoding="utf-8-sig")
for required in (
    "[switch] $CleanNonQa",
    "if ($CleanNonQa -and $env:CL -match 'DISCOGGER_RUNNER_MEDIA_QA')",
    "if ($CleanNonQa) { '/t:Rebuild' } else { '/t:Build' }",
    "if ($hash -eq $previousQaHash)",
    "'DISCOGGER_QA_RUNNER'",
    "'DISCOGGER_QA_MEDIA_DIR'",
    "'DISCOGGER_NATIVE_JSON_ROLLBACK_QA'",
    'CLEAN_NONQA_DLL_$platform=PASS',
):
    assert required in clean_build, required
qa_gui = validation.index("name: HARD GATE - Discogger first-run SQLite configuration and x64 native GUI")
clean_rebuild = validation.index("name: HARD GATE - clean non-QA Win32 x64 DLL rebuild")
clean_win32 = validation.index("name: HARD GATE - clean non-QA Win32 original foobar host load")
clean_x64 = validation.index("name: HARD GATE - clean non-QA x64 original foobar host load")
assert qa_gui < clean_rebuild < clean_win32 < clean_x64
assert "./scripts/build-component-dll.ps1 -CleanNonQa" in validation
assert "upload-artifact" not in validation
print("PASS: clean non-QA host acceptance follows real synthetic GUI QA, rebuilds both architectures and forbids runner-only markers/artifacts.")

# Same-HWND is not same-query: every modeless producer and the presentation
# callback must carry the identity of the accepted search, including direct
# ID and multi-artist paths. These are source-wiring contracts; real Win32/x64
# native GUI and portable/MSVC C++ regressions remain independent gates.
search_dialog_h = (root / "find_release_dialog.h").read_text(encoding="utf-8-sig")
search_dialog_cpp = (root / "find_release_dialog.cpp").read_text(encoding="utf-8-sig")
search_tasks_h = (root / "tasks.h").read_text(encoding="utf-8-sig")
search_tasks_cpp = (root / "tasks.cpp").read_text(encoding="utf-8-sig")
assert '#include "discogger_search_request.h"' in search_dialog_h
assert "m_search_request_epoch.begin();" in search_dialog_cpp
assert search_dialog_cpp.count("m_search_request_epoch.begin();") >= 5  # 3 routes + native QA
assert "on_search_artist_done_if_current(" in search_dialog_h
assert "DISCOGGER_NATIVE_STALE_SEARCH_QA=PASS" in search_dialog_cpp
assert "on_search_artist_done_if_current(obsolete" in search_dialog_cpp
assert "on_search_artist_done_if_current(latest" in search_dialog_cpp
for producer in ("get_artist_process_callback", "get_multi_artists_process_callback",
                 "search_artist_process_callback", "on_get_artist_done_process_callback"):
    assert f"void {producer}::start(HWND parent)" in search_tasks_cpp
    assert "m_search_ticket" in search_tasks_h.split(f"class {producer}", 1)[1].split("\nclass ", 1)[0]
for callback in ("get_artist_process_callback::on_success",
                 "get_multi_artists_process_callback::on_success",
                 "get_multi_artists_process_callback::on_error",
                 "search_artist_process_callback::on_success",
                 "on_search_artist_done_process_callback::on_success",
                 "on_get_artist_done_process_callback::on_success"):
    block = search_tasks_cpp.split(f"void {callback}(", 1)[1].split("\nvoid ", 1)[0]
    assert "is_current_search_request(m_search_ticket)" in block, callback
assert "res_cap, m_search_ticket);" in search_tasks_cpp
assert "on_search_artist_done_if_current(m_search_ticket" in search_tasks_cpp
for test_runner in (".github/workflows/validation.yml", "scripts/run-msvc-unit-tests.ps1"):
    assert "search_request_epoch_test.cpp" in (root / test_runner).read_text(encoding="utf-8-sig")
print("PASS: same-window asynchronous search identity across native artist, multi-artist, direct-ID, and presentation paths.")

# Release-detail processing is another modeless UI request, not just an
# artist-search result. The worker may fetch metadata but must never hide a
# Win32 dialog or present a stale release after a new search/selection.
release_route = search_dialog_cpp.split("void CFindReleaseDialog::trigger_release(", 1)[1].split(
    "\nvoid CFindReleaseDialog::on_expand_master_release_done(", 1)[0]
assert "m_search_request_epoch.begin();" in release_route
assert release_route.index("m_search_request_epoch.begin();") < release_route.index("task->start(m_hWnd);")
release_class = search_tasks_h.split("class process_release_callback", 1)[1].split(
    "\nclass ", 1)[0]
for required in ("m_owner_window", "m_search_ticket", "owner_is_current() const"):
    assert required in release_class, required
release_start = search_tasks_cpp.split("void process_release_callback::start(", 1)[1].split(
    "\nbool process_release_callback::owner_is_current(", 1)[0]
assert "m_owner_window = parent;" in release_start
assert "m_search_ticket = m_dialog->current_search_request();" in release_start
release_owner = search_tasks_cpp.split("bool process_release_callback::owner_is_current(", 1)[1].split(
    "\nvoid process_release_callback::safe_run(", 1)[0]
for required in ("core_api::is_main_thread()", "::IsWindow(m_owner_window)",
                 "g_discogs->find_release_dialog == m_dialog",
                 "m_dialog->m_hWnd == m_owner_window",
                 "is_current_search_request(m_search_ticket)"):
    assert required in release_owner, required
release_worker = search_tasks_cpp.split("void process_release_callback::safe_run(", 1)[1].split(
    "\nvoid process_release_callback::generate_track_mapping(", 1)[0]
assert "m_dialog->hide()" not in release_worker
assert "m_dialog->show()" not in release_worker
release_success = search_tasks_cpp.split("void process_release_callback::on_success(", 1)[1].split(
    "\nvoid process_release_callback::on_abort(", 1)[0]
assert release_success.count("owner_is_current()") >= 2
assert release_success.index("owner_is_current()") < release_success.index("m_dialog->hide()")
assert release_success.index("m_dialog->hide()") < release_success.index("fb2k::newDialog<CTrackMatchingDialog>")
for callback in ("on_abort", "on_error"):
    block = search_tasks_cpp.split(f"void process_release_callback::{callback}(", 1)[1].split(
        "\n}", 1)[0]
    assert "if (owner_is_current()) m_dialog->show();" in block
print("PASS: release detail UI-thread ownership, stale selection/abort denial and same-HWND supersession are wired.")

# Async GUI completion must reject stale master expansions, rapid filter
# edits (including A->B->A) and stale aborts, not merely compare HWND/text.
filter_dialog = (root / "find_release_dialog.cpp").read_text(encoding="utf-8-sig")
filter_header = (root / "find_release_dialog.h").read_text(encoding="utf-8-sig")
filter_tasks = (root / "tasks.cpp").read_text(encoding="utf-8-sig")
filter_task_header = (root / "tasks.h").read_text(encoding="utf-8-sig")
assert "m_filter_request_epoch.begin();" in filter_dialog
assert "is_current_filter_request(std::uint64_t ticket)" in filter_header
assert "m_filter_request_epoch" in filter_header
for name, end in (("expand_master_release_process_callback", "tree_apply_filter_process_callback"),
                  ("tree_apply_filter_process_callback", "on_search_artist_done_process_callback")):
    block = filter_tasks.split("void " + name + "::start(", 1)[1].split("void " + end + "::start(", 1)[0]
    assert "current_search_request()" in block
    assert "is_current_search_request(m_search_ticket)" in block
filter_block = filter_tasks.split("void tree_apply_filter_process_callback::start(", 1)[1].split(
    "void on_search_artist_done_process_callback::start(", 1)[0]
assert "current_filter_request()" in filter_block
assert filter_block.count("is_current_filter_request(m_filter_ticket)") >= 2
assert "std::uint64_t m_filter_ticket = 0;" in filter_task_header
assert "Native filter generation failed on live foobar controls." in filter_dialog
assert "Native master expansion generation was not superseded." in filter_dialog
artist_worker = filter_tasks.split("void get_artist_process_callback::safe_run(", 1)[1].split(
    "void get_artist_process_callback::on_success(", 1)[0]
assert "if (!m_artist)" in artist_worker
artist_completion = filter_tasks.split("void get_artist_process_callback::on_success(", 1)[1].split(
    "void get_artist_process_callback::on_abort(", 1)[0]
assert artist_completion.count("is_current_search_request(m_search_ticket)") >= 3
# Master HTTP completions must also be tied to the filter generation and
# original row identity; an unchanged search HWND is not sufficient.
assert "m_filter_ticket = dialog->current_filter_request();" in expand_parts
assert "m_filter_text = trim(uGetWindowText(dialog->m_edit_filter));" in expand_parts
assert "is_current_filter_request(m_filter_ticket)" in expand_foreground
assert "GetSearchMode() != static_cast<size_t>(m_query_mode)" in expand_foreground
assert "uGetWindowText(g_discogs->find_release_dialog->m_edit_filter)" in expand_foreground
assert "std::uint64_t m_filter_ticket = 0;" in expand_header
assert "pfc::string8 m_filter_text;" in expand_header
assert "mounted_param selected_row(original_rows->at(" in master_completion
assert "selected_row.is_master()" in master_completion
assert "m_find_release_artist->master_releases[selected_row.master_ndx] != master_release" in master_completion
assert master_completion.index("selected_row.is_master()") < master_completion.index("set_node_expanded(")
assert "myparam.master_ndx != static_cast<size_t>(master_i)" in master_completion
assert "DISCOGGER_NATIVE_MASTER_FILTER_SUPERSESSION_QA=PASS" in filter_dialog
print("PASS: master expansion filter generation, row identity, and live native supersession.")
print("PASS: async artist/master/filter UI lifecycle and stale callback gates.")
