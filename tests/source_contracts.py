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
