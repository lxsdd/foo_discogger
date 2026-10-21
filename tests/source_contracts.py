#!/usr/bin/env python3
"""Static wiring/regression guards. They do not replace a foobar2000 runtime test."""
from pathlib import Path

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
assert "|| explicit_refresh" in artist_load
assert artist_load.count("release_refresh.retry_after_failure(explicit_refresh);") == 3
tasks_callbacks = (root / "tasks.cpp").read_text(encoding="utf-8-sig")
assert "m_artist->release_refresh.pending()" in tasks_callbacks
assert "dlg->m_alist.Get_Selected_Id().equals(m_artist->id)" in tasks_callbacks
for name in (".github/workflows/validation.yml", "scripts/run-msvc-unit-tests.ps1"):
    assert "artist_refresh_test.cpp" in (root / name).read_text(encoding="utf-8-sig")
assert "if (need_data || cupdsrc == updRelSrc::UndefFast || cupdsrc == updRelSrc::ArtistList)" in dialog
assert "discogger_net::parse_http_status_code(status.c_str())" in fetcher
assert "throw http_429_exception();" in fetcher
assert "error_code = 4011" not in fetcher

search = (root / "discogs_interface.cpp").read_text(encoding="utf-8-sig")
assert "inline std::string trailing_collaborator(" in (root / "discogger_search_fallback.h").read_text(encoding="utf-8-sig")
assert "discogger_search::build_fallback_plan(" in search
assert "for (const auto& candidate : plans)" in search
assert "get_all_pages(fallback_url, fallback_params" in search
assert "search_query::TextToMap(fallback_expression, fallback_map)" in search
assert "if (fallback_max > max_to_abort)" in search
assert 'presentation_query["artist="].first.clear();' in search
assert "used_broad_fallback = true;" in search
assert "p_abort.check();" in search
parsed_releases = (root / "discogs.cpp").read_text(encoding="utf-8-sig")
release_section = parsed_releases[parsed_releases.index("void Discogs::parseArtistReleases("):
                                  parsed_releases.index("void Discogs::parseMasterVersions(")]
assert release_section.count("if (!present)") >= 2
plan = (root / "discogger_search_fallback.h").read_text(encoding="utf-8-sig")
assert "without_terminal_mix_suffix" in plan
assert "build_fallback_plan(" in plan
assert "if (key.size() < 3) return plan;" in plan
assert "title-only discovery (review required)" in plan
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
print("PASS: callback-owned metadata lifetime, single-shot async failure reset and synchronous scheduling cleanup.")

# Safe disk-marker TTL, independent artist freshness and legacy config migration.
cache = (root / "ol_cache.h").read_text(encoding="utf-8-sig")
config = (root / "configuration_dialog.cpp").read_text(encoding="utf-8-sig")
conf_code = (root / "conf.cpp").read_text(encoding="utf-8-sig")
assert "fs::last_write_time(complete, ec)" in cache
assert "check_download(out_relative_path, expired, gfFrom)" in cache
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
assert "json_t* root = nullptr;" in cache_read
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
assert "json_decref(root);" in cache_atomic
assert "atomic_cache_file_test.cpp" in (root / "scripts/run-msvc-unit-tests.ps1").read_text(encoding="utf-8-sig")
print("PASS: atomic JSON cache publication and non-destructive parse failure.")


# Full cache directory publication uses isolated staging, a completed marker,
# two-phase rename, recovery, and negative portable/MSVC tests.
txn = (root / "discogger_directory_cache.h").read_text(encoding="utf-8-sig")
assert "recover_cache_unlocked(active_)" in txn
assert "fs::rename(active_, backup_, ec);" in txn
assert "fs::rename(stage_, active_, ec);" in txn
assert "fs::rename(backup_, active_, restore_ec);" in txn
assert "cache_has_marker(stage_)" in txn
assert "std::unique_lock<std::mutex> lock_" in txn
assert "directory_cache_transaction_test.cpp" in (root / "scripts/run-msvc-unit-tests.ps1").read_text(encoding="utf-8-sig")
core = (root / "discogs.cpp").read_text(encoding="utf-8-sig")
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
shared_action = "lxsdd/dev-infrastructure-public/.github/actions/foobar-host-smoke@99cc4b8c31d4ac8e640da0c375675566dc44762e"
assert host_wf.count("uses: " + shared_action) == 2
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
assert "myparam.master_ndx >= m_find_release_artist->master_releases.get_count()" in tree_source
assert "test_getatcursor(p, tvi, true)" in tree_source
assert "(tvhitinfo.flags & TVHT_ONITEM)" in tree_source
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
