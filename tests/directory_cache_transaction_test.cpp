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
    // Independent minimum counters must saturate regardless of enumeration
    // order. The historical unsigned countdown wrapped after extra files.
    {
        discogger_cache::cache_entry_requirements c{1, 1};
        c.observe_file(); c.observe_file();
        assert(!c.satisfied());
        c.observe_directory();
        assert(c.satisfied());
        c.observe_file(); c.observe_directory();
        assert(c.satisfied());
    }
    {
        discogger_cache::cache_entry_requirements c{1, 1};
        c.observe_directory(); c.observe_directory();
        assert(!c.satisfied());
        c.observe_file();
        assert(c.satisfied());
    }
    {
        discogger_cache::cache_entry_requirements c{2, 0};
        c.observe_directory(); c.observe_file();
        assert(!c.satisfied());
        c.observe_directory(); c.observe_file();
        assert(c.satisfied());
    }
    const fs::path inventory = base / "minimum-entry-inventory";
    const fs::path entity = inventory / "entity";
    put(entity / "root.json", "payload");
    put(entity / "taskreg.txt", "stamp");
    assert(discogger_cache::cache_has_minimum_entries(entity, 2, 0));
    assert(!discogger_cache::cache_has_minimum_entries(entity, 3, 0));
    assert(!discogger_cache::cache_has_minimum_entries(entity, 2, 1));
    assert(!discogger_cache::cache_has_minimum_entries(entity / "root.json", 1, 0));
    assert(!discogger_cache::cache_has_minimum_entries(inventory / "missing", 1, 0));
    assert(discogger_cache::cache_has_marker(entity));
    fs::create_directory(entity / "unexpected");
    assert(discogger_cache::cache_has_minimum_entries(entity, 2, 0));
    // Shape acceptance never overrides the strict completed-marker gate.
    assert(!discogger_cache::cache_has_marker(entity));
    const fs::path paged = inventory / "paged";
    put(paged / "taskreg.txt", "stamp");
    put(paged / "auxiliary.txt", "extra");
    put(paged / "page-0/root.json", "first");
    assert(discogger_cache::cache_has_minimum_entries(paged, 1, 1));
    assert(discogger_cache::cache_has_minimum_entries(paged, 2, 1));
    assert(discogger_cache::cache_has_minimum_entries(paged, 3, 1));
    assert(!discogger_cache::cache_has_minimum_entries(paged, 4, 1));
    assert(!discogger_cache::cache_has_minimum_entries(paged, 1, 2));
    assert(discogger_cache::cache_has_marker(paged));
    fs::create_directory(paged / "page-2");
    assert(discogger_cache::cache_has_minimum_entries(paged, 1, 1));
    assert(!discogger_cache::cache_has_marker(paged));
    fs::remove(paged / "page-2");
    assert(discogger_cache::cache_has_marker(paged));
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
    // A mixed entity/paged cache must not delete a complete rollback merely
    // because the root.json file and completion stamp both exist.
    const fs::path mixed = base / "mixed-root-and-pages";
    put(mixed / "taskreg.txt", "mixed-stamp");
    put(mixed / "root.json", "misleading-entity");
    put(mixed / "page-0/root.json", "misleading-page");
    assert(!discogger_cache::cache_has_marker(mixed));
    fs::copy(active, discogger_cache::cache_backup_path(mixed),
             fs::copy_options::recursive);
    assert(discogger_cache::recover_cache_directory(mixed));
    assert(read(mixed / "page-0/root.json") == "old");
    assert(!fs::exists(mixed / "root.json"));
    assert(!fs::exists(discogger_cache::cache_backup_path(mixed)));
    // The offline page reader rejects unexpected subdirectories. The
    // completion gate must agree and retain the last good rollback.
    const fs::path rogue = base / "unexpected-cache-directory";
    put(rogue / "taskreg.txt", "rogue-stamp");
    put(rogue / "page-0/root.json", "plausible-page");
    put(rogue / "unexpected/root.json", "unreadable-extra");
    assert(!discogger_cache::cache_has_marker(rogue));
    fs::copy(active, discogger_cache::cache_backup_path(rogue),
             fs::copy_options::recursive);
    assert(discogger_cache::recover_cache_directory(rogue));
    assert(read(rogue / "page-0/root.json") == "old");
    assert(!fs::exists(rogue / "unexpected"));
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
