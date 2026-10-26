#!/usr/bin/env python3
"""Enrich only a brand-new ephemeral GitHub synthetic FLAC with picture/metadata."""
from pathlib import Path
import os
import struct
import sys
import zlib

def png():
    def block(kind, value):
        pair = kind + value
        return len(value).to_bytes(4, "big") + pair + (zlib.crc32(pair) & 0xffffffff).to_bytes(4, "big")
    ihdr = struct.pack(">IIBBBBB", 1, 1, 8, 2, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + block(b"IHDR", ihdr)
            + block(b"IDAT", zlib.compress(b"\x00\x40\x80\xc0")) + block(b"IEND", b""))

def enrich(path):
    if os.getenv("GITHUB_ACTIONS") != "true" or not os.getenv("RUNNER_TEMP"):
        raise RuntimeError("Rich FLAC fixture is only permitted in disposable Actions runners")
    target = Path(path)
    if target.is_symlink() or not target.is_file():
        raise RuntimeError("Synthetic FLAC must be a regular existing file, never a link")
    root = Path(os.environ["RUNNER_TEMP"]).resolve(strict=True)
    if root not in target.resolve(strict=True).parents:
        raise RuntimeError("Refuse to change FLAC outside Actions RUNNER_TEMP")
    original = target.read_bytes()
    if original[:4] != b"fLaC" or len(original) > 4096:
        raise RuntimeError("Invalid small generated FLAC source")
    blocks, offset = [], 4
    while True:
        if offset + 4 > len(original):
            raise RuntimeError("Invalid FLAC metadata")
        flags = original[offset]
        size = int.from_bytes(original[offset + 1:offset + 4], "big")
        content = original[offset + 4:offset + 4 + size]
        if len(content) != size: raise RuntimeError("Truncated FLAC source metadata")
        blocks.append((flags & 127, content))
        offset += 4 + size
        if flags & 128: break
    if [k for k, _ in blocks] != [0, 5, 4]:
        raise RuntimeError("Unexpected original FLAC metadata block layout")
    vendor = blocks[-1][1]
    vendor_length = int.from_bytes(vendor[:4], "little")
    comment_pos = 4 + vendor_length
    if len(vendor) < comment_pos + 4 or int.from_bytes(vendor[comment_pos:comment_pos + 4], "little") != 1:
        raise RuntimeError("Unexpected source Vorbis comment structure")
    def entry(value): return len(value).to_bytes(4, "little") + value
    extras = [
        b"GENRE=House", b"GENRE=Progressive Trance",
        b"REPLAYGAIN_TRACK_GAIN=-5.00 dB",
        b"REPLAYGAIN_TRACK_PEAK=0.800000",
        b"DISCOGGER_QA_CUSTOM=Keep me",
    ]
    newcomments = (vendor[:comment_pos] + (1 + len(extras)).to_bytes(4, "little")
                   + vendor[comment_pos + 4:] + b"".join(map(entry, extras)))
    img = png()
    image = (struct.pack(">I", 3) + struct.pack(">I", 9) + b"image/png"
             + struct.pack(">I", 0) + struct.pack(">IIIII", 1, 1, 24, 0, len(img)) + img)
    final = [blocks[0], blocks[1], (6, image), (4, newcomments)]
    metadata = bytearray()
    for idx, (kind, value) in enumerate(final):
        metadata.extend(bytes((kind | (128 if idx == len(final) - 1 else 0),)))
        metadata.extend(len(value).to_bytes(3, "big"))
        metadata.extend(value)
    result = b"fLaC" + metadata + original[offset:]
    if result[-len(original[offset:]):] != original[offset:] or len(result) > 8192:
        raise RuntimeError("Synthetic FLAC audio payload changed")
    target.write_bytes(result)
    print("DISCOGGER_SYNTHETIC_FLAC_PICTURE_MULTIVALUE=PASS")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Expected one ephemeral runner-local FLAC fixture")
    enrich(sys.argv[1])
