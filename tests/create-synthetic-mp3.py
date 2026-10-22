#!/usr/bin/env python3
"""Original silent ID3v2 MP3 fixture for isolated Windows host tests.

Compressed deterministic synthetic audio created with FFmpeg libmp3lame;
no user music, codec download or installer needed at CI runtime.
"""
import base64
import hashlib
from pathlib import Path
import sys
import zlib

PAYLOAD = (
    "eNrzdDFmZgCBshDPECMgLQDEwZV5JRmpJZnJCiGpxSUMIQGuhkBRPiB2ySxOzk9PTy1SCHRkCHH29wOKcQBxSFFiXnIqQ0ikaxCQxwbERgZGZgwhwcGuUJ0+iWVpZoZ65nqGBsYMCPD/scUBOMczLy0fSPEDMRfQlAoGcxjwhYMUOKiCgwlwsAwO9sLBZTh4CQf/4YAB4rZkoNsMLZEcxqDCLABhsFXoeWzXZCAN/H+scQRIMXuAzXf0dQ0NDQVRxkD/G4TSH4DcUwNzD1Rs1D2j7hl1z6h7Bso9AwkAZEgrSQ=="
)
EXPECTED_SHA = "b430229832fb8a1e7824716b1fefb34e5706ea89593dd73ddf7133c17ae8bec0"


def synthetic_png():
    """Deterministic one-pixel PNG generated with stdlib only (not third-party art)."""
    import struct
    def chunk(kind, payload):
        data = kind + payload
        return (struct.pack(">I", len(payload)) + data +
                struct.pack(">I", zlib.crc32(data) & 0xffffffff))
    header = struct.pack(">IIBBBBB", 1, 1, 8, 2, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) +
            chunk(b"IDAT", zlib.compress(b"\x00\x40\x80\xc0")) +
            chunk(b"IEND", b""))


def with_synthetic_apic(raw):
    if raw[:3] != b"ID3" or raw[3] not in (3, 4) or raw[5] & 0x40:
        raise ValueError("Unexpected original MP3 ID3v2 header")
    def syncsafe(n):
        if n >= (1 << 28): raise ValueError("ID3v2 size limit exceeded")
        return bytes(((n >> 21) & 127, (n >> 14) & 127, (n >> 7) & 127, n & 127))
    old_length = ((raw[6] << 21) | (raw[7] << 14) | (raw[8] << 7) | raw[9])
    if old_length > len(raw) - 10: raise ValueError("Truncated original MP3 ID3v2 tag")
    payload = b"\x00image/png\x00\x03\x00" + synthetic_png()
    frame_size = (len(payload).to_bytes(4, "big") if raw[3] == 3 else syncsafe(len(payload)))
    frame = b"APIC" + frame_size + bytes(2) + payload
    # Original physical custom ID3v2 fields plus ReplayGain must survive an
    # unrelated metadata write. These are synthetic, disposable test values.
    for name, value in (
        ("DISCOGGER_QA_CUSTOM", "Keep original MP3 metadata"),
        ("REPLAYGAIN_TRACK_GAIN", "-5.00 dB"),
        ("REPLAYGAIN_TRACK_PEAK", "0.800000"),
    ):
        text = bytes((3,)) + name.encode("utf-8") + bytes(1) + value.encode("utf-8")
        size = len(text).to_bytes(4, "big") if raw[3] == 3 else syncsafe(len(text))
        frame += b"TXXX" + size + bytes(2) + text
    return (raw[:6] + syncsafe(old_length + len(frame)) + frame +
            raw[10:10 + old_length] + raw[10 + old_length:])

def create(path: Path):
    if path.exists() or not path.parent.is_dir():
        raise ValueError("Refuse overwrite or create unexpected directories")
    raw = zlib.decompress(base64.b64decode(PAYLOAD, validate=True))
    if len(raw) != 1784 or raw[:3] != b"ID3" or hashlib.sha256(raw).hexdigest() != EXPECTED_SHA:
        raise ValueError("Original synthetic MP3 fixture integrity failure")
    enriched = with_synthetic_apic(raw)
    if not enriched.startswith(b"ID3") or len(enriched) <= len(raw):
        raise ValueError("MP3 physical artwork fixture not generated")
    path.write_bytes(enriched)
    print("DISCOGGER_SYNTHETIC_MP3_FIXTURE=PASS (stdlib-generated APIC cover)")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Expected one new throwaway fixture path")
    create(Path(sys.argv[1]))
