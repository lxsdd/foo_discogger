#!/usr/bin/env python3
"""Validate all named HTML entities against Python's canonical HTML5 UTF-8."""
from html.entities import html5
from pathlib import Path
import re

source = (Path(__file__).resolve().parents[1] / "entities.cpp").read_text(encoding="utf-8")
entry = re.compile(r'^\s*\{"([A-Za-z0-9]+;)", "((?:\\.|[^"\\])*)"\},?\s*$', re.M)
matches = entry.findall(source)
assert len(matches) == 253, f"Expected 253 complete mappings, got {len(matches)}"
names = [name for name, _ in matches]
assert names == sorted(set(names)), "bsearch requires sorted, unique keys"

def cpp_bytes(literal):
    result = bytearray()
    i = 0
    while i < len(literal):
        if literal[i] == "\\":
            if literal[i:i+2] == "\\x":
                digits = literal[i+2:i+4]
                assert re.fullmatch(r"[0-9A-Fa-f]{2}", digits), repr(literal)
                result.append(int(digits, 16))
                i += 4
            else:
                assert literal[i+1:i+2] in ('"', "\\"), repr(literal)
                result.append(ord(literal[i+1]))
                i += 2
        else:
            assert ord(literal[i]) < 128, "No locale-dependent source characters allowed"
            result.append(ord(literal[i]))
            i += 1
    return bytes(result)

for name, literal in matches:
    expected = html5[name].encode("utf-8")
    actual = cpp_bytes(literal)
    assert actual == expected, f"{name}: expected {expected!r}, got {actual!r}"

assert '#include "discogger_html_numeric.h"' in source
assert "valid_numeric_digits(digits, end, hex)" in source
assert "valid_numeric_scalar(cp)" in source
assert "cp > UNICODE_MAX" not in source
print("PASS: 253 named entities have canonical UTF-8 bytes; malformed numeric references rejected.")
