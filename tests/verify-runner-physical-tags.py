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


def _syncsafe_size(value):
    if len(value) != 4 or any(byte & 0x80 for byte in value):
        raise AssertionError("Invalid ID3v2 synchsafe header")
    return (value[0] << 21) | (value[1] << 14) | (value[2] << 7) | value[3]


def id3_txxx_values(content):
    """Read actual TXXX descriptions/values, including UTF-16 ID3v2 frames."""
    if content[:3] != b"ID3" or len(content) < 10:
        raise AssertionError("ID3v2 header absent")
    version = content[3]
    if version not in (3, 4):
        raise AssertionError("Unsupported synthetic fixture ID3v2 version")
    end = 10 + _syncsafe_size(content[6:10])
    if end > len(content):
        raise AssertionError("Truncated ID3v2 tag")
    offset = 10
    if content[5] & 0x40:
        if offset + 4 > end:
            raise AssertionError("Truncated extended ID3v2 header")
        size = (int.from_bytes(content[offset:offset + 4], "big")
                if version == 3 else _syncsafe_size(content[offset:offset + 4]))
        offset += 4 + size if version == 3 else size
    decoded = []
    while offset + 10 <= end:
        frame_id = content[offset:offset + 4]
        if frame_id == bytes(4): break  # padding
        if not all((65 <= ch <= 90 or 48 <= ch <= 57) for ch in frame_id):
            raise AssertionError("Invalid ID3v2 frame identifier")
        frame_size = (int.from_bytes(content[offset + 4:offset + 8], "big")
                      if version == 3 else _syncsafe_size(content[offset + 4:offset + 8]))
        offset += 10
        if frame_size < 1 or offset + frame_size > end:
            raise AssertionError("Malformed ID3v2 frame length")
        frame = content[offset:offset + frame_size]
        offset += frame_size
        if frame_id != b"TXXX":
            continue
        enc, raw = frame[0], frame[1:]
        codecs = {0: "latin-1", 1: "utf-16", 2: "utf-16-be", 3: "utf-8"}
        if enc not in codecs:
            raise AssertionError("Unsupported TXXX text encoding")
        codec = codecs[enc]
        if enc == 1 and not (raw.startswith(b"\xff\xfe") or raw.startswith(b"\xfe\xff")):
            codec = "utf-16-le"
        try:
            payload = raw.decode(codec).strip("\x00")
        except UnicodeError as error:
            raise AssertionError("Corrupt Unicode TXXX frame") from error
        if "\x00" in payload:
            name, value = payload.split("\x00", 1)
            decoded.append((name.strip(), value.strip("\x00")))
    return decoded

def verify(before: bytes, after: bytes, suffix):
    if before == after:
        raise AssertionError("requested physical tag update was a no-op")
    if suffix == ".mp3":
        if not before.startswith(b"ID3") or not after.startswith(b"ID3"):
            raise AssertionError("ID3v2 header lost")
        for item in (b"Synthetic Test", b"Discogger QA", b"Trance"):
            if item not in after:
                raise AssertionError(f"Original MP3 metadata missing after writer: {item!r}")
        tags = id3_txxx_values(after)
        if ("DISCOGGER_QA_RUNNER", "RUNNER-VERIFIED") not in tags:
            raise AssertionError("Real foobar MP3 writer did not persist requested TXXX tag")
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
    if suffix == ".flac" and (b"RUNNER-VERIFIED" not in after or
                              b"DISCOGGER_QA_RUNNER" not in after):
        raise AssertionError("Real foobar FLAC writer did not serialize QA Vorbis comment")


def selftest_id3():
    def ss(n):
        return bytes(((n >> 21) & 0x7f, (n >> 14) & 0x7f, (n >> 7) & 0x7f, n & 0x7f))
    for version in (3, 4):
        for enc, value in ((0, b"DISCOGGER_QA_RUNNER\x00RUNNER-VERIFIED"),
                           (1, "DISCOGGER_QA_RUNNER\x00RUNNER-VERIFIED".encode("utf-16")),
                           (3, b"DISCOGGER_QA_RUNNER\x00RUNNER-VERIFIED")):
            body = bytes((enc,)) + value
            size = len(body).to_bytes(4, "big") if version == 3 else ss(len(body))
            frame = b"TXXX" + size + bytes(2) + body
            fake = b"ID3" + bytes((version, 0, 0)) + ss(len(frame)) + frame
            assert id3_txxx_values(fake) == [("DISCOGGER_QA_RUNNER", "RUNNER-VERIFIED")]
    print("DISCOGGER_ID3_TXXX_UNICODE_VERIFIER=PASS")

if __name__ == "__main__":
    if len(sys.argv) == 2 and sys.argv[1] == "--selftest":
        selftest_id3()
        raise SystemExit(0)
    if len(sys.argv) != 3:
        raise SystemExit("Expected --selftest or original-copy and disposable updated file")
    original, updated = map(Path, sys.argv[1:])
    suffix = ".mp3" if "mp3" in original.name else ".flac"
    verify(original.read_bytes(), updated.read_bytes(), suffix)
    print("DISCOGGER_RUNNER_PHYSICAL_TAG_PRESERVATION=PASS")
