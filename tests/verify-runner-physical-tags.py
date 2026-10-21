#!/usr/bin/env python3
"""Verify real runner-only physical writes and preserved CUE metadata."""
from pathlib import Path
import sys

def flac_blocks(content):
    if content[:4] != b"fLaC":
        raise AssertionError("FLAC magic absent")
    offset = 4
    output = {}
    while True:
        if offset + 4 > len(content):
            raise AssertionError("truncated FLAC block")
        kind = content[offset] & 127
        last = content[offset] & 128
        size = int.from_bytes(content[offset+1:offset+4], "big")
        value = content[offset+4:offset+4+size]
        if len(value) != size:
            raise AssertionError("incomplete FLAC block")
        output.setdefault(kind, []).append(value)
        offset += 4 + size
        if last: return output

def verify(before: bytes, after: bytes, suffix):
    if before == after:
        raise AssertionError("requested physical tag update was a no-op")
    if suffix == ".mp3":
        if not before.startswith(b"ID3") or not after.startswith(b"ID3"):
            raise AssertionError("ID3v2 header lost")
        for item in (b"Synthetic Test", b"Discogger QA", b"Trance"):
            if item not in after:
                raise AssertionError(f"Original MP3 metadata missing after writer: {item!r}")
    elif suffix == ".flac":
        first, second = flac_blocks(before), flac_blocks(after)
        if first.get(5) != second.get(5):
            raise AssertionError("Native FLAC Type-5 CUESHEET changed")
        if not first.get(4) or not second.get(4):
            raise AssertionError("Vorbis comments missing")
        before_cue = [x for x in first[4][0].split(b"CUESHEET=")][1:]
        after_cue = [x for x in second[4][0].split(b"CUESHEET=")][1:]
        if not before_cue or not after_cue:
            raise AssertionError("Embedded textual CUESHEET missing")
        for token in (b"Runner Embedded EP", b"Progressive Trance", b"Preserve me"):
            if token not in second[4][0]:
                raise AssertionError(f"Custom FLAC tag or CUE marker missing: {token!r}")
    else:
        raise AssertionError("Unknown media extension")
    if b"RUNNER-VERIFIED" not in after or b"DISCOGGER_QA_RUNNER" not in after:
        raise AssertionError("Real foobar physical tag writer did not serialize QA tag")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("Expected original-copy and disposable updated file")
    original, updated = map(Path, sys.argv[1:])
    suffix = ".mp3" if "mp3" in original.name else ".flac"
    verify(original.read_bytes(), updated.read_bytes(), suffix)
    print("DISCOGGER_RUNNER_PHYSICAL_TAG_PRESERVATION=PASS")
