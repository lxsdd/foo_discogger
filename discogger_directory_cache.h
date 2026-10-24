#pragma once
#include "discogger_atomic_cache_file.h"
#include "discogger_cache_pages.h"
#include <algorithm>
#include <vector>
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
// A completed marker does not prove that the JSON payload survived.
inline bool cache_payload_file(const fs::path& path) {
    std::error_code ec;
    if (fs::is_symlink(path, ec) || ec) return false;
    if (!fs::is_regular_file(path, ec) || ec) return false;
    const auto size = fs::file_size(path, ec);
    return !ec && size != 0;
}
inline bool cache_has_marker(const fs::path& root) {
    std::error_code ec;
    if (!fs::is_directory(root, ec) || ec ||
        fs::is_symlink(root, ec) || ec) return false;
    const fs::path marker = root / "taskreg.txt";
    // Empty markers are intentional for transient entity caches.
    if (!fs::is_regular_file(marker, ec) || ec ||
        fs::is_symlink(marker, ec) || ec) return false;
    const fs::path single = root / "root.json";
    if (fs::exists(single, ec)) return !ec && cache_payload_file(single);
    if (ec) return false;
    // A missing later page must not cause the last complete rollback to
    // be deleted merely because the new cache has a completed marker.
    std::vector<std::size_t> indexes;
    for (fs::directory_iterator it(root, ec), last; !ec && it != last;
         it.increment(ec)) {
        const std::string name = it->path().filename().string();
        if (name.rfind("page-", 0) != 0) continue;
        std::size_t index = 0;
        if (!parse_cache_page_folder(name, index) ||
            it->is_symlink(ec) || ec ||
            !it->is_directory(ec) || ec ||
            !cache_payload_file(it->path() / "root.json")) return false;
        indexes.push_back(index);
    }
    if (ec) return false;
    std::sort(indexes.begin(), indexes.end());
    return complete_cache_page_sequence(indexes);
}
inline bool recover_cache_unlocked(const fs::path& active) {
    const fs::path backup = cache_backup_path(active);
    std::error_code ec;
    // On POSIX, is_symlink(missing, ec) can report ENOENT. First
    // distinguish a new cache from an existing potentially unsafe link.
    const bool active_exists = fs::exists(active, ec);
    if (ec) return false;
    const bool backup_exists = fs::exists(backup, ec);
    if (ec) return false;
    if (active_exists && (fs::is_symlink(active, ec) || ec)) return false;
    if (backup_exists && (fs::is_symlink(backup, ec) || ec)) return false;
    if (!backup_exists) return true;
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
