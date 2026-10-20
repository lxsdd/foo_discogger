#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
namespace discogger_cache {
namespace fs = std::filesystem;
// A failed serializer never truncates the published cache JSON.
template <typename Writer>
bool atomic_cache_replace(const fs::path& destination, Writer&& write_temp) {
    std::error_code ec;
    const fs::path parent = destination.parent_path();
    fs::create_directories(parent, ec);
    if (ec || !fs::is_directory(parent, ec) || ec) return false;
    static std::atomic<std::uint64_t> sequence{0};
    const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
#ifdef _WIN32
    const auto pid = static_cast<std::uint64_t>(GetCurrentProcessId());
#else
    const auto pid = static_cast<std::uint64_t>(getpid());
#endif
    fs::path temporary = destination;
    temporary += ".pending-" + std::to_string(pid) + "-" +
        std::to_string(tick) + "-" + std::to_string(sequence.fetch_add(1));
    bool written = false;
    try { written = write_temp(temporary); }
    catch (...) { written = false; }
    if (!written) {
        fs::remove(temporary, ec);
        return false;
    }
#ifdef _WIN32
    // Filesystem rename cannot overwrite an existing file on Windows.
    const bool replaced = MoveFileExW(temporary.c_str(), destination.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    fs::rename(temporary, destination, ec);
    const bool replaced = !ec;
#endif
    if (!replaced) fs::remove(temporary, ec);
    return replaced;
}
} // namespace discogger_cache
