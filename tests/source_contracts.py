#!/usr/bin/env python3
"""Static wiring/regression guards. They do not replace a foobar2000 runtime test."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
artist = (root / "find_artist_list.cpp").read_text(encoding="utf-8-sig")
dialog = (root / "find_release_dialog.cpp").read_text(encoding="utf-8-sig")
fetcher = (root / "fetcher.cpp").read_text(encoding="utf-8-sig")

assert '"&Refresh artist releases"' in artist
assert "discogs_interface->delete_artist_cache(artist->id)" in artist
assert "dlg->convey_artist_list_selection(refresh);" in artist
assert "artist->loaded_releases = false;" in artist
assert "if (need_data || cupdsrc == updRelSrc::UndefFast || cupdsrc == updRelSrc::ArtistList)" in dialog
assert "discogger_net::parse_http_status_code(status.c_str())" in fetcher
assert "throw http_429_exception();" in fetcher
assert "error_code = 4011" not in fetcher

search = (root / "discogs_interface.cpp").read_text(encoding="utf-8-sig")
assert "discogger_search::trailing_collaborator(original_artist)" in search
assert "get_all_pages(fallback_url, fallback_params" in search
assert "search_query::TextToMap(fallback_expression, fallback_map)" in search
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
assert tasks.index("discogger_write_safety::contains_virtual_subsong") < tasks.index("m_tag_writer->write_tags();")
assert manager.index("discogger_write_safety::contains_virtual_subsong") < manager.index("api->update_info_async_simple(")
assert "Writing tags failed; dependent artwork was not started." in manager
assert "display_errors();" in manager[manager.index("void write_tags_completion::on_completion"):manager.index("void read_tags_completion::on_completion")]
print("PASS: fail-closed virtual subsong preflight and async write failure notification are wired.")

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
assert "m_gate.finish(success" in manager
assert "m_tag_writer->m_finfo_manager->write_infos(std::move(artwork_after_tags))" in tasks
assert "artwork_after_tags = [release_id, physical_items, bfile_match]" in tasks
assert tasks.index("artwork_after_tags = [release_id") < tasks.index("m_tag_writer->m_finfo_manager->write_infos(std::move(artwork_after_tags))")
assert "new service_impl_t<download_art_task>" in tasks[tasks.index("artwork_after_tags = [release_id"):tasks.index("m_tag_writer->m_finfo_manager->write_infos(std::move(artwork_after_tags))")]
print("PASS: dependent artwork is gated by asynchronous physical tag completion; positive/negative portable gate tests are separate.")

# Real physical metadata no-op: compare the pre-edit snapshot including ReplayGain.
# Artwork no-op and actual host byte/timestamp preservation remain separate gates.
manager_header = (root / "file_info_manager.h").read_text(encoding="utf-8-sig")
writer = (root / "tag_writer.cpp").read_text(encoding="utf-8-sig")
assert "m_finfo_manager->invalidate_all();" in writer
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
assert manager.index("discogger_write_safety::plan_snapshot_batch(") < manager.index("items_to_update.set_size(count)")
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
assert "class write_tags_task : public foo_discogs_write_tag_locked_threaded_process_callback" in task_header
print("PASS: search/filter lock base and mutex-guarded counter transitions are wired.")
