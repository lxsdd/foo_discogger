#pragma once
// The historical Discogger sources only retain pointers to this optional
// legacy database-fetcher type. Upstream does not ship its old header.
// Keep the type opaque: do not invent database behavior or enable writes.
class db_fetcher;
