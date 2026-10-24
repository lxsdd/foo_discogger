#!/usr/bin/env python3
"""Verify real runner-only physical writes and preserved CUE metadata."""
from pathlib import Path
from collections import Counter
import sys

def flac_blocks(content):
    if content[:4] != b"fLaC":
        raise AssertionError("FLAC magic absent")
    offset = 4
    output = {}
    block_index = 0
    while True:
        if offset + 4 > len(content):
            raise AssertionError("truncated FLAC block")
        kind = content[offset] & 127
        last = content[offset] & 128
        size = int.from_bytes(content[offset+1:offset+4], "big")
        # STREAMINFO is mandatory, exactly once, first, and exactly 34 bytes.
        # Comparing only block multisets would accept a writer that reordered
        # STREAMINFO behind comments, making an otherwise intact FLAC invalid.
        if kind == 127:
            raise AssertionError("Forbidden FLAC metadata block type 127")
        if block_index == 0 and (kind != 0 or size != 34):
            raise AssertionError("Missing/invalid first FLAC STREAMINFO")
        if kind == 0 and (block_index != 0 or size != 34):
            raise AssertionError("Duplicate/invalid FLAC STREAMINFO")
        value = content[offset+4:offset+4+size]
        if len(value) != size:
            raise AssertionError("incomplete FLAC block")
        output.setdefault(kind, []).append(value)
        offset += 4 + size
        block_index += 1
        if last: return output



def flac_vorbis_fields(block):
    """Decode exact FLAC Vorbis-comment field names, not raw substrings.

    In particular, CUE_TRACK01_DISCOGGER_QA_RUNNER is NOT the same field as
    physical DISCOGGER_QA_RUNNER; a byte-string grep falsely accepts both.
    """
    offset = 0
    def read_u32():
        nonlocal offset
        if offset + 4 > len(block):
            raise AssertionError("Truncated FLAC Vorbis field length")
        length = int.from_bytes(block[offset:offset + 4], "little")
        offset += 4
        return length
    def read_bytes(length):
        nonlocal offset
        if length > len(block) - offset:
            raise AssertionError("Truncated FLAC Vorbis comment")
        data = block[offset:offset + length]
        offset += length
        return data

    read_bytes(read_u32())  # vendor string, never displayed or logged
    count = read_u32()
    if count > (len(block) - offset) // 4:
        raise AssertionError("Invalid FLAC Vorbis comment count")
    fields = {}
    for _ in range(count):
        data = read_bytes(read_u32())
        if b"=" not in data:
            raise AssertionError("Invalid FLAC Vorbis comment key/value")
        name, value = data.split(b"=", 1)
        try:
            key = name.decode("ascii").upper()
        except UnicodeError as error:
            raise AssertionError("Invalid FLAC Vorbis tag name") from error
        if not key:
            raise AssertionError("Empty FLAC Vorbis tag key")
        fields.setdefault(key, []).append(value)
    if offset != len(block):
        raise AssertionError("Unexpected FLAC Vorbis trailing bytes")
    return fields


def single_flac_vorbis_fields(blocks):
    """Reject duplicate/missing physical FLAC Vorbis-comment metadata blocks.

    Comparing only blocks[4][0] can falsely PASS if a native writer appends a
    second Vorbis-comment block containing conflicting or destructive tags.
    A valid FLAC stream has exactly one VORBIS_COMMENT metadata block.
    """
    comments = blocks.get(4, [])
    if len(comments) != 1:
        raise AssertionError("FLAC must contain exactly one Vorbis-comment block")
    return flac_vorbis_fields(comments[0])


def verify_original_flac_vorbis(before_fields, after_fields):
    """Fail closed if *any* existing physical Vorbis field was lost or edited.

    Only the explicitly requested QA marker is allowed to differ. Comparing
    Counters preserves exact byte values and multivalue multiplicity while
    tolerating harmless field reordering by the native metadata writer.
    """
    def preserved(fields):
        return {key: Counter(values) for key, values in fields.items()
                if key != "DISCOGGER_QA_RUNNER"}
    if preserved(before_fields) != preserved(after_fields):
        raise AssertionError("Original physical FLAC Vorbis fields changed or disappeared")



def verify_original_flac_blocks(before_blocks, after_blocks):
    """Protect every physical FLAC metadata block except mutable padding/comments.

    Native writers may resize PADDING (type 1) and rewrite VORBIS_COMMENT
    (type 4, separately checked field-by-field). All other metadata, including
    STREAMINFO, APPLICATION, SEEKTABLE, CUESHEET, PICTURE and unknown blocks,
    must retain their exact bytes and multiplicity, independent of order.
    """
    for kind in set(before_blocks) | set(after_blocks):
        if kind in (1, 4):
            continue
        if Counter(before_blocks.get(kind, [])) != Counter(after_blocks.get(kind, [])):
            raise AssertionError(f"Physical FLAC metadata block type {kind} changed")


def _syncsafe_size(value):
    if len(value) != 4 or any(byte & 0x80 for byte in value):
        raise AssertionError("Invalid ID3v2 synchsafe header")
    return (value[0] << 21) | (value[1] << 14) | (value[2] << 7) | value[3]


def id3_frame_bounds(content):
    """Validate the ID3v2 tag and skip v2.3/v2.4 extended headers.

    Both TXXX and APIC checks must inspect actual frames, not bytes inside
    extended-header data that happen to resemble frame identifiers.
    """
    if content[:3] != b"ID3" or len(content) < 10:
        raise AssertionError("ID3v2 header absent")
    version = content[3]
    if version not in (3, 4):
        raise AssertionError("Unsupported synthetic fixture ID3v2 version")
    # Unsynchronisation changes the meaning of on-disk frame bytes. Likewise,
    # experimental/footer/reserved flags are not decoded by this verifier.
    # Reject rather than incorrectly certify preservation of opaque metadata.
    if content[4] != 0:
        raise AssertionError("Unsupported ID3v2 revision")
    if content[5] & ~0x40:
        raise AssertionError("Unsupported ID3v2 unsync/experimental/footer flags")
    end = 10 + _syncsafe_size(content[6:10])
    if end > len(content):
        raise AssertionError("Truncated ID3v2 tag")
    offset = 10
    if content[5] & 0x40:
        if offset + 4 > end:
            raise AssertionError("Truncated extended ID3v2 header")
        size = (int.from_bytes(content[offset:offset + 4], "big")
                if version == 3 else _syncsafe_size(content[offset:offset + 4]))
        skip = 4 + size if version == 3 else size
        if size < 6 or skip > end - offset:
            raise AssertionError("Invalid extended ID3v2 header length")
        offset += skip
    return version, offset, end


