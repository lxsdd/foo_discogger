#pragma once
#include <atomic>
#include <memory>
namespace discogger_write_safety {
class async_write_gate {
    std::atomic<bool> busy_{false};
    struct token {
        explicit token(async_write_gate* gate) : gate_(gate) {}
        ~token() { gate_->busy_.store(false, std::memory_order_release); }
        async_write_gate* gate_;
    };
public:
    using lease = std::shared_ptr<void>;
    lease acquire() {
        bool expected = false;
        if (!busy_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) return {};
        try { return lease(new token(this), [](void* p){ delete static_cast<token*>(p); }); }
        catch (...) { busy_.store(false, std::memory_order_release); throw; }
    }
    bool busy() const { return busy_.load(std::memory_order_acquire); }
};
inline async_write_gate& physical_write_gate() {
    // Process-lifetime storage avoids dangling gate during late completion callback.
    static auto* gate = new async_write_gate();
    return *gate;
}
inline async_write_gate::lease acquire_physical_write_lease() {
    return physical_write_gate().acquire();
}
}
