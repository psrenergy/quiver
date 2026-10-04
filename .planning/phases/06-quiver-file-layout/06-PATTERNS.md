# Phase 6: Quiver File Layout - Pattern Map

**Mapped:** 2026-10-04
**Files analyzed:** 19 (11 new/renamed binder TUs, 3 carved/edited TUs, internal.h, lua_runner.cpp, src/CMakeLists.txt, AGENTS.md, src/AGENTS.md, lua-api.ts:3)
**Analogs found:** 19 / 19. Every new file is a verbatim move, and its analog is the existing `src/lua_runner/` binder it comes from. RESEARCH.md §Complete Mapping and §Shared Helpers say which lines move where. This file covers only the skeleton.

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match |
|---|---|---|---|---|
| `src/lua_runner/database_read.cpp` | binder | request-response | `db_read.cpp` (git mv) | exact |
| `src/lua_runner/database_metadata.cpp` | binder | request-response | `db_metadata.cpp` (git mv) | exact |
| `src/lua_runner/database_time_series.cpp` | binder | request-response | `db_time_series.cpp` (git mv) | exact |
| `src/lua_runner/database_update.cpp` | binder | CRUD | `db_write.cpp` (git mv, then trimmed) | exact |
| `src/lua_runner/database.cpp`, `database_query.cpp`, `database_csv_export.cpp`, `database_csv_import.cpp` | binder | request-response / file-I/O | `db_core.cpp` (carved) | exact |
| `src/lua_runner/database_create.cpp`, `database_delete.cpp` | binder | CRUD | `db_write.cpp` (carved) | exact |
| `src/lua_runner/database_describe.cpp` | binder | request-response | `db_metadata.cpp` skeleton + `db_core.cpp:190-192` | exact |
| `src/lua_runner/binary.cpp` (trimmed, returns usertype) | binder | file-I/O | itself | exact |
| `src/lua_runner/expression.cpp` | binder | transform | `binary.cpp:99-343` | exact |
| `src/lua_runner/internal.h` | header (decls/templates) | — | itself :89-103, :295-311 | exact |
| `src/lua_runner/lua_runner.cpp` | wiring | — | itself :113-119 | exact |
| `src/CMakeLists.txt` | config | — | itself :17-28 | exact |
| `src/AGENTS.md`, `AGENTS.md`, `bindings/js/src/lua-api.ts:3` | docs | — | `src/AGENTS.md:46-58` | exact |

## Pattern Assignments

### Canonical binder TU skeleton (applies to every `database_*.cpp`, `expression.cpp`)

**Analog:** `src/lua_runner/db_metadata.cpp` (91 lines, the cleanest one)

**Includes** (lines 1-6). The project header comes first, then `quiver/*`, then `<sol/sol.hpp>`, then std headers, with a blank line between groups. Include only what the TU uses. Copy the include set from `db_core.cpp:1-13` and trim it per carved file.
```cpp
#include "lua_runner/internal.h"
#include "quiver/database.h"

#include <sol/sol.hpp>

#include <string>
```

**Namespace and private helpers** (lines 8-10, 77). Helpers go in an anonymous namespace nested inside `quiver::lua_internal`. Helpers shared across TUs are named in `lua_internal` and declared in internal.h.
```cpp
namespace quiver::lua_internal {

namespace {
// ... helpers (verbatim moves) ...
}  // namespace
```

**Binder** (lines 79-91). The parameter is `bind` (or `ns` for the quiver table). The sync test parses `bind.set_function(` / `ns.set_function(`. Member pointers go straight in; anything else goes through a named adapter or a lambda.
```cpp
void bind_metadata(sol::usertype<Database>& bind) {
    bind.set_function("get_scalar_metadata", &get_metadata_lua<&Database::get_scalar_metadata>);
    ...
}

}  // namespace quiver::lua_internal
```
Shape for small files (`database_delete.cpp`, `database_describe.cpp`), from `db_core.cpp:190-192`:
```cpp
void bind_describe(sol::usertype<Database>& bind) {
    bind.set_function("describe", &Database::describe);
    bind.set_function("describe_collection", &Database::describe_collection);
    bind.set_function("summarize_collection", &Database::summarize_collection);
}
```
Sandboxed file-op lambda (`db_core.cpp:198-201`, moves to `database.cpp`; the export/import ones follow the same shape). Its comment ("like the file I/O in binary.cpp") stays accurate after the move.
```cpp
    bind.set_function("validate_migrations", [](Database& self, const std::string& path) {
        Database::validate_migrations(resolve_sandboxed_path(self, "validate_migrations", path));
    });
```