def id3_txxx_values(content):
    """Read actual TXXX descriptions/values, including UTF-16 ID3v2 frames."""
    version, offset, end = id3_frame_bounds(content)
    decoded = []
    while offset + 10 <= end:
        frame_id = content[offset:offset + 4]
        if frame_id == bytes(4):
            if any(content[offset:end]):
                raise AssertionError("Invalid nonzero ID3v2 TXXX padding")
            return decoded
        if not all((65 <= ch <= 90 or 48 <= ch <= 57) for ch in frame_id):
            raise AssertionError("Invalid ID3v2 frame identifier")
        frame_size = (int.from_bytes(content[offset + 4:offset + 8], "big")
                      if version == 3 else _syncsafe_size(content[offset + 4:offset + 8]))
        frame_flags = content[offset + 8:offset + 10]
        offset += 10
        if frame_size < 1 or offset + frame_size > end:
            raise AssertionError("Malformed ID3v2 frame length")
        frame = content[offset:offset + frame_size]
        offset += frame_size
        if frame_id != b"TXXX":
            continue
        # Grouped, encrypted, compressed or otherwise opaque TXXX cannot
        # prove custom-tag/ReplayGain preservation from decoded plain text.
        if frame_flags != bytes(2):
            raise AssertionError("Opaque/modified ID3v2 TXXX frame flags")
        enc, raw = frame[0], frame[1:]
        codecs = {0: "latin-1", 1: "utf-16", 2: "utf-16-be", 3: "utf-8"}
        if enc not in codecs:
            raise AssertionError("Unsupported TXXX text encoding")
        codec = codecs[enc]
        if enc == 1 and not (raw.startswith(b"\xff\xfe") or raw.startswith(b"\xfe\xff")):
            codec = "utf-16-le"
        try:
            payload = raw.decode(codec)
        except UnicodeError as error:
            raise AssertionError("Corrupt Unicode TXXX frame") from error
        # A BOM at the beginning is an encoding marker; a BOM *inside* a
        # custom field is data. Globally removing U+FEFF would falsely certify
        # an SDK write that silently deleted a user tag character.
        if payload.startswith("\ufeff"):
            payload = payload[1:]
        # A TXXX frame without the required description/value separator is
        # malformed, not a field that may be silently ignored during a
        # physical metadata preservation audit. Keep a legitimate empty value.
        if "\x00" not in payload:
            raise AssertionError("Malformed ID3v2 TXXX missing value separator")
        name, value = payload.split("\x00", 1)
        # UTF-16 descriptions and values can each carry their own BOM.
        # Strip only that second boundary marker, never an interior BOM.
        if enc in (1, 2) and value.startswith("\ufeff"):
            value = value[1:]
        # TXXX descriptions are user-defined field names: whitespace is data.
        # Trimming it hides destructive renames and can spoof the QA marker.
        decoded.append((name, value.rstrip("\x00")))
    if any(content[offset:end]):
        raise AssertionError("Truncated trailing ID3v2 TXXX frame")
    return decoded



def id3_standard_text_values(content):
    """Read real ID3v2 text frames; never trust markers in audio or artwork.

    Compare decoded values so an SDK re-encoding of the same physical tag is
    accepted, but a missing/changed TITLE, ARTIST, GENRE or other T*** field
    cannot pass merely because its old bytes remain elsewhere in the file.
    TXXX custom/ReplayGain tags are verified independently.
    """
    version, offset, end = id3_frame_bounds(content)
    values = Counter()
    while offset + 10 <= end:
        frame_id = content[offset:offset + 4]
        if frame_id == bytes(4):
            if any(content[offset:end]):
                raise AssertionError("Invalid nonzero ID3v2 standard-text padding")
            return values
        if not all(65 <= ch <= 90 or 48 <= ch <= 57 for ch in frame_id):
            raise AssertionError("Invalid ID3v2 text frame identifier")
        size = (int.from_bytes(content[offset + 4:offset + 8], "big")
                if version == 3 else _syncsafe_size(content[offset + 4:offset + 8]))
        flags = content[offset + 8:offset + 10]
        offset += 10
        if size < 1 or size > end - offset:
            raise AssertionError("Invalid ID3v2 text frame size")
        payload = content[offset:offset + size]
        offset += size
        if not frame_id.startswith(b"T") or frame_id == b"TXXX":
            continue
        # Grouped/compressed/encrypted text cannot be compared as plain bytes.
        if flags != bytes(2):
            raise AssertionError("Unsupported ID3v2 text frame flags")
        encoding = payload[0]
        codecs = {0: "latin-1", 1: "utf-16", 2: "utf-16-be", 3: "utf-8"}
        if encoding not in codecs:
            raise AssertionError("Invalid ID3v2 text encoding")
        raw = payload[1:]
        codec = codecs[encoding]
        if encoding == 1 and not (raw.startswith(b"\xff\xfe") or raw.startswith(b"\xfe\xff")):
            codec = "utf-16-le"
        try:
            decoded = raw.decode(codec).lstrip("\ufeff").rstrip("\x00")
        except UnicodeError as error:
            raise AssertionError("Corrupt ID3v2 standard text frame") from error
        values[(frame_id, decoded)] += 1
    if any(content[offset:end]):
        raise AssertionError("Truncated trailing ID3v2 standard-text frame")
    return values


