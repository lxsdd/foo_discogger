#pragma once
// A failed or unavailable track matcher must not silently approve a positional
// mapping. Only an explicitly enabled assume-sorted route may do so.
namespace discogger_match_policy {
enum class action { preserve, assume, disable };
constexpr action for_status(int status, int success, int assume) noexcept {
    return status == success ? action::preserve :
        status == assume ? action::assume : action::disable;
}
} // namespace discogger_match_policy
