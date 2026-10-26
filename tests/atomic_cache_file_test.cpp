#include "../discogger_atomic_cache_file.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
static void put(const std::filesystem::path& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << content;
    out.close();
    assert(out.good());
}
static std::string get(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
int main() {
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "discogger-atomic-cache-test";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);
    const fs::path dest = dir / "page" / "root.json";
    assert(discogger_cache::atomic_cache_replace(dest, [&](const fs::path& tmp) {
        put(tmp, "old"); return true;
    }));
    assert(get(dest) == "old");
    assert(!discogger_cache::atomic_cache_replace(dest, [&](const fs::path& tmp) {
        put(tmp, "partial"); return false;
    }));
    assert(get(dest) == "old");
    assert(!discogger_cache::atomic_cache_replace(dest, [&](const fs::path& tmp) -> bool {
        put(tmp, "partial"); throw std::runtime_error("failed serialization");
    }));
    assert(get(dest) == "old");
    assert(discogger_cache::atomic_cache_replace(dest, [&](const fs::path& tmp) {
        put(tmp, "new"); return true;
    }));
    assert(get(dest) == "new");
    assert(discogger_cache::atomic_cache_replace(dir / "new.json", [&](const fs::path& tmp) {
        put(tmp, "first"); return true;
    }));
    assert(get(dir / "new.json") == "first");
    for (const auto& entry : fs::recursive_directory_iterator(dir)) {
        assert(entry.path().filename().string().find(".pending-") == std::string::npos);
    }
    fs::remove_all(dir, ec);
}
