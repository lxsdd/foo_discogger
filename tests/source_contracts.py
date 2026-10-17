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

print("PASS: refresh selection and HTTP rate-limit error routing are wired.")
print("NOTE: full component compilation and live Discogs/foobar testing are separate gates.")