def verify_original_mp3_standard_text(before, after):
    """Reject silent physical title/artist/genre removal or replacement."""
    original = id3_standard_text_values(before)
    if not original:
        raise AssertionError("Synthetic MP3 has no physical standard text tags")
    if original != id3_standard_text_values(after):
        raise AssertionError("Physical MP3 standard text tags changed or disappeared")

def verify_original_mp3_txxx(before, after):
    """Preserve physical custom TXXX and ReplayGain despite ID3 re-encoding."""
    def existing(raw):
        # TXXX descriptions are user-defined physical names: preserve their
        # exact case. Uppercasing here concealed destructive case-only renames.
        return Counter((name, value) for name, value in id3_txxx_values(raw)
                       if name.upper() != "DISCOGGER_QA_RUNNER")
    original = existing(before)
    for name, value in (
        ("DISCOGGER_QA_CUSTOM", "Keep original MP3 metadata"),
        ("REPLAYGAIN_TRACK_GAIN", "-5.00 dB"),
        ("REPLAYGAIN_TRACK_PEAK", "0.800000"),
    ):
        if original[(name, value)] != 1:
            raise AssertionError("Synthetic MP3 lacks custom/ReplayGain fixture: " + name)
    observed = existing(after)
    # foobar's MP3 writer canonicalizes the *standard* ReplayGain TXXX
    # descriptions (their field names are case-insensitive in foobar's tag
    # model). This is not a custom-field rename: only these two standard
    # identifiers may differ in letter case during a real metadata update.
    # Values and multiplicity remain exact. All custom TXXX names, including
    # case, whitespace and interior BOMs, remain strictly protected.
    replaygain_names = {"REPLAYGAIN_TRACK_GAIN", "REPLAYGAIN_TRACK_PEAK"}
    def canonicalize_standard_replaygain(fields):
        normalized = Counter()
        seen_spelling = {}
        for (name, value), count in fields.items():
            canonical = name.upper()
            if canonical in replaygain_names:
                prior = seen_spelling.setdefault(canonical, name)
                if prior != name:
                    # Different physical TXXX descriptions would collapse to
                    # one SDK field: do not certify this lossy transformation.
                    raise AssertionError("Ambiguous ReplayGain TXXX description aliases")
                name = canonical
            normalized[(name, value)] += count
        return normalized
    if canonicalize_standard_replaygain(original) != canonicalize_standard_replaygain(observed):
        # Only classify the difference. Never print values, file paths or
        # user-defined tag names into a public CI log. Custom case-only
        # rewrites are still destructive and remain a hard failure.
        def folded(fields):
            return Counter((name.casefold(), value) for (name, value), count
                           in fields.items() for _ in range(count))
        case_only = folded(original) == folded(observed)
        missing = sum((original - observed).values())
        added = sum((observed - original).values())
        reason = "CASE_ONLY_DESCRIPTION_REWRITE" if case_only else "FIELD_VALUE_OR_MULTIPLICITY_CHANGE"
        raise AssertionError(
            "Physical MP3 custom TXXX/ReplayGain metadata was changed "
            f"({reason}; missing={missing}; unexpected={added}; values_redacted=1)")

def verify_exact_mp3_runner_marker(content):
    """Require exactly one physical QA marker; duplicates cannot certify a write.

    A writer appending the marker twice (or leaving an old conflicting value)
    must not be accepted merely because one expected tuple is present.
    """
    markers = [(name, value) for name, value in id3_txxx_values(content)
               if name.upper() == "DISCOGGER_QA_RUNNER"]
    if markers != [("DISCOGGER_QA_RUNNER", "RUNNER-VERIFIED")]:
        raise AssertionError("MP3 physical QA marker absent, duplicate or conflicting")


def id3_apic_frames(content):
    """Fail closed on malformed physical ID3 frames or opaque APIC flags.

    Identical picture payload bytes are not preservation evidence when a
    writer changes APIC frame flags to compression/encryption/grouping.
    """
    version, pos, end = id3_frame_bounds(content)
    pictures = []
    while pos + 10 <= end:
        name = content[pos:pos + 4]
        if name == bytes(4):
            if any(content[pos:end]):
                raise AssertionError("Invalid nonzero ID3v2 padding")
            return pictures
        if not all(65 <= ch <= 90 or 48 <= ch <= 57 for ch in name):
            raise AssertionError("Invalid ID3v2 picture-walker frame identifier")
        size = (int.from_bytes(content[pos + 4:pos + 8], "big")
                if version == 3 else _syncsafe_size(content[pos + 4:pos + 8]))
        flags = content[pos + 8:pos + 10]
        pos += 10
        if size < 1 or size > end - pos:
            raise AssertionError("Invalid ID3v2 picture-walker frame size")
        if name == b"APIC":
            if flags != bytes(2):
                raise AssertionError("Opaque/modified ID3v2 APIC frame flags")
            pictures.append(content[pos:pos + size])
        pos += size
    if any(content[pos:end]):
        raise AssertionError("Truncated trailing ID3v2 frame")
    return pictures


def id3_other_physical_frames(content):
    """Preserve non-text/non-artwork ID3 frames, including comments and private data.

    The ordinary text, TXXX/ReplayGain and APIC checks run separately. For
    COMM, USLT, UFID, PRIV, GEOB, chapters and unknown frames, do not silently
    treat a lost frame as harmless just because TITLE and APIC still match.
    Frame flags and raw payloads are part of the physical preservation contract.
    """
    version, pos, end = id3_frame_bounds(content)
    frames = Counter()
    while pos + 10 <= end:
        name = content[pos:pos + 4]
        if name == bytes(4):
            if any(content[pos:end]):
                raise AssertionError("Invalid nonzero ID3v2 opaque-frame padding")
            return frames
        if not all(65 <= ch <= 90 or 48 <= ch <= 57 for ch in name):
            raise AssertionError("Invalid ID3v2 opaque-frame identifier")
        size = (int.from_bytes(content[pos + 4:pos + 8], "big")
                if version == 3 else _syncsafe_size(content[pos + 4:pos + 8]))
        flags = content[pos + 8:pos + 10]
        pos += 10
        if size < 1 or size > end - pos:
            raise AssertionError("Invalid ID3v2 opaque-frame size")
        if name not in (b"TXXX", b"APIC") and not name.startswith(b"T"):
            frames[(name, flags, content[pos:pos + size])] += 1
        pos += size
    if any(content[pos:end]):
        raise AssertionError("Truncated trailing ID3v2 opaque frame")
    return frames


