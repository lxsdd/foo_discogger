#include "../discogger_directory_cache.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
namespace fs = std::filesystem;
static void put(const fs::path& p, const std::string& text) {
    fs::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out << text; out.close(); assert(out.good());
}
static std::string read(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
int main() {
    std::error_code ec;
    const fs::path base = fs::temp_directory_path() / "discogger-txn-regression";
    fs::remove_all(base, ec);
    fs::create_directories(base);
    const fs::path active = base / "releases";
    { discogger_cache::directory_cache_transaction tx(active);
      assert(tx.ready()); put(tx.staging_directory() / "page-0/root.json", "old");
      assert(tx.publish("old-marker")); }
    assert(read(active / "page-0/root.json") == "old");
    assert(discogger_cache::cache_has_marker(active));
    { discogger_cache::directory_cache_transaction tx(base / "marker-only");
      assert(tx.ready()); assert(!tx.publish("")); }
    assert(!fs::exists(base / "marker-only"));
    { discogger_cache::directory_cache_transaction tx(base / "empty-payload");
      assert(tx.ready()); put(tx.staging_directory() / "root.json", "");
      assert(!tx.publish("marker")); }
    assert(!fs::exists(base / "empty-payload"));
    { discogger_cache::directory_cache_transaction tx(base / "gap");
      assert(tx.ready());
      put(tx.staging_directory() / "page-0/root.json", "zero");
      put(tx.staging_directory() / "page-2/root.json", "two");
      assert(!tx.publish("marker")); }
    assert(!fs::exists(base / "gap"));
    // A completed stamp without a rollback may still hide a missing page.
    // Recovery must leave it intact for re-fetch, but the read gate must
    // reject it until the contiguous payload is complete.
    const fs::path incomplete = base / "incomplete-no-backup";
    put(incomplete / "taskreg.txt", "complete-stamp");
    put(incomplete / "page-0/root.json", "first");
    put(incomplete / "page-2/root.json", "third");
    assert(discogger_cache::recover_cache_directory(incomplete));
    assert(!discogger_cache::cache_has_marker(incomplete));
    assert(read(incomplete / "page-0/root.json") == "first");
    put(incomplete / "page-1/root.json", "second");
    assert(discogger_cache::cache_has_marker(incomplete));
    { discogger_cache::directory_cache_transaction tx(base / "complete");
      assert(tx.ready());
      put(tx.staging_directory() / "page-0/root.json", "zero");
      put(tx.staging_directory() / "page-1/root.json", "one");
      assert(tx.publish("")); }
    assert(discogger_cache::cache_has_marker(base / "complete"));
    fs::copy(base / "complete", discogger_cache::cache_backup_path(base / "complete"),
             fs::copy_options::recursive);
    fs::remove(base / "complete/page-1/root.json");
    assert(!discogger_cache::cache_has_marker(base / "complete"));
    assert(discogger_cache::recover_cache_directory(base / "complete"));
    assert(read(base / "complete/page-1/root.json") == "one");
    assert(!fs::exists(discogger_cache::cache_backup_path(base / "complete")));
    // A damaged rollback must never replace or delete the current cache.
    const fs::path damaged = base / "damaged-rollback";
    { discogger_cache::directory_cache_transaction tx(damaged);
      assert(tx.ready()); put(tx.staging_directory() / "page-0/root.json", "original");
      assert(tx.publish("original-marker")); }
    const fs::path damaged_backup = discogger_cache::cache_backup_path(damaged);
    fs::copy(damaged, damaged_backup, fs::copy_options::recursive);
    fs::remove(damaged_backup / "page-0/root.json");
    fs::remove(damaged / "taskreg.txt");
    assert(!discogger_cache::recover_cache_directory(damaged));
    assert(read(damaged / "page-0/root.json") == "original");
    assert(!fs::exists(damaged / "taskreg.txt"));
    assert(!fs::exists(damaged_backup / "page-0/root.json"));
    put(damaged_backup / "page-0/root.json", "restored");
    assert(discogger_cache::recover_cache_directory(damaged));
    assert(read(damaged / "page-0/root.json") == "restored");
    assert(read(damaged / "taskreg.txt") == "original-marker");
    assert(!fs::exists(damaged_backup));
    fs::copy(active, discogger_cache::cache_backup_path(active),
             fs::copy_options::recursive);
    fs::remove(active / "page-0/root.json");
    assert(discogger_cache::recover_cache_directory(active));
    assert(read(active / "page-0/root.json") == "old");
    assert(!fs::exists(discogger_cache::cache_backup_path(active)));
    { discogger_cache::directory_cache_transaction tx(active);
      assert(tx.ready()); put(tx.staging_directory() / "page-0/root.json", "partial");
      // Serialization failed, no publication; destructor discards stage.
    }
    assert(read(active / "page-0/root.json") == "old");
    { discogger_cache::directory_cache_transaction tx(active);
      assert(tx.ready()); put(tx.staging_directory() / "page-0/root.json", "partial");
      assert(!tx.publish("bad", [] { throw std::runtime_error("injected"); })); }
    assert(read(active / "page-0/root.json") == "old");
    assert(!fs::exists(discogger_cache::cache_backup_path(active)));
    fs::rename(active, discogger_cache::cache_backup_path(active));
    assert(discogger_cache::recover_cache_directory(active));
    assert(read(active / "page-0/root.json") == "old");
    fs::copy(active, discogger_cache::cache_backup_path(active),
             fs::copy_options::recursive);
    put(active / "page-0/root.json", "new");
    assert(discogger_cache::recover_cache_directory(active));
    assert(read(active / "page-0/root.json") == "new");
    assert(!fs::exists(discogger_cache::cache_backup_path(active)));
    fs::copy(active, discogger_cache::cache_backup_path(active),
             fs::copy_options::recursive);
    fs::remove(active / "taskreg.txt");
    assert(discogger_cache::recover_cache_directory(active));
    assert(read(active / "page-0/root.json") == "new");
    assert(read(active / "taskreg.txt") == "old-marker");
    { discogger_cache::directory_cache_transaction tx(active);
      assert(tx.ready()); put(tx.staging_directory() / "page-0/root.json", "fresh0");
      put(tx.staging_directory() / "page-1/root.json", "fresh1");
      assert(tx.publish("ready")); }
    assert(read(active / "page-0/root.json") == "fresh0");
    assert(read(active / "page-1/root.json") == "fresh1");
    assert(!fs::exists(discogger_cache::cache_backup_path(active)));
    fs::remove_all(base, ec);
}
