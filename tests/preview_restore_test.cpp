#include "../discogger_preview_restore.h"
#include <cassert>
#include <cstddef>
#include <string>
#include <vector>
#include <utility>

struct synthetic_values {
    std::vector<std::string> values;
    std::size_t get_count() const { return values.size(); }
    const std::string& operator[](std::size_t i) const { return values.at(i); }
    void append_single(const std::string& s) { values.push_back(s); }
};

int main() {
    using discogger_preview::restore_original_values;
    using discogger_preview::safe_value_index;
    const std::size_t bad = static_cast<std::size_t>(-1);
    assert(safe_value_index(0, 1, 1) == 0);
    assert(safe_value_index(3, 1, 4) == 0);
    assert(safe_value_index(2, 4, 4) == 2);
    assert(safe_value_index(0, 0, 4) == bad);
    assert(safe_value_index(3, 3, 4) == bad);
    assert(safe_value_index(4, 4, 4) == bad);
    assert(safe_value_index(0, 1, 0) == bad);
    assert(safe_value_index(bad, 1, 1) == bad);
    synthetic_values lead{{"Modified", "Other"}};
    const synthetic_values originals{{"Old Genre", "Existing Custom"}};
    assert(restore_original_values(lead, originals, 2));
    assert((lead.values == originals.values));
    assert((originals.values == std::vector<std::string>{"Old Genre", "Existing Custom"}));
    // A shared original must stay collapsed, so every track displays it.
    const synthetic_values shared{{"Original for all"}};
    assert(restore_original_values(lead, shared, 4));
    assert(lead.values.size() == 1 && lead.values[0] == "Original for all");
    // Empty or stale/mismatched values must not destroy pending edits.
    const synthetic_values invalid{{"A", "B", "C"}};
    assert(!restore_original_values(lead, invalid, 2));
    assert(lead.values[0] == "Original for all");
    assert(!restore_original_values(lead, synthetic_values{}, 2));
    assert(!restore_original_values(lead, shared, 0));
    assert(lead.values.size() == 1);
    // Single-track and multi-valued originals (represented as a string)
    // roundtrip without filtering metadata.
    assert(restore_original_values(lead, synthetic_values{{"Genre=A;Genre=B"}}, 1));
    assert(lead.values[0] == "Genre=A;Genre=B");
    return 0;
}
