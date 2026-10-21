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

def create(path: Path):
    if path.exists() or not path.parent.is_dir():
        raise ValueError("Refuse overwrite or create unexpected directories")
    raw = zlib.decompress(base64.b64decode(PAYLOAD, validate=True))
    if len(raw) != 1784 or raw[:3] != b"ID3" or hashlib.sha256(raw).hexdigest() != EXPECTED_SHA:
        raise ValueError("Original synthetic MP3 fixture integrity failure")
    path.write_bytes(raw)
    print("DISCOGGER_SYNTHETIC_MP3_FIXTURE=PASS")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Expected one new throwaway fixture path")
    create(Path(sys.argv[1]))
