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
    auto tag_writer = gate.acquire();
    assert(tag_writer);
    auto manual_art = gate.acquire();
    assert(!manual_art);
    tag_writer.reset();
    manual_art = gate.acquire();
    assert(manual_art && gate.busy());
    assert(!gate.acquire()); // Tag writer cannot race active manual artwork.
    manual_art.reset();
    assert(!gate.busy());
}
