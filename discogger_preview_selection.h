#pragma once
#include <cstddef>

namespace discogger_ui {
struct preview_detail_choice {
    bool valid = false;
    std::size_t index = 0;
};

// A regenerated tag list may be empty, shorter than the old list, or contain
// an ineligible currently selected entry. Never underflow size_t or select a
// row beyond the freshly generated tag results.
constexpr preview_detail_choice choose_preview_detail(
    std::size_t current, std::size_t count, bool current_enabled) noexcept {
    if (count == 0) return {false, 0};
    if (current >= count) return {true, count - 1};
    if (!current_enabled && current > 0) return {true, current - 1};
    return {true, current};
}
} // namespace discogger_ui
