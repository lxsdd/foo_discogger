#include "../discogger_release_id.h"
#include <cassert>
#include <climits>
#include <string>
int main() {
    using discogger_search::valid_release_id;
    for (const char* id : {
        "38012673", "37332711", "36397609", "36819730",
        "35534398", "1", "2147483647", "0000001"
    }) assert(valid_release_id(id));
    for (const char* bad : {
        "", "0", "00000", "-1", "+1", "2147483648", "99999999999999999999",
        "123abc", "1e3", "12 3", " 123", "123 ", "/release/123", "0x123"
    }) assert(!valid_release_id(bad));
    assert(!valid_release_id(std::string(4096, '9')));
    return 0;
}
