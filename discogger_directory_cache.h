#pragma once
#include "discogger_atomic_cache_file.h"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <string>
#include <system_error>

namespace discogger_cache {
namespace fs = std::filesystem;
inline std::mutex& directory_cache_mutex() {
    static std::mutex m;
    return m;
}
inline fs::path cache_backup_path(const fs::path& active) {
    fs::path p = active; p += ".rollback"; return p;
}
inline bool cache_has_marker(const fs::path& root) {
    std::error_code ec;
    return fs::is_directory(root, ec) && !ec &&
        fs::is_regular_file(root / "taskreg.txt", ec) && !ec &&
        !fs::is_symlink(root / "taskreg.txt", ec) && !ec;
}
inline bool recover_cache_unlocked(const fs::path& active) {
    const fs::path backup = cache_backup_path(active);
    std::error_code ec;
    if (fs::is_symlink(active, ec) || ec) return false;
    if (fs::is_symlink(backup, ec) || ec) return false;
    const bool backup_exists = fs::exists(backup, ec);
    if (ec || !backup_exists) return !ec;
    const bool active_exists = fs::exists(active, ec);
    if (ec) return false;
    if (active_exists && cache_has_marker(active)) {
        fs::remove_all(backup, ec);
        return !ec;
    }
    if (active_exists) {
        fs::remove_all(active, ec);
        if (ec) return false;
    }
    fs::rename(backup, active, ec);
    return !ec;
}
inline bool recover_cache_directory(const fs::path& active) {
    std::lock_guard<std::mutex> lock(directory_cache_mutex());
    return recover_cache_unlocked(active);
}
class directory_cache_transaction {
public:
    explicit directory_cache_transaction(fs::path destination)
        : active_(std::move(destination)), backup_(cache_backup_path(active_)),
          lock_(directory_cache_mutex()) {
        std::error_code ec;
        if (!recover_cache_unlocked(active_)) return;
        fs::create_directories(active_.parent_path(), ec);
        if (ec || !fs::is_directory(active_.parent_path(), ec) || ec) return;
        static std::atomic<unsigned long long> counter{0};
        stage_ = active_;
        stage_ += ".staging-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
            "-" + std::to_string(counter.fetch_add(1));
        ready_ = fs::create_directory(stage_, ec) && !ec;
    }
    directory_cache_transaction(const directory_cache_transaction&) = delete;
    directory_cache_transaction& operator=(const directory_cache_transaction&) = delete;
    ~directory_cache_transaction() {
        if (ready_ && !committed_) {
            std::error_code ec;
            fs::remove_all(stage_, ec);
        }
    }
    bool ready() const { return ready_; }
    const fs::path& staging_directory() const { return stage_; }

    bool publish(const std::string& marker, const std::function<void()>& failpoint = {}) {
        if (!ready_ || committed_) return false;
        const bool marker_ok = atomic_cache_replace(stage_ / "taskreg.txt",
            [&](const fs::path& path) {
                std::ofstream out(path, std::ios::binary | std::ios::trunc);
                if (!out) return false;
                out.write(marker.data(), static_cast<std::streamsize>(marker.size()));
                out.flush();
                const bool ok = out.good();
                out.close();
                return ok && out.good();
            });
        if (!marker_ok || !cache_has_marker(stage_)) return false;
        std::error_code ec;
        const bool prior = fs::exists(active_, ec);
        if (ec) return false;
        if (prior) {
            if (fs::exists(backup_, ec) || ec) return false;
            fs::rename(active_, backup_, ec);
            if (ec) return false;
        }
        bool switched = false;
        try {
            if (failpoint) failpoint();
            fs::rename(stage_, active_, ec);
            switched = !ec;
        } catch (...) { switched = false; }
        if (!switched) {
            if (prior) {
                std::error_code restore_ec;
                fs::rename(backup_, active_, restore_ec);
                // Failed rollback remains on disk for next recovery.
            }
            return false;
        }
        committed_ = true;
        if (prior) {
            std::error_code cleanup_ec;
            fs::remove_all(backup_, cleanup_ec);
            // Leftover backup is cleaned on the next read.
        }
        return true;
    }
private:
    fs::path active_, backup_, stage_;
    std::unique_lock<std::mutex> lock_;
    bool ready_ = false, committed_ = false;
};
} // namespace discogger_cache
