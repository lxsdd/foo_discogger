#include "../crc.h"
#include <cassert>
#include <string>
int main() {
    const std::string empty;
    const std::string standard = "123456789";
    const std::string query = "artist=Kyau & Albert X Maywave & title=1000 Years";
    assert(discogger_crc::crc32(empty.begin(), empty.end()) == 0u);
    assert(discogger_crc::crc32(standard.begin(), standard.end()) == 0xCBF43926u);
    const std::string changed = query + " (DJ Version)";
    assert(discogger_crc::crc32(query.begin(), query.end()) !=
           discogger_crc::crc32(changed.begin(), changed.end()));
}
