#include "../sqlite3ren.h"
#include <string_view>
#define DISCOGGER_S2(x) #x
#define DISCOGGER_S(x) DISCOGGER_S2(x)
static_assert(std::string_view(DISCOGGER_S(sqlite3_close)) == "discogger_sqlite3_close", "namespaced API sqlite3_close");
static_assert(std::string_view(DISCOGGER_S(sqlite3_exec)) == "discogger_sqlite3_exec", "namespaced API sqlite3_exec");
static_assert(std::string_view(DISCOGGER_S(sqlite3_prepare_v2)) == "discogger_sqlite3_prepare_v2", "namespaced API sqlite3_prepare_v2");
static_assert(std::string_view(DISCOGGER_S(sqlite3_bind_parameter_index)) == "discogger_sqlite3_bind_parameter_index", "namespaced API sqlite3_bind_parameter_index");
static_assert(std::string_view(DISCOGGER_S(sqlite3_bind_int)) == "discogger_sqlite3_bind_int", "namespaced API sqlite3_bind_int");
static_assert(std::string_view(DISCOGGER_S(sqlite3_bind_text)) == "discogger_sqlite3_bind_text", "namespaced API sqlite3_bind_text");
static_assert(std::string_view(DISCOGGER_S(sqlite3_step)) == "discogger_sqlite3_step", "namespaced API sqlite3_step");
static_assert(std::string_view(DISCOGGER_S(sqlite3_reset)) == "discogger_sqlite3_reset", "namespaced API sqlite3_reset");
static_assert(std::string_view(DISCOGGER_S(sqlite3_column_text)) == "discogger_sqlite3_column_text", "namespaced API sqlite3_column_text");
static_assert(std::string_view(DISCOGGER_S(sqlite3_finalize)) == "discogger_sqlite3_finalize", "namespaced API sqlite3_finalize");
static_assert(std::string_view(DISCOGGER_S(sqlite3_config)) == "discogger_sqlite3_config", "namespaced API sqlite3_config");
static_assert(std::string_view(DISCOGGER_S(sqlite3_initialize)) == "discogger_sqlite3_initialize", "namespaced API sqlite3_initialize");
static_assert(std::string_view(DISCOGGER_S(sqlite3_open_v2)) == "discogger_sqlite3_open_v2", "namespaced API sqlite3_open_v2");
static_assert(std::string_view(DISCOGGER_S(sqlite3_errmsg)) == "discogger_sqlite3_errmsg", "namespaced API sqlite3_errmsg");
static_assert(std::string_view(DISCOGGER_S(sqlite3_busy_timeout)) == "discogger_sqlite3_busy_timeout", "namespaced API sqlite3_busy_timeout");
// Negative contract: unknown SQLite-like names must not acquire arbitrary prefixes.
static_assert(std::string_view(DISCOGGER_S(sqlite3_nonexistent_fixture)) == "sqlite3_nonexistent_fixture", "unexpected macro pollution");
int main() { return 0; }
