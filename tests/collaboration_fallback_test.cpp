#include "../discogger_search_fallback.h"
#include <cassert>

int main() {
    using discogger_search::trailing_collaborator;
    assert(trailing_collaborator("Kyau & Albert X Maywave") == "Maywave");
    assert(trailing_collaborator("Kyau & Albert x Maywave") == "Maywave");
    assert(trailing_collaborator("Kyau & Albert") == "");
    assert(trailing_collaborator("Maywave x Kyau & Albert") == "Kyau & Albert");
    assert(trailing_collaborator("Matt Fax feat. Lovlee") == "Lovlee");
    assert(trailing_collaborator("Quivver") == "");
    assert(trailing_collaborator("Artist VS. Guest") == "Guest");
    assert(trailing_collaborator("Artist x   ") == "");
    return 0;
}
