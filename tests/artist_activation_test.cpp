#include "../discogger_artist_activation.h"
#include "../discogger_artist_numeric.h"
#include <cassert>
#include <cstddef>
int main() {
    using discogger_ui::should_load_artist_on_double_click;
    using discogger_ui::valid_artist_list_row;
    using discogger_ui::valid_artist_context_target;
    const std::size_t invalid = static_cast<std::size_t>(-1);
    // Do not refresh or open a different artist when modal menu state ages.
    assert(valid_artist_context_target(0, 0, true));
    assert(valid_artist_context_target(2, 2, true));
    assert(!valid_artist_context_target(0, 1, true));
    assert(!valid_artist_context_target(1, 1, false));
    assert(!valid_artist_context_target(invalid, invalid, true));
    assert(valid_artist_list_row(0, 1));
    assert(valid_artist_list_row(1, 2));
    assert(!valid_artist_list_row(-1, 3));
    assert(!valid_artist_list_row(0, 0));
    assert(!valid_artist_list_row(2, 2));
    assert(!valid_artist_list_row(2147483647, 1));
    assert(should_load_artist_on_double_click(0, 0, 1));
    assert(should_load_artist_on_double_click(2, 2, 3));
    assert(!should_load_artist_on_double_click(-1, 0, 1)); // blank area
    assert(!should_load_artist_on_double_click(0, invalid, 1)); // no selection
    assert(!should_load_artist_on_double_click(2, 1, 3)); // old selection
    assert(!should_load_artist_on_double_click(2, 2, 2)); // stale row
    assert(!should_load_artist_on_double_click(0, 0, 0)); // empty list
    assert(!should_load_artist_on_double_click(-2, invalid, 0));
    using discogger_ui::artist_id_or_invalid;
    using discogger_ui::is_tracer_artist_id;
    assert(artist_id_or_invalid("101") == 101);
    assert(artist_id_or_invalid("000101") == 101);
    assert(artist_id_or_invalid("38012673") == 38012673);
    assert(artist_id_or_invalid("2147483647") == 2147483647u);
    assert(artist_id_or_invalid("0") == invalid);
    assert(artist_id_or_invalid("000") == invalid);
    assert(artist_id_or_invalid("0", true) == 0);
    assert(artist_id_or_invalid("00", true) == invalid);
    assert(artist_id_or_invalid("") == invalid);
    assert(artist_id_or_invalid("-1") == invalid);
    assert(artist_id_or_invalid("+101") == invalid);
    assert(artist_id_or_invalid("101garbage") == invalid);
    assert(artist_id_or_invalid(" 101") == invalid);
    assert(artist_id_or_invalid("101 ") == invalid);
    assert(artist_id_or_invalid("2147483648") == invalid);
    assert(artist_id_or_invalid("999999999999999999999") == invalid);
    assert(!is_tracer_artist_id(artist_id_or_invalid("0", true)));
    assert(!is_tracer_artist_id(artist_id_or_invalid("not-a-number")));
    assert(is_tracer_artist_id(artist_id_or_invalid("000101")));
    return 0;
}
