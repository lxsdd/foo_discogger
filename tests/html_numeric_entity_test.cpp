#include "../discogger_html_numeric.h"
#include <cassert>
#include <climits>
int main() {
    using discogger_unicode::valid_numeric_digits;
    using discogger_unicode::valid_numeric_scalar;
    const char decimal[] = "1234", hex[] = "aF09", empty[] = "", sign[] = "+41", spaces[] = " 65", negative[] = "-1", mixed[] = "41z";
    assert(valid_numeric_digits(decimal, decimal + 4, false));
    assert(valid_numeric_digits(hex, hex + 4, true));
    assert(!valid_numeric_digits(hex, hex + 4, false));
    assert(!valid_numeric_digits(empty, empty, false));
    assert(!valid_numeric_digits(sign, sign + 3, false));
    assert(!valid_numeric_digits(spaces, spaces + 3, false));
    assert(!valid_numeric_digits(negative, negative + 2, false));
    assert(!valid_numeric_digits(mixed, mixed + 3, true));
    assert(!valid_numeric_digits(nullptr, decimal + 4, false));
    assert(valid_numeric_scalar(0x41));
    assert(valid_numeric_scalar(0xE9));
    assert(valid_numeric_scalar(0x1F3B5));
    assert(valid_numeric_scalar(0x10FFFF));
    assert(!valid_numeric_scalar(0));
    assert(!valid_numeric_scalar(0xD800));
    assert(!valid_numeric_scalar(0xDFFF));
    assert(valid_numeric_scalar(0xE000));
    assert(!valid_numeric_scalar(0x110000));
    assert(!valid_numeric_scalar(ULONG_MAX));
}