def verify_original_mp3_other_frames(before, after):
    """Require exact preservation of every other physical ID3 frame and count."""
    if id3_other_physical_frames(before) != id3_other_physical_frames(after):
        raise AssertionError("Physical MP3 comments/private/unknown frames changed")


def id3_audio_payload(content):
    end = 10 + _syncsafe_size(content[6:10])
    if end > len(content): raise AssertionError("ID3 tag overlaps MP3 data")
    return content[end:]

def verify(before: bytes, after: bytes, suffix):
    if before == after:
        raise AssertionError("requested physical tag update was a no-op")
    if suffix == ".mp3":
        if not before.startswith(b"ID3") or not after.startswith(b"ID3"):
            raise AssertionError("ID3v2 header lost")
        for item in (b"Synthetic Test", b"Discogger QA", b"Trance"):
            if item not in after:
                raise AssertionError(f"Original MP3 metadata missing after writer: {item!r}")
        original_art = id3_apic_frames(before)
        if not original_art or original_art != id3_apic_frames(after):
            raise AssertionError("Original ID3v2 APIC picture bytes were changed or removed")
        if id3_audio_payload(before) != id3_audio_payload(after):
            raise AssertionError("Original MP3 compressed audio payload changed")
        verify_original_mp3_standard_text(before, after)
        verify_original_mp3_txxx(before, after)
        verify_original_mp3_other_frames(before, after)
        verify_exact_mp3_runner_marker(after)
    elif suffix == ".flac":
        first, second = flac_blocks(before), flac_blocks(after)
        verify_original_flac_blocks(first, second)
        if first.get(5) != second.get(5):
            raise AssertionError("Native FLAC Type-5 CUESHEET changed")
        if not first.get(6) or first.get(6) != second.get(6):
            raise AssertionError("Original FLAC Type-6 PICTURE block changed")
        def audio_tail(raw):
            pos = 4
            while True:
                is_last = (raw[pos] & 128) != 0
                size = int.from_bytes(raw[pos+1:pos+4], "big")
                pos += 4 + size
                if is_last: return raw[pos:]
        if audio_tail(before) != audio_tail(after):
            raise AssertionError("Original FLAC compressed audio payload changed")
        before_fields = single_flac_vorbis_fields(first)
        after_fields = single_flac_vorbis_fields(second)
        if not before_fields.get("CUESHEET") or before_fields["CUESHEET"] != after_fields.get("CUESHEET"):
            raise AssertionError("Embedded textual CUESHEET bytes changed or disappeared")
        for token in (b"Runner Embedded EP", b"Progressive Trance", b"Preserve me"):
            if not any(token in value for value in after_fields["CUESHEET"]):
                raise AssertionError(f"Embedded FLAC CUE marker missing: {token!r}")
        verify_original_flac_vorbis(before_fields, after_fields)
        if Counter(after_fields.get("GENRE", [])) != Counter((b"House", b"Progressive Trance")):
            raise AssertionError("Original separate FLAC multi-value GENRE entries not retained")
        if after_fields.get("DISCOGGER_QA_RUNNER") != [b"RUNNER-VERIFIED"]:
            raise AssertionError("Physical FLAC writer did not persist the exact custom tag key and value")
    else:
        raise AssertionError("Unknown media extension")



