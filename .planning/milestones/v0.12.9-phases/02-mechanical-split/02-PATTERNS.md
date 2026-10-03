# Phase 2: Mechanical Split - Pattern Map

**Mapped:** 2026-10-02
**Files analyzed:** 11 new (src/lua_runner/*) + ~25 modified (CMake, sync test, citations)
**Analogs found:** 11 / 11 (every new TU is carved from `src/lua_runner.cpp`; the header follows `src/database_internal.h`)

Line ranges below refer to the current `src/lua_runner.cpp` (byte-identical to `bdf9087`). The full per-file range table is RESEARCH.md "Cluster-to-file assignment"; it is not repeated here.

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match |
|---|---|---|---|---|
| `src/lua_runner/lua_runner.cpp` (git mv) | Pimpl owner / lifecycle | request-response | `src/lua_runner.cpp` 238-259, 298-342, 2491-2539 | exact (source) |
| `src/lua_runner/internal.h` | shared internal header | transform | `src/database_internal.h` (structure), `src/csv/csv_read.h` (guard) | exact |
| `src/lua_runner/return_json.cpp` | utility (leaf) | transform | `src/lua_runner.cpp` 37-236 | exact |
| `src/lua_runner/path_policy.cpp` | utility (security gate) | file-I/O | `src/lua_runner.cpp` 1298-1357 | exact |
| `src/lua_runner/db_core.cpp` | binder | request-response | `src/lua_runner.cpp` 510-532, 1359-1396, 1884-1936; binder 602-692, 740-755 | exact |
| `src/lua_runner/db_read.cpp` | binder | CRUD (read) | 1706-1769, 1938-2087; binder 689-713 | exact |
| `src/lua_runner/db_write.cpp` | binder | CRUD (write) | 1575-1704, 2170-2307; binder 595-600, 687, 715-724 | exact |
| `src/lua_runner/db_metadata.cpp` | binder | request-response | 1771-1882, 2093-2111; binder 729-737 | exact |
| `src/lua_runner/db_time_series.cpp` | binder | CRUD | 1549-1573, 2113-2156, 2309-2488; binder 609-610, 706-738 | exact |
| `src/lua_runner/csv.cpp` | binder + usertype | streaming file-I/O | 261-285, 344-471, 534-588, 1398-1448; binder 784-943 | exact |
| `src/lua_runner/binary.cpp` | binder + usertypes | file-I/O | 1102-1296; binder 757-782, 955-1094 | exact |
| `src/CMakeLists.txt` | config | — | own lines 17, 61-74 | exact |
| `bindings/js/test/lua-api-sync.test.ts` | test | file-I/O | own lines 5-6, 10-11, 29-45, 55-57 | exact |
| Citation sites (AGENTS.md x6, `src/csv/*`, `cmake/Platform.cmake`, `bindings/dart/hook/build.dart`, `lua-api.ts` header, 2 test comments) | docs/comments | — | RESEARCH.md "Citation Inventory" | n/a |

## Pattern Assignments

### `src/lua_runner/internal.h` (shared header)

**Analog:** `src/database_internal.h` lines 1-30 — guard, quoted project includes, then `<std>` includes, then a nested `namespace quiver::internal {` with `inline` non-template helpers:
```cpp
#ifndef QUIVER_DATABASE_INTERNAL_H
#define QUIVER_DATABASE_INTERNAL_H

#include "quiver/attribute_metadata.h"
#include "quiver/value.h"
#include "schema.h"

#include <optional>
...
namespace quiver::internal {

inline std::optional<int64_t> get_row_value(const Row& row, size_t index, int64_t*) {
```
Apply: guard `QUIVER_SRC_LUA_RUNNER_INTERNAL_H` (spelling from `src/csv/csv_read.h:1-2`, `QUIVER_SRC_CSV_CSV_READ_H`); namespace `quiver::lua_internal`. Needs forward decls of `quiver::BinaryFile` and `quiver::csv_write::Writer`, so open `namespace quiver {` and nest (skeleton in RESEARCH.md Code Examples, lines ~543-589).
Content moved verbatim: `to_lua_table` x3 (1450-1481), `is_lua_boolean` (1483-1489, add `inline`), `lua_cell_as<T>` (1491-1512), `lua_to_value` (1514-1534, add `inline`), `lua_table_to_vector<T>` (1536-1547), `csv_options_entries` (473-508, add `inline`), `GroupColumn` + decoder decls (2158-2168), `RunHandles` (from 287-296), binder/`resolve_sandboxed_path`/`encode_return_json`/`table_to_element` decls.
Rules: no `static`, no anonymous namespace in the header. Never includes `csv/csv_read.h` (csv-parser).

### Every new `.cpp` (common skeleton)

**Analog include style:** `src/database_csv_import.cpp:1-8` — internal headers by src-relative path (`"csv/csv_read.h"`, `"database_internal.h"`), then `quiver/...`, sorted, clang-format grouped. Apply:
```cpp
#include "lua_runner/internal.h"   // never bare "internal.h" (src/c/internal.h exists)
#include "quiver/..."              // only what this TU uses, from src/lua_runner.cpp:1-33

#include <sol/sol.hpp>

#include <...>

namespace quiver::lua_internal {
namespace {
// file-local helpers (helpers first, in dependency order)
}  // namespace

// NOLINTBEGIN(performance-unnecessary-value-param) sol2 lambda bindings require pass-by-value ...
void bind_xxx(sol::usertype<Database>& bind) {
    bind.set_function("name", &wrapper);
}
// NOLINTEND(performance-unnecessary-value-param)

}  // namespace quiver::lua_internal
```
Anonymous-namespace closer style matches `src/lua_runner.cpp:236` (`}  // namespace`).

### NOLINT pairs
**Analog:** `src/lua_runner.cpp:591/685, 953/1016, 1026/1095, 1248/...` — currently spelled `performance-unnecessary-value-parameter` (wrong; suppresses nothing). Fix to `performance-unnecessary-value-param` in commit 2; after extraction one pair each in `db_core.cpp`, `db_write.cpp`, `db_time_series.cpp`, `csv.cpp`, `binary.cpp`. Keep the existing trailing rationale text.

### `src/lua_runner/lua_runner.cpp` (root)
Source: 238-259 (Impl + ctor, rewritten to: `open_libraries` → nil `dofile`/`loadfile` → `ns` = `quiver` table → `auto bind = lua.new_usertype<Database>("Database");` → 7 binders → `lua["db"] = &db`), 298-342 (become `RunHandles::path_has_open_writer` / `RunHandles::close_open_writers`), 2491-2539 (Pimpl + `run()` + GcGuard; dtor becomes `impl.handles.close_open_writers(); impl.lua.collect_garbage();`). Member order `Database& db; lua_internal::RunHandles handles; sol::state lua;`. Only TU that includes `csv/csv_write.h` + `quiver/binary/binary_file.h` for the RunHandles bodies. Comments must not spell `new_usertype<Database>` or `open_libraries(`.

### `return_json.cpp` / `path_policy.cpp`
`return_json.cpp`: keep 37-236 anon namespace whole; add named `encode_return_json(const sol::object&)` wrapping the existing entry. `path_policy.cpp`: 1298-1357 moved into named `quiver::lua_internal`, no sol2 types in its logic.

### `csv.cpp`
`struct CsvWriter` (261-285) stays in the **named** `quiver::lua_internal` namespace (sol2 usertype key demangles anon namespaces away). Move `csv_cell_to_string` (394-450) above `csv_row_cells_from_lua` (374-392). Captures at 876: `[this]` → `[&handles]`. Budget: ≤ ~450 lines (prototype 445); fallback in RESEARCH Pitfall 2.

### `binary.cpp`
`ns` arrives as parameter (replaces `lua["quiver"]` at 951/1024). Captures: 761 `[this]` → `[&handles]` (`handles.open_binary_files.push_back`), 1032 `[this]` → `[&db]`. The three non-Database usertypes stay variadic, one name per line (sync-test Pass 2 depends on `"name",` lines). `bind_expression_operators<T>` (1248-1276) placed after its helpers, file-local.

### `db_metadata.cpp`
Place `lua_data_type_name`, `scalar_metadata_lua`, `group_metadata_lua` before the list/get wrappers (Pattern 5).

### `db_write.cpp`
Defines (named namespace, declared in header) `table_to_element` (1593-1636) and `collect_group_columns` / `columns_to_cpp_rows` / `join_column_names` (2170-2231). `require_dense_array` (1575-1591) stays anon.

### `src/CMakeLists.txt`
**Analog:** own `QUIVER_SOURCES` list — one relative path per line, 4-space indent, alphabetical within the folder (cf. `csv/csv_read.cpp`). Replace line 17 with the 11 `lua_runner/...` entries (incl. `internal.h`); replace 68-74 per-file `set_source_files_properties(... /bigobj)` with target-wide `target_compile_options(quiver PRIVATE /bigobj)` / `-Wa,-mbig-obj` under the same `if(MSVC)/elseif(WIN32)`. Leave defines block 61-66 and `c/lua_runner.cpp` (131) untouched. Exact snippet: RESEARCH.md Code Examples.

### `bindings/js/test/lua-api-sync.test.ts`
Replace `CPP_PATH`/`CPP` (10-11) with sorted `readdirSync` of `src/lua_runner` filtered to `.cpp|.h`; `CPP = SOURCES.join("\n")` for Pass 1 and the `open_libraries(` count; wrap the Pass 2 loop (29-45) in `for (const source of SOURCES)` with `let current = ""` per file; retitle describe (55-57); fix line-17 comment (`bind_expression` gone). Exact code: RESEARCH.md "Sync Test Change". Keep Biome-clean style of the existing file.

## Shared Patterns

- **Linkage:** header = templates + `inline`; each `.cpp` = anon namespace nested in `quiver::lua_internal`; no same-named helper in two TUs.
- **Registration:** `bind.set_function("name", fn)` for Database (17 variadic pairs converted); binder params named literally `bind` and `ns` (sync test regex `\b(bind|ns)\.set_function`).
- **Errors:** message strings move byte-for-byte (3 C++ patterns, root AGENTS.md).
- **Formatting:** `uvx --from clang-format==22.1.8 clang-format -i`, before running the sync test.
- **No planning IDs** in moved code or comments.

## No Analog Found

None. All new files are carved from one existing TU.

## Metadata

**Analog search scope:** `src/lua_runner.cpp`, `src/database_internal.h`, `src/csv/csv_read.h`, `src/database_csv_import.cpp`, `src/CMakeLists.txt`, `bindings/js/test/lua-api-sync.test.ts` (via RESEARCH.md)
**Pattern extraction date:** 2026-10-02
