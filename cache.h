#pragma once

#include <algorithm>
#include <cstddef>
#include <list>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <utility>

// Bounded LRU for metadata objects. The returned value is a copy: callers
// must not hold references to cache nodes after the lock is released.
template <typename key_t, typename value_t>
class lru_cache {
public:
    using key_value_pair_t = std::pair<key_t, value_t>;
    using list_iterator_t = typename std::list<key_value_pair_t>::iterator;

    explicit lru_cache(size_t max_size) : _max_size(max_size) {}

    // Existing Discogger code uses this to serialize a release load. Keep it
    // public until loading has its own per-release synchronization mechanism.
    std::mutex modify_mutex;

    bool remove(const key_t& key) {
        std::lock_guard<std::mutex> lock(modify_mutex);
        return remove_unlocked(key);
    }

    void put(const key_t& key, const value_t& value) {
        std::lock_guard<std::mutex> lock(modify_mutex);
        if (_max_size == 0) return;
        remove_unlocked(key);
        _cache_items_list.emplace_front(key, value);
        try {
            _cache_items_map.emplace(key, _cache_items_list.begin());
        } catch (...) {
            _cache_items_list.pop_front();
            throw;
        }
        trim_unlocked();
    }

    value_t get(const key_t& key) {
        std::lock_guard<std::mutex> lock(modify_mutex);
        auto it = _cache_items_map.find(key);
        if (it == _cache_items_map.end()) throw std::out_of_range("cache error");
        _cache_items_list.splice(_cache_items_list.begin(), _cache_items_list, it->second);
        return it->second->second;
    }

    // Atomic lookup + copy, avoiding the race between exists() and get().
    bool try_get(const key_t& key, value_t& out) {
        std::lock_guard<std::mutex> lock(modify_mutex);
        auto it = _cache_items_map.find(key);
        if (it == _cache_items_map.end()) return false;
        out = it->second->second;
        _cache_items_list.splice(_cache_items_list.begin(), _cache_items_list, it->second);
        return true;
    }

    bool exists(const key_t& key) {
        std::lock_guard<std::mutex> lock(modify_mutex);
        return _cache_items_map.find(key) != _cache_items_map.end();
    }

    // For encoded release keys, the zero-role key takes precedence. If neither
    // exact key exists, return the *complete encoded key* of the most recently
    // used release with the same Discogs ID (not merely its role index).
    // Only used for size_t release/master-release caches.
    std::pair<bool, bool> exists_comp(const key_t& key, key_t& alternate_key) {
        std::lock_guard<std::mutex> lock(modify_mutex);
        const auto decoded = decode_mr(key);
        const key_t simple_key = static_cast<key_t>(decoded.second);
        const bool simple = _cache_items_map.find(simple_key) != _cache_items_map.end();
        const bool exact = _cache_items_map.find(key) != _cache_items_map.end();
        alternate_key = key_t{};
        if (!simple && !exact) {
            for (const auto& item : _cache_items_list) {
                if (decode_mr(item.first).second == decoded.second) {
                    alternate_key = item.first;
                    break;
                }
            }
        }
        return {simple, exact};
    }

    size_t size() {
        std::lock_guard<std::mutex> lock(modify_mutex);
        return _cache_items_map.size();
    }

    void clear() {
        std::lock_guard<std::mutex> lock(modify_mutex);
        _cache_items_list.clear();
        _cache_items_map.clear();
    }

    void set_max_size(size_t max_size) {
        std::lock_guard<std::mutex> lock(modify_mutex);
        _max_size = max_size;
        trim_unlocked();
    }

private:
    bool remove_unlocked(const key_t& key) {
        auto it = _cache_items_map.find(key);
        if (it == _cache_items_map.end()) return false;
        _cache_items_list.erase(it->second);
        _cache_items_map.erase(it);
        return true;
    }

    void trim_unlocked() {
        while (_cache_items_map.size() > _max_size) {
            auto last = std::prev(_cache_items_list.end());
            _cache_items_map.erase(last->first);
            _cache_items_list.pop_back();
        }
    }

    std::list<key_value_pair_t> _cache_items_list;
    std::unordered_map<key_t, list_iterator_t> _cache_items_map;
    size_t _max_size;
};

namespace std {
template <>
struct hash<pfc::string8> {
    size_t operator()(const pfc::string8& value) const {
        const unsigned char* str = reinterpret_cast<const unsigned char*>(value.get_ptr());
        size_t hash_value = 5381;
        while (*str) hash_value = ((hash_value << 5) + hash_value) + *str++;
        return hash_value;
    }
};

template <>
struct equal_to<pfc::string8> {
    bool operator()(const pfc::string8& x, const pfc::string8& y) const {
        return STR_EQUAL(x, y);
    }
};
} // namespace std