def selftest_flac_vorbis():
    def field(value):
        return len(value).to_bytes(4, "little") + value
    vendor = field(b"Discogger QA")
    entries = [
        b"CUESHEET=TITLE \"Test\"",
        b"CUE_TRACK01_DISCOGGER_QA_RUNNER=RUNNER-VERIFIED",
    ]
    raw = vendor + len(entries).to_bytes(4, "little") + b"".join(map(field, entries))
    parsed = flac_vorbis_fields(raw)
    assert parsed["CUESHEET"] == [b'TITLE "Test"']
    assert "DISCOGGER_QA_RUNNER" not in parsed
    entries.append(b"DISCOGGER_QA_RUNNER=RUNNER-VERIFIED")
    raw = vendor + len(entries).to_bytes(4, "little") + b"".join(map(field, entries))
    assert flac_vorbis_fields(raw)["DISCOGGER_QA_RUNNER"] == [b"RUNNER-VERIFIED"]
    for invalid in (raw[:-2], raw + b"junk", vendor + (99).to_bytes(4, "little")):
        try:
            flac_vorbis_fields(invalid)
        except AssertionError:
            pass
        else:
            raise AssertionError("Invalid Vorbis comment was not rejected")
    # P1 physical FLAC gate: an appended second Vorbis block must never be
    # ignored just because the first block still contains the expected tags.
    def flac_with_comments(comments):
        # A real FLAC must start with a 34-byte STREAMINFO metadata block.
        encoded = bytes((0,)) + (34).to_bytes(3, "big") + bytes(34)
        for index, comment in enumerate(comments):
            encoded += bytes((4 | (0x80 if index == len(comments) - 1 else 0),))
            encoded += len(comment).to_bytes(3, "big") + comment
        return b"fLaC" + encoded + b"synthetic-audio"
    assert single_flac_vorbis_fields(flac_blocks(flac_with_comments([raw]))) == flac_vorbis_fields(raw)
    # P1: reject metadata-block ordering/structure damage that a byte-multiset
    # comparison cannot detect, even if the original STREAMINFO bytes survive.
    streaminfo = bytes((0,)) + (34).to_bytes(3, "big") + bytes(34)
    comment_last = bytes((0x84,)) + len(raw).to_bytes(3, "big") + raw
    good_flac = b"fLaC" + streaminfo + comment_last + b"synthetic-audio"
    assert flac_blocks(good_flac)[0] == [bytes(34)]
    invalid_flac = (
        b"fLaC" + comment_last + b"synthetic-audio",  # no STREAMINFO
        b"fLaC" + streaminfo + streaminfo + comment_last + b"synthetic-audio",
        b"fLaC" + bytes((0,)) + (33).to_bytes(3, "big") + bytes(33)
        + comment_last + b"synthetic-audio",
        b"fLaC" + streaminfo + bytes((0xff,)) + len(raw).to_bytes(3, "big")
        + raw + b"synthetic-audio",  # forbidden metadata type 127
        b"fLaC" + streaminfo[:-1],  # truncated STREAMINFO
    )
    for malformed in invalid_flac:
        try:
            flac_blocks(malformed)
        except AssertionError:
            pass
        else:
            raise AssertionError("Invalid FLAC metadata ordering/structure escaped audit")
    print("DISCOGGER_FLAC_STREAMINFO_STRUCTURE_VERIFIER=PASS")
    conflicting = raw.replace(b"RUNNER-VERIFIED", b"RUNNER-CORRUPTED")
    for invalid in (
        {}, {4: []}, {4: [raw, raw]}, {4: [raw, conflicting]},
        flac_blocks(flac_with_comments([raw, raw])),
        flac_blocks(flac_with_comments([raw, conflicting])),
    ):
        try:
            single_flac_vorbis_fields(invalid)
        except AssertionError:
            pass
        else:
            raise AssertionError("Duplicate/missing FLAC Vorbis block escaped verifier")
    print("DISCOGGER_FLAC_SINGLE_VORBIS_BLOCK_VERIFIER=PASS")
    # P0 physical-writer safety: all physical Vorbis fields must survive,
    # including unknown custom keys and standard TITLE/ARTIST/DATE fields.
    # The SDK may reorder fields, but must not silently lose multiplicity.
    baseline = {
        "TITLE": [b"Synthetic Track"],
        "ARTIST": [b"QA Artist"],
        "DATE": [b"2026"],
        "GENRE": [b"Trance", b"House"],
        "X-UNMAPPED": [b"Keep original custom value"],
        "REPLAYGAIN_TRACK_GAIN": [b"-5.00 dB"],
        "CUESHEET": [b'FILE "synthetic.flac"'],
    }
    updated = {**baseline, "GENRE": [b"House", b"Trance"],
               "DISCOGGER_QA_RUNNER": [b"RUNNER-VERIFIED"]}
    verify_original_flac_vorbis(baseline, updated)
    corrupted_cases = (
        {key: values for key, values in updated.items() if key != "ARTIST"},
        {**updated, "TITLE": [b"Unexpected replacement"]},
        {**updated, "X-UNMAPPED": [b"Lost custom value"]},
        {**updated, "GENRE": [b"Trance"]},
        {**updated, "GENRE": [b"Trance", b"House", b"House"]},
        {**updated, "UNREQUESTED_NEW_TAG": [b"Unexpected addition"]},
    )
    for corrupted in corrupted_cases:
        try:
            verify_original_flac_vorbis(baseline, corrupted)
        except AssertionError:
            pass
        else:
            raise AssertionError("FLAC physical-field preservation regression escaped")
    # Metadata preservation must include opaque FLAC block types, not just
    # Vorbis comments, artwork and CUESHEET. Padding/comment rewrites are OK.
    physical = {
        0: [b"STREAMINFO-original"], 1: [b"padding"],
        2: [b"APP-opaque"], 3: [b"SEEKTABLE-opaque"],
        4: [b"vorbis-before"], 5: [b"native-cuesheet"],
        6: [b"picture-a", b"picture-b"], 10: [b"unknown-block"],
    }
    rewritten = {**physical, 1: [b"resized-padding"],
                 4: [b"vorbis-after"], 6: [b"picture-b", b"picture-a"]}
    verify_original_flac_blocks(physical, rewritten)
    for damaged in (
        {key: values for key, values in rewritten.items() if key != 2},
        {**rewritten, 0: [b"STREAMINFO-changed"]},
        {**rewritten, 3: []},
        {**rewritten, 5: [b"changed-cuesheet"]},
        {**rewritten, 6: [b"picture-a"]},
        {**rewritten, 10: [b"unknown-block", b"unknown-block"]},
        {**rewritten, 11: [b"unexpected-added-block"]},
    ):
        try:
            verify_original_flac_blocks(physical, damaged)
        except AssertionError:
            pass
        else:
            raise AssertionError("Opaque FLAC physical metadata loss escaped verifier")
    print("DISCOGGER_FLAC_OPAQUE_METADATA_PRESERVED=PASS")
    print("DISCOGGER_FLAC_ALL_PHYSICAL_FIELDS_PRESERVED=PASS")
    print("DISCOGGER_FLAC_VORBIS_EXACT_KEY_VERIFIER=PASS")

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
            # Empty TXXX values remain actual fields, not silently skipped.
            empty_raw = ("EMPTY\x00".encode("utf-16")
                         if enc == 1 else b"EMPTY\x00")
            empty_body = bytes((enc,)) + empty_raw
            empty_size = (len(empty_body).to_bytes(4, "big")
                          if version == 3 else ss(len(empty_body)))
            empty_frame = b"TXXX" + empty_size + bytes(2) + empty_body
            empty_tag = b"ID3" + bytes((version, 0, 0)) + ss(len(empty_frame)) + empty_frame
            assert id3_txxx_values(empty_tag) == [("EMPTY", "")]
            # Malformed custom fields without a separator must fail closed.
            no_sep_raw = ("DISCOGGER_QA_RUNNER".encode("utf-16")
                          if enc == 1 else b"DISCOGGER_QA_RUNNER")
            no_sep_body = bytes((enc,)) + no_sep_raw
            no_sep_size = (len(no_sep_body).to_bytes(4, "big")
                           if version == 3 else ss(len(no_sep_body)))
            no_sep_frame = b"TXXX" + no_sep_size + bytes(2) + no_sep_body
            no_sep_tag = b"ID3" + bytes((version, 0, 0)) + ss(len(no_sep_frame)) + no_sep_frame
            try:
                id3_txxx_values(no_sep_tag)
            except AssertionError:
                pass
            else:
                raise AssertionError("Malformed TXXX separator escaped physical audit")
            # A writer must not hide the same TXXX payload behind changed
            # compression/grouping flags, corrupt padding, or a short tail.
            # Valid zero padding remains permitted in both ID3 versions.
            padded = fake[:6] + ss(len(frame) + 3) + frame + bytes(3)
            assert id3_txxx_values(padded) == [
                ("DISCOGGER_QA_RUNNER", "RUNNER-VERIFIED")]
            opaque = fake[:18] + bytes((0, 8)) + fake[20:]
            short_tail = fake[:6] + ss(len(frame) + 3) + frame + bytes((1, 2, 3))
            bad_padding = (fake[:6] + ss(len(frame) + 10) + frame +
                           bytes(4) + bytes((1,)) + bytes(5))
            for malformed in (opaque, short_tail, bad_padding):
                try:
                    id3_txxx_values(malformed)
                except AssertionError:
                    pass
                else:
                    raise AssertionError("Opaque or malformed ID3v2 TXXX escaped verifier")
    # UTF-16 values may carry their own second BOM after the description.
    dual = (b"\xff\xfe" + "DISCOGGER_QA_RUNNER".encode("utf-16-le") +
            b"\x00\x00\xff\xfe" + "RUNNER-VERIFIED".encode("utf-16-le"))
    body = b"\x01" + dual
    frame = b"TXXX" + len(body).to_bytes(4, "big") + bytes(2) + body
    assert id3_txxx_values(b"ID3\x03\x00\x00" + ss(len(frame)) + frame) == [
        ("DISCOGGER_QA_RUNNER", "RUNNER-VERIFIED")]
    # The verifier must reject lost/custom ReplayGain values, not just parse.
    def frame(name, value):
        body = bytes((3,)) + name.encode() + bytes(1) + value.encode()
        return b"TXXX" + len(body).to_bytes(4, "big") + bytes(2) + body
    fields = b"".join(frame(k, val) for k, val in (
        ("DISCOGGER_QA_CUSTOM", "Keep original MP3 metadata"),
        ("REPLAYGAIN_TRACK_GAIN", "-5.00 dB"),
        ("REPLAYGAIN_TRACK_PEAK", "0.800000"),
    ))
    original = b"ID3" + bytes((3, 0, 0)) + ss(len(fields)) + fields
    verify_original_mp3_txxx(original, original)
    # An interior U+FEFF in a real custom tag must survive byte-level writer
    # verification. Previously a global BOM replacement hid its deletion.
    def custom_tag(payload):
        return b"ID3" + bytes((3, 0, 0)) + ss(len(payload)) + payload
    bom_original = custom_tag(fields + frame("DISCOGGER_QA_BOM", "alpha\ufeffbeta"))
    bom_removed = custom_tag(fields + frame("DISCOGGER_QA_BOM", "alphabeta"))
    assert ("DISCOGGER_QA_BOM", "alpha\ufeffbeta") in id3_txxx_values(bom_original)
    try:
        verify_original_mp3_txxx(bom_original, bom_removed)
    except AssertionError:
        pass
    else:
        raise AssertionError("Deleted interior TXXX BOM escaped preservation audit")
    # A writer may not silently trim whitespace from custom ID3 field names.
    # Previous .strip() normalized these two distinct physical descriptions.
    spaced_original = custom_tag(fields + frame(" DJ USER KEY ", "unchanged"))
    spaced_changed = custom_tag(fields + frame("DJ USER KEY", "unchanged"))
    assert (" DJ USER KEY ", "unchanged") in id3_txxx_values(spaced_original)
    verify_original_mp3_txxx(spaced_original, spaced_original)
    try:
        verify_original_mp3_txxx(spaced_original, spaced_changed)
    except AssertionError:
        pass
    else:
        raise AssertionError("Trimmed physical TXXX description escaped preservation audit")
    # ID3 TXXX descriptions are not ordinary case-folded Vorbis keys.
    # A physical writer must not silently change the spelling of a custom
    # description while retaining its value (v2.3 and v2.4).
    for version in (3, 4):
        def case_tag(custom_name):
            entries = (
                ("DISCOGGER_QA_CUSTOM", "Keep original MP3 metadata"),
                ("REPLAYGAIN_TRACK_GAIN", "-5.00 dB"),
                ("REPLAYGAIN_TRACK_PEAK", "0.800000"),
                (custom_name, "unchanged"),
            )
            payload = b""
            for name, value in entries:
                body = b"\x03" + name.encode("utf-8") + b"\x00" + value.encode("utf-8")
                size = (len(body).to_bytes(4, "big") if version == 3
                        else ss(len(body)))
                payload += b"TXXX" + size + bytes(2) + body
            return b"ID3" + bytes((version, 0, 0)) + ss(len(payload)) + payload
        original_case = case_tag("DJ User Key")
        renamed_case = case_tag("DJ USER KEY")
        verify_original_mp3_txxx(original_case, original_case)
        # Standard ReplayGain capitalization is normalized by foobar on MP3
        # metadata updates. Only that known normalization is permitted.
        def standard_replaygain_case(name_gain, name_peak):
            entries = (
                ("DISCOGGER_QA_CUSTOM", "Keep original MP3 metadata"),
                (name_gain, "-5.00 dB"),
                (name_peak, "0.800000"),
                ("DJ User Key", "unchanged"),
            )
            payload = b""
            for name, value in entries:
                body = b"\\x03" + name.encode() + b"\\x00" + value.encode()
                size = (len(body).to_bytes(4, "big") if version == 3
                        else ss(len(body)))
                payload += b"TXXX" + size + bytes(2) + body
            return b"ID3" + bytes((version, 0, 0)) + ss(len(payload)) + payload
        canonical_rg = standard_replaygain_case(
            "replaygain_track_gain", "replaygain_track_peak")
        verify_original_mp3_txxx(original_case, canonical_rg)
        # Two differently-cased original fields must never be conflated.
        duplicate_rg = custom_tag(fields + frame(
            "replaygain_track_gain", "-5.00 dB"))
        try:
            verify_original_mp3_txxx(duplicate_rg, canonical_rg)
        except AssertionError:
            pass
        else:
            raise AssertionError("Colliding ReplayGain aliases escaped preservation audit")
        try:
            verify_original_mp3_txxx(original_case, renamed_case)
        except AssertionError:
            pass
        else:
            raise AssertionError("Case-only TXXX description rename escaped preservation audit")
    marker = frame("DISCOGGER_QA_RUNNER", "RUNNER-VERIFIED")
    def with_qa_marker(marker_frames):
        payload = fields + marker_frames
        return b"ID3" + bytes((3, 0, 0)) + ss(len(payload)) + payload
    verify_exact_mp3_runner_marker(with_qa_marker(marker))
    for bad_marker in (b"", marker + marker,
                       marker + frame("DISCOGGER_QA_RUNNER", "STALE"),
                       frame("DISCOGGER_QA_RUNNER", "RUNNER-\ufeffVERIFIED"),
                       frame("DISCOGGER_QA_RUNNER", "STALE")):
        try:
            verify_exact_mp3_runner_marker(with_qa_marker(bad_marker))
        except AssertionError:
            pass
        else:
            raise AssertionError("Missing/duplicate/conflicting MP3 QA marker escaped audit")
    # Leading/trailing spaces cannot turn a different physical TXXX field
    # into the exact runner marker expected from a native SDK write.
    for forged_name in (" DISCOGGER_QA_RUNNER", "DISCOGGER_QA_RUNNER "):
        try:
            verify_exact_mp3_runner_marker(with_qa_marker(
                frame(forged_name, "RUNNER-VERIFIED")))
        except AssertionError:
            pass
        else:
            raise AssertionError("Whitespace-spoofed TXXX QA marker escaped verifier")
    print("DISCOGGER_ID3_TXXX_DESCRIPTION_WHITESPACE_PRESERVATION=PASS")
    # Classification must not mask case-only corruption or disclose values.
    assert Counter((name.casefold(), value) for name, value in
                   id3_txxx_values(case_tag("DJ User Key"))) == Counter(
                       (name.casefold(), value) for name, value in
                       id3_txxx_values(case_tag("DJ USER KEY")))
    print("DISCOGGER_ID3_TXXX_DESCRIPTION_CASE_PRESERVATION=PASS")
    print("DISCOGGER_STANDARD_REPLAYGAIN_CASE_NORMALIZATION=PASS")
    print("DISCOGGER_ID3_TXXX_INTERIOR_BOM_PRESERVATION=PASS")
    for corrupted in (original.replace(b"0.800000", b"0.700000"),
                      b"ID3" + bytes((3, 0, 0)) + ss(len(fields)-1) + fields[:-1]):
        try:
            verify_original_mp3_txxx(original, corrupted)
        except AssertionError:
            pass
        else:
            raise AssertionError("MP3 ReplayGain preservation regression escaped")
    # P1 media-safety verifier: ID3v2 extended headers must not hide APIC
    # frames, or cause a false picture-preservation PASS/FAIL.
    for version in (3, 4):
        picture = b"\x00image/png\x00\x03\x00\x89PNG-SYNTHETIC"
        size = len(picture).to_bytes(4, "big") if version == 3 else ss(len(picture))
        frame = b"APIC" + size + bytes(2) + picture
        ext = ((6).to_bytes(4, "big") + bytes(6)
               if version == 3 else ss(6) + b"\x01\x00")
        payload = ext + frame
        fake = b"ID3" + bytes((version, 0, 0x40)) + ss(len(payload)) + payload
        assert id3_apic_frames(fake) == [picture]
        assert id3_txxx_values(fake) == []
        # Same APIC payload but altered flags is NOT an identical picture.
        # Likewise reject corrupt frame IDs, zero-size frames and stray bytes.
        bad_flags = fake.replace(b"APIC" + size + bytes(2),
                                 b"APIC" + size + bytes((0, 8)), 1)
        bad_id = fake.replace(b"APIC" + size, b"A?IC" + size, 1)
        zero_size = fake.replace(b"APIC" + size,
                                 b"APIC" + bytes(4), 1)
        trailing = (fake[:6] + ss(len(payload) + 1) +
                    payload + bytes((1,)))
        for invalid_apic in (bad_flags, bad_id, zero_size, trailing):
            try:
                id3_apic_frames(invalid_apic)
            except AssertionError:
                pass
            else:
                raise AssertionError("Unsafe/malformed APIC framing escaped verifier")
        changed = fake.replace(b"PNG-SYNTHETIC", b"PNG-SYNTHETIx")
        assert id3_apic_frames(fake) != id3_apic_frames(changed)
        # An extended header claiming bytes beyond the tag must fail closed.
        oversized = ((100).to_bytes(4, "big") if version == 3 else ss(100))
        malformed = b"ID3" + bytes((version, 0, 0x40)) + ss(6) + oversized + bytes(2)
        for reader in (id3_apic_frames, id3_txxx_values):
            try:
                reader(malformed)
            except AssertionError:
                pass
            else:
                raise AssertionError("Invalid ID3v2 extended header was accepted")

    # P1 physical-media gate: the original TITLE/GENRE must survive a real
    # writer even if their literal bytes are still present in compressed audio.
    # Re-encoding the same value between ID3v2.3 UTF-16 and v2.4 UTF-8 is OK.
    def text_frame(name, value, version, encoding):
        raw = value.encode("utf-16") if encoding == 1 else value.encode("utf-8")
        body = bytes((encoding,)) + raw
        size = len(body).to_bytes(4, "big") if version == 3 else ss(len(body))
        return name + size + bytes(2) + body
    def text_tag(version, encoding, genre="Trance", include_genre=True):
        frames = text_frame(b"TIT2", "Synthetic Test", version, encoding)
        if include_genre:
            frames += text_frame(b"TCON", genre, version, encoding)
        # Deliberate audio marker: a raw substring grep would falsely PASS.
        return b"ID3" + bytes((version, 0, 0)) + ss(len(frames)) + frames + b"Trance"
    original_text = text_tag(3, 1)
    verify_original_mp3_standard_text(original_text, text_tag(4, 3))
    # P1: toggling ID3 header unsynchronisation (or other unsupported bits)
    # changes the interpretation of identical frame bytes. Every physical
    # metadata reader must fail closed, for both supported ID3v2 versions.
    for version in (3, 4):
        clean = text_tag(version, 1 if version == 3 else 3)
        for reader in (id3_txxx_values, id3_standard_text_values,
                       id3_apic_frames, id3_other_physical_frames):
            reader(clean)
            for flags in (0x80, 0x20, 0x10, 0x01, 0xc0):
                malformed = clean[:5] + bytes((flags,)) + clean[6:]
                try:
                    reader(malformed)
                except AssertionError:
                    pass
                else:
                    raise AssertionError("Opaque ID3v2 header flags escaped verifier")
            malformed_revision = clean[:4] + bytes((1,)) + clean[5:]
            try:
                reader(malformed_revision)
            except AssertionError:
                pass
            else:
                raise AssertionError("Unsupported ID3v2 revision escaped verifier")
    print("DISCOGGER_ID3_HEADER_FLAG_INTEGRITY_VERIFIER=PASS")
    # Reject corrupt ID3v2 padding and partial frame headers, even when
    # all original text fields are still present and the audio contains
    # their literal bytes. Clean zero padding remains valid.
    text_payload = original_text[10:10 + _syncsafe_size(original_text[6:10])]
    def with_text_tail(tail):
        return (b"ID3\x03\x00\x00" + ss(len(text_payload) + len(tail)) +
                text_payload + tail + b"Trance")
    assert (id3_standard_text_values(with_text_tail(bytes(11))) ==
            id3_standard_text_values(original_text))
    for corrupt_tail in (b"\x00\x01", bytes(4) + b"\x01" + bytes(5)):
        malformed = with_text_tail(corrupt_tail)
        try:
            verify_original_mp3_standard_text(original_text, malformed)
        except AssertionError:
            pass
        else:
            raise AssertionError("Malformed ID3v2 text padding escaped preservation gate")
    for damaged in (text_tag(4, 3, genre="House"),
                    text_tag(4, 3, include_genre=False)):
        try:
            verify_original_mp3_standard_text(original_text, damaged)
        except AssertionError:
            pass
        else:
            raise AssertionError("Changed/missing physical MP3 text tag escaped verifier")
    # Non-text physical ID3 frames were previously invisible to the verifier.
    # Preserve comments/lyrics/private/unique-ID frames even across frame order
    # changes and ID3v2.3 -> v2.4 upgrades; reject removals, edits and extras.
    def opaque_frame(name, value, version, flags=bytes(2)):
        size = len(value).to_bytes(4, "big") if version == 3 else ss(len(value))
        return name + size + flags + value
    def opaque_tag(version, entries, padding=b""):
        payload = b"".join(opaque_frame(name, value, version, flags)
                           for name, value, flags in entries) + padding
        return b"ID3" + bytes((version, 0, 0)) + ss(len(payload)) + payload
    entries = [
        (b"COMM", b"eng\x00Original comment", bytes(2)),
        (b"USLT", b"eng\x00Original lyrics", bytes(2)),
        (b"PRIV", b"owner\x00opaque-data", bytes(2)),
        (b"UFID", b"owner\x00original-id", bytes(2)),
        (b"GEOB", b"application/octet-stream\x00attachment", bytes(2)),
    ]
    for version in (3, 4):
        original_other = opaque_tag(version, entries)
        assert len(id3_other_physical_frames(original_other)) == len(entries)
        verify_original_mp3_other_frames(
            original_other, opaque_tag(4, list(reversed(entries)), bytes(3)))
        invalid_others = [
            opaque_tag(4, entries[:-1]),
            opaque_tag(4, entries + [entries[0]]),
            opaque_tag(4, entries[:-1] + [(b"GEOB", b"modified", bytes(2))]),
            opaque_tag(4, entries[:-1] + [(b"GEOB", entries[-1][1], b"\x00\x08")]),
            opaque_tag(4, entries, b"\x00\x01"),
            opaque_tag(4, entries, b"\x01"),
        ]
        for invalid_other in invalid_others:
            try:
                verify_original_mp3_other_frames(original_other, invalid_other)
            except AssertionError:
                pass
            else:
                raise AssertionError("Lost/altered physical MP3 opaque frame escaped verifier")
    print("DISCOGGER_ID3_OPAQUE_FRAME_PRESERVATION_VERIFIER=PASS")
    print("DISCOGGER_ID3_STANDARD_TEXT_PRESERVATION_VERIFIER=PASS")
    print("DISCOGGER_ID3_EXTENDED_APIC_VERIFIER=PASS")
    print("DISCOGGER_ID3_TXXX_UNICODE_VERIFIER=PASS")
    print("DISCOGGER_ID3_TXXX_FRAME_INTEGRITY_VERIFIER=PASS")
    print("DISCOGGER_ID3_TXXX_MARKER_UNIQUENESS_VERIFIER=PASS")

if __name__ == "__main__":
    if len(sys.argv) == 2 and sys.argv[1] == "--selftest":
        selftest_id3()
        selftest_flac_vorbis()
        raise SystemExit(0)
    if len(sys.argv) != 3:
        raise SystemExit("Expected --selftest or original-copy and disposable updated file")
    original, updated = map(Path, sys.argv[1:])
    suffix = ".mp3" if "mp3" in original.name else ".flac"
    verify(original.read_bytes(), updated.read_bytes(), suffix)
    print("DISCOGGER_RUNNER_PHYSICAL_TAG_PRESERVATION=PASS")
