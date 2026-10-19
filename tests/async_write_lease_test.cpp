#include "../discogger_async_write_lease.h"
#include <atomic>
#include <cassert>
#include <thread>
#include <vector>
int main() {
    discogger_write_safety::async_write_gate gate;
    assert(!gate.busy());
    auto task = gate.acquire();
    assert(task && gate.busy() && !gate.acquire());
    auto completion = task;
    auto artwork = completion;
    task.reset();
    assert(!gate.acquire());
    completion.reset();
    assert(!gate.acquire());
    artwork.reset();
    assert(!gate.busy());
    auto held = gate.acquire();
    std::atomic<int> denied{0};
    std::vector<std::thread> workers;
    for(int i=0; i<8; ++i) workers.emplace_back([&]{ if (!gate.acquire()) ++denied; });
    for(auto& worker : workers) worker.join();
    assert(denied == 8);
    held.reset();
    assert(gate.acquire());
}
