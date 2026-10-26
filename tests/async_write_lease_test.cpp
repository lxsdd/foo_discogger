#include "../discogger_async_write_lease.h"
#include "../discogger_task_completion.h"
#include <atomic>
#include <cassert>
#include <thread>
#include <vector>
#include <stdexcept>
#include <string>
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

    // The modeless UI-thread completion callback must not crash foobar if
    // physical readback, artwork scheduling or error reporting throws.
    int completions = 0, reports = 0;
    std::string message;
    auto report = [&](const char* what) { ++reports; message = what; };
    discogger_async::run_success_with_error_boundary([&] { ++completions; }, report);
    assert(completions == 1 && reports == 0);
    discogger_async::run_success_with_error_boundary(
        [] { throw std::runtime_error("Physical metadata readback failed"); }, report);
    assert(reports == 1 && message == "Physical metadata readback failed");
    discogger_async::run_success_with_error_boundary([] { throw 42; }, report);
    assert(reports == 2 && message == "Unknown error completing task.");
    discogger_async::run_success_with_error_boundary(
        [] { throw std::runtime_error("Failed write"); },
        [](const char*) { throw std::runtime_error("Reporter failed"); });
}
