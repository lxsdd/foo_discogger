#pragma once
#include <chrono>
#include <algorithm>
namespace discogger_cache {
enum class group { artist_releases, other_entity };
inline int artist_hours(int n) noexcept { return std::clamp(n, 1, 720); }
inline int entity_days(int n) noexcept { return std::clamp(n, 1, 365); }
inline std::chrono::seconds ttl(group kind, int artist_release_hours,
                                bool general_enabled, int general_days) noexcept {
    if (kind == group::artist_releases)
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::hours(artist_hours(artist_release_hours)));
    if (!general_enabled) return (std::chrono::seconds::max)();
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::hours(24 * entity_days(general_days)));
}
inline bool expired(std::chrono::seconds age, std::chrono::seconds lifetime) noexcept {
    if (lifetime == (std::chrono::seconds::max)()) return false;
    if (age < -std::chrono::minutes(5)) return true;
    return age >= lifetime;
}
 // A disk-loaded list remains valid only while its original marker is fresh;
 // a network-loaded list has its own bounded in-memory lifetime.
 inline bool reuse_artist_releases(bool loaded, bool from_disk, bool disk_read_enabled,
                                   bool disk_current, bool force_overwrite,
                                   std::chrono::seconds memory_age,
                                   std::chrono::seconds lifetime) noexcept {
     if (!loaded || force_overwrite) return false;
     if (from_disk && disk_read_enabled) return disk_current;
     return memory_age >= std::chrono::seconds::zero() && !expired(memory_age, lifetime);
 }

}