**NOLINT pair.** One per TU, around the helper block plus the binder, with the closing `END` placed after the binder's `}` and before the namespace close. Example: `db_core.cpp:114` / `:203`, `binary.cpp:161-162` / `:343`.
```cpp
// NOLINTBEGIN(performance-unnecessary-value-param) sol2 lambda bindings require pass-by-value for type
// deduction
...
}
// NOLINTEND(performance-unnecessary-value-param)

}  // namespace quiver::lua_internal
```
This is required in `expression.cpp` (span: `bind_expression_operators` to the end of `bind_expression`). `csv.cpp` stays unchanged. `database_time_series.cpp` and `database_update.cpp` keep their pairs because their text moves verbatim. The carved db_core/create/delete files need none, since they have no by-value sol2 params. The gate is tidy uvp=0.

### `binary.cpp` / `expression.cpp`
**Analog:** `binary.cpp` itself. RESEARCH §Code Examples "bind_binary tail and bind_expression head" gives the shape. Hard rule: **`binary.cpp` keeps `#include "quiver/expression/expression.h"`**, with a one-line comment explaining why (sol2's auto `__lt/__le/__eq` on BinaryFile; see RESEARCH Pitfall 1). `binary_file_type` gets only `[sol::meta_function::...]` assignments and never `.set_function(`. `bind_expression`'s quiver-table parameter must be named `ns`.

### `internal.h`
**Analog:** binder decls at :295-301. Replace them with the 14 decls in RESEARCH §Binder Declarations, using this existing style:
```cpp
void bind_csv(sol::state& state, sol::usertype<Database>& bind, RunHandles& handles);
```
Shared helper decls sit after the binder decls, next to the existing ones (:305-311):
```cpp
Element table_to_element(const std::string& caller, const sol::object& values);
```
Add `metadata_to_lua` x2 and `parse_csv_options` there. Template helpers go next to `bulk_read_lua` (:89-97), each with a one-line role comment before `template <auto X>`, the same as the `list_metadata_lua`/`get_metadata_lua` text at `db_metadata.cpp:58-75`, moved verbatim. Comments must never spell `new_usertype<Database>` or `open_libraries(`.

### `lua_runner.cpp`
Replace only the binder calls at :113-119 with the 14-call block in RESEARCH §Binder Declarations. Keep the `lua_internal::bind_X(...)` style. `auto binary_file_type = lua_internal::bind_binary(...)` feeds `bind_expression`.

### `src/CMakeLists.txt` (:17-28)
These are plain `lua_runner/<file>` lines inside `QUIVER_SOURCES`, 4-space indent, alphabetical, `.h` listed among them:
```
    lua_runner/binary.cpp
    lua_runner/csv.cpp
    lua_runner/db_core.cpp
    ...
    lua_runner/internal.h
```
The final list is in RESEARCH §CMake.

### `src/AGENTS.md` file map (:46-58)
The file map uses one line per file: name padded to the comment column, then `# bind_X: names; notable helpers`.
```
    db_metadata.cpp       # bind_metadata: get_*_metadata, list_* groups
    csv.cpp               # bind_csv: read_csv, read_csv_stream, write_csv, CsvWriter
```
The replacement block is in RESEARCH §Citation Inventory, along with the other 24+6 citation fixes.

## Shared Patterns
- **Sandbox:** every file-op lambda calls `resolve_sandboxed_path(self|db, "<op>", path)`. The 10 call sites move verbatim.
- **Error text:** moves are verbatim. Pattern 1/2/3 strings stay byte-identical.
- **Formatting:** `uvx --from clang-format==22.1.8 clang-format -i`.

## No Analog Found
None.

## Metadata
**Analog search scope:** `src/lua_runner/`, `src/CMakeLists.txt`, `src/AGENTS.md`
**Files scanned:** 6
