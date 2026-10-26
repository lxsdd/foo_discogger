#include "../interruptible_wait.h"
#include <cassert>
#include <chrono>
#include <stdexcept>

struct abort_signal : std::runtime_error { abort_signal() : std::runtime_error("cancel") {} };
struct probe {
    int checks = 0;
    int abort_after = 0;
    void check() { ++checks; if (abort_after && checks >= abort_after) throw abort_signal(); }
};
int main() {
    probe zero;
    discogger_net::interruptible_wait(0, zero);
    assert(zero.checks >= 2);
    probe completed;
    discogger_net::interruptible_wait(60, completed);
    assert(completed.checks >= 3);
    probe cancelled;
    cancelled.abort_after = 2;
    bool threw = false;
    const auto start = std::chrono::steady_clock::now();
    try { discogger_net::interruptible_wait(40000, cancelled); }
    catch (const abort_signal&) { threw = true; }
    assert(threw);
    assert(std::chrono::steady_clock::now() - start < std::chrono::seconds(2));
}
