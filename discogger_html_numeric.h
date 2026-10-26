#pragma once
// Invalid numeric entities remain literal; never inject a NUL or malformed
// UTF-8 scalar into Discogs artist/release names or planned tag values.
namespace discogger_unicode {
inline bool valid_numeric_digits(const char* begin, const char* end, bool hex) noexcept {
    if (!begin || !end || begin >= end) return false;
    for (const char* p = begin; p != end; ++p) {
        const unsigned char c = static_cast<unsigned char>(*p);
        if (c >= '0' && c <= '9') continue;
        if (hex && ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) continue;
        return false;
    }
    return true;
}
inline bool valid_numeric_scalar(unsigned long cp) noexcept {
    return cp != 0 && cp <= 0x10FFFFul && !(cp >= 0xD800ul && cp <= 0xDFFFul);
}
} // namespace discogger_unicode
