# Coding Conventions

**Analysis Date:** 2026-04-18

## Naming Patterns

### Files

- C++ implementation files split by verb category: `src/database_create.cpp`, `src/database_read.cpp`, `src/database_update.cpp`, `src/database_delete.cpp`, `src/database_metadata.cpp`, `src/database_query.cpp`, `src/database_time_series.cpp`, `src/database_csv_export.cpp`, `src/database_csv_import.cpp`, `src/database_describe.cpp`.
- C API files mirror C++ counterparts under `src/c/`: `src/c/database.cpp`, `src/c/database_create.cpp`, `src/c/database_read.cpp`, etc. Binary subsystem uses `src/c/binary/*`.
- Private implementation helpers use `_impl.h` / `_internal.h` suffixes: `src/database_impl.h`, `src/database_internal.h`, `src/c/internal.h`, `src/c/database_helpers.h`, `src/c/database_options.h`.
- Public headers live under `include/quiver/` (C++) and `include/quiver/c/` (C API), grouped by subsystem (e.g. `include/quiver/binary/`, `include/quiver/c/binary/`).
- Tests mirror source files with `test_` prefix: `tests/test_database_read.cpp`, `tests/test_c_api_database_read.cpp`, `tests/test_c_api_binary_file.cpp`.
- Schemas: `tests/schemas/valid/*.sql`, `tests/schemas/invalid/*.sql`, `tests/schemas/migrations/{version}/{up,down}.sql`.

### C++ Identifiers (enforced by `.clang-tidy`)

- **Classes/structs/enums:** `CamelCase` (`Database`, `BinaryFile`, `ScalarMetadata`, `LogLevel`).
- **Enum constants:** `CamelCase` (`LogLevel::Debug`, `TimeFrequency::Monthly`).
- **Functions/methods/variables/members/parameters:** `lower_case` with underscores.
- **Private members:** trailing underscore (`impl_`, `last_error_`). Public data members (plain value types) have no suffix.
- **Namespaces:** `lower_case` (`quiver`, `quiver::string`, `quiver::test`).
- **Constants / `constexpr` / globals:** `lower_case`.

### Method Naming: `verb_[category_]type[_by_id]`

Reference: `include/quiver/database.h`.

- **Verbs:** `create`, `read`, `update`, `delete`, `get`, `list`, `has`, `query`, `describe`, `export`, `import`, `begin_transaction`, `commit`, `rollback`, `in_transaction`.
- **Single-element reads end with `_by_id`** (e.g., `read_scalar_integer_by_id`). Plural form returns all elements (`read_scalar_integers`).
- **Typed read variants** use `_integers` / `_floats` / `_strings` suffixes.
- **Group categories** are `scalar`, `vector`, `set`, `time_series`.
- Examples: `create_element`, `read_vector_floats`, `read_vector_floats_by_id`, `update_time_series_group`, `list_set_groups`, `get_scalar_metadata`.

### C API Identifiers

- All C API symbols prefixed with `quiver_`: `quiver_database_create_element`, `quiver_binary_file_open_read`, `quiver_database_options_default`.
- Types: `quiver_{entity}_t` (`quiver_database_t`, `quiver_element_t`, `quiver_binary_file_t`, `quiver_scalar_metadata_t`, `quiver_group_metadata_t`, `quiver_error_t`, `quiver_data_type_t`).
- Enum constants use `UPPER_SNAKE_CASE` with shared prefix: `QUIVER_OK`, `QUIVER_ERROR`, `QUIVER_LOG_DEBUG`, `QUIVER_DATA_TYPE_INTEGER`.
- Output parameters use `out_` prefix (`out_db`, `out_values`, `out_count`, `out_healthy`).
- Free functions mirror allocator: `quiver_database_free_integer_array`, `quiver_database_free_string_array`, `quiver_database_free_scalar_metadata`, `quiver_binary_metadata_free_dimension`.

### Cross-Layer Name Transformation

Mechanical transformation rules allow derivation from any C++ method without lookup tables (see `CLAUDE.md` lines 362-409 for the full cross-layer table):

| From C++ | To C API | To Julia | To Dart | To Python | To Lua | To JS |
|----------|----------|----------|---------|-----------|--------|-------|
| `method_name` | `quiver_database_method_name` | `method_name` (+`!` if mutating) | `methodName` | `method_name` | `method_name` | `methodName` |
| `Database::from_schema()` | `quiver_database_from_schema()` | `from_schema()` | `Database.fromSchema()` | `Database.from_schema()` | N/A | `Database.fromSchema()` |
| `create_element` | `quiver_database_create_element` | `create_element!` | `createElement` | `create_element` (`**kwargs`) | `create_element` | `createElement` |
| `read_scalar_integers` | `quiver_database_read_scalar_integers` | `read_scalar_integers` | `readScalarIntegers` | `read_scalar_integers` | `read_scalar_integers` | `readScalarIntegers` |

Notes:
- Julia adds `!` only for mutating operations (`create_element!`, `update_element!`, `delete_element!`, `close!`, `begin_transaction!`, `commit!`, `rollback!`).
- Python `create_element` / `update_element` accept `**kwargs` in place of the Element builder: `db.create_element("Collection", label="x", value=42)`.
- Dart factory methods become named constructors: `Database.fromSchema()`, `Database.fromMigrations()`.
- Lua has no lifecycle methods; database is provided as `db` userdata by `LuaRunner`.

### Binding-Only Convenience Methods

These compose core operations and have no C++/C API counterpart:
- Transaction block wrappers: `transaction(db) do db...end` (Julia), `db.transaction((db) {...})` (Dart), `db:transaction(function(db)...end)` (Lua).
- Composite readers: `read_scalars_by_id`, `read_vectors_by_id`, `read_sets_by_id` (Julia, Dart, Lua, JS).
- DateTime wrappers: `read_scalar_date_time_by_id`, `query_date_time` (Julia, Dart).
- Multi-column group readers: `read_vector_group_by_id`, `read_set_group_by_id` (Julia, Dart).

## Code Style

### Formatting

- Tool: `clang-format-21` (configured in `.clang-format`).
- Based on `LLVM` style with `C++20` standard.
- **Line length:** 120 columns.
- **Indent:** 4 spaces, no tabs.
- **Line ending:** LF (`UseCRLF: false`).
- `BreakBeforeBraces: Attach` (opening brace on same line).
- `PointerAlignment: Left` (`int* x`, not `int *x`).
- `NamespaceIndentation: None`, `FixNamespaceComments: true`.
- `IncludeBlocks: Regroup`, `SortIncludes: true`.
- `BinPackArguments: false`, `BinPackParameters: false` (one-per-line when wrapping).
- `AllowShortFunctionsOnASingleLine: Inline` only.
- Python: `ruff` with `line-length = 120`, `indent-width = 4`, `quote-style = "double"`, `target-version = "py313"`.
- Dart: `analysis_options.yaml` with `page_width: 120`, `trailing_commas: preserve`, `lints/recommended.yaml`.
- CI enforces formatting via `find include src tests -name '*.cpp' -o -name '*.h' | xargs clang-format-21 --dry-run --Werror` in `.github/workflows/ci.yml` (`clang-format` job).

### Linting

- Tool: `clang-tidy` (configured in `.clang-tidy`).
- Enabled checks: `bugprone-*`, `modernize-*`, `performance-*`, `readability-identifier-naming`, `readability-redundant-*`, `readability-simplify-*`.
- Disabled checks: `bugprone-easily-swappable-parameters`, `modernize-use-trailing-return-type`, `modernize-avoid-c-arrays`, `bugprone-narrowing-conversions`, `modernize-use-nodiscard`, `modernize-use-ranges`, `modernize-use-designated-initializers`, `performance-inefficient-string-concatenation`, `performance-enum-size`.
- `HeaderFilterRegex: '(include/quiver/|src/)'` restricts tidy to first-party headers.
- Run via `scripts/tidy.bat` or CMake target `tidy`.
- `WarningsAsErrors: ''` (warnings do not fail build); CI does not run clang-tidy.
- Python: `ruff` configured with only isort checks (`select = ["I"]`) in `bindings/python/ruff.toml`.

## Import / Include Organization

C++ translation units follow this order (`clang-format`'s `IncludeBlocks: Regroup` regroups automatically):
1. Paired `_impl.h` or `internal.h` (for `.cpp` files).
2. Project public headers (`"quiver/..."`, `"database_helpers.h"`).
3. Third-party headers (spdlog, sqlite3, gtest).
4. Standard library headers (`<string>`, `<vector>`, `<filesystem>`).

Example from `src/database.cpp`:
```cpp
#include "database_impl.h"
#include "quiver/migrations.h"
#include "quiver/result.h"
#include "utils/string.h"

#include <atomic>
#include <filesystem>
#include <spdlog/sinks/basic_file_sink.h>
#include <sqlite3.h>
#include <stdexcept>
```

- Julia: `using` then `include("fixture.jl")` at top of module (`bindings/julia/src/Quiver.jl`).
- Dart: `package:` imports first, then `dart:io`, then local `../` imports (`bindings/dart/test/database_lifecycle_test.dart`).
- Python: `from __future__ import annotations` at top, then stdlib, then project imports (`bindings/python/src/quiverdb/database.py`).
- TypeScript/Deno: `jsr:@std/*` imports then local `../src/*.ts` (`bindings/js/test/database.test.ts`).

## Error Handling

### C++: Three Canonical `std::runtime_error` Patterns

All `throw std::runtime_error(...)` in the C++ layer use exactly these three message shapes. Downstream layers (C API, bindings) rely on this consistency.

**Pattern 1 — Precondition failure:** `"Cannot {operation}: {reason}"`
```cpp
// src/database_create.cpp:11
throw std::runtime_error("Cannot create_element: element must have at least one scalar attribute");
// src/database_impl.h:34
throw std::runtime_error(std::string("Cannot ") + operation + ": no schema loaded");
// src/database_csv_import.cpp:38
throw std::runtime_error("Cannot import_csv: could not open file: " + path);
```

**Pattern 2 — Not found:** `"{Entity} not found: {identifier}"`
```cpp
// src/database_impl.h:41
throw std::runtime_error(std::string("Cannot ") + operation + ": collection not found: " + collection);
// src/database_impl.h:49-53
throw std::runtime_error(std::string("Cannot ") + operation + ": table not found: " + table);
```

**Pattern 3 — Operation failure:** `"Failed to {operation}: {reason}"`
```cpp
// src/database.cpp:112
throw std::runtime_error("Failed to open database: " + error_msg);
// src/database_impl.h:71-72
throw std::runtime_error("Failed to resolve label '" + str_val + "' to ID in table '" + fk.to_table + "'");
```

No ad-hoc message formats. Use the `impl_->require_schema()`, `impl_->require_collection()`, `impl_->require_column()` helpers in `src/database_impl.h` rather than crafting new error text.

### C API: Binary Return Codes + Thread-Local Error Message

Return `quiver_error_t` (`QUIVER_OK = 0`, `QUIVER_ERROR = 1`). Error details retrieved via `quiver_get_last_error()` (thread-local storage in `src/c/common.cpp`).

Canonical try/catch body (see `src/c/database_read.cpp`, `src/c/database_transaction.cpp`):
```cpp
QUIVER_C_API quiver_error_t quiver_database_some_op(quiver_database_t* db, ...) {
    QUIVER_REQUIRE(db, ...);            // null-check output parameters
    try {
        // delegate to db->db.<method>(...)
        return QUIVER_OK;
    } catch (const std::bad_alloc&) {
        quiver_set_last_error("Memory allocation failed");
        return QUIVER_ERROR;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}
```

- Exceptions to the `quiver_error_t` rule (direct return): `quiver_get_last_error`, `quiver_version`, `quiver_clear_last_error`, `quiver_database_options_default`, `quiver_csv_options_default`.
- `QUIVER_REQUIRE(...)` macro in `src/c/internal.h` (lines 39-127) validates up to 9 pointer arguments and emits `"Null argument: <name>"` via stringification.

### Bindings: Never Craft Errors

All error messages originate in the C++/C API layer. Bindings retrieve them and re-raise:

- Python `check()` (`bindings/python/src/quiverdb/_helpers.py:7-17`) — calls `quiver_get_last_error()` and raises `QuiverError(detail)`.
- Julia `check(...)` in `bindings/julia/src/` raises `DatabaseException` with the C API message.
- Dart throws `DatabaseException` wrapping `quiver_get_last_error()` (`bindings/dart/lib/src/exceptions.dart`).
- JS throws `QuiverError` (`bindings/js/src/errors.ts`).

## Logging

- Framework: `spdlog` (linked privately to `quiver`, see `src/CMakeLists.txt:69`).
- Per-database logger created in `src/database.cpp:43-88` via `create_database_logger()`:
  - Unique logger name (`quiver_database_N`) via atomic counter.
  - Console sink (`stderr_color_sink_mt`) respects `options.console_level`.
  - File sink (`basic_file_sink_mt`) writes to `{db_dir}/quiver_database.log` for file-based databases, always at `debug` level. In-memory databases skip file logging.
- Levels map directly from `quiver::LogLevel` (`Debug`, `Info`, `Warn`, `Error`, `Off`) via `to_spdlog_level()`.
- Usage: `impl_->logger->debug("Opening database: {}", path);`, `impl_->logger->info(...)`, `impl_->logger->warn(...)`, `impl_->logger->error(...)`.
- 41 call sites across `src/database.cpp`, `src/database_create.cpp`, `src/database_delete.cpp`, `src/database_time_series.cpp`, `src/database_update.cpp`, `src/database_impl.h`.
- No free `spdlog::info(...)` calls — always use the database's owned logger.

## Class / Type Patterns

### Pimpl vs Value Types

**Pimpl** is used only to hide private dependencies (SQLite, Lua, sol2, file I/O). Examples:
- `quiver::Database` (hides `sqlite3*`, spdlog, `Schema`, `TypeValidator`): `include/quiver/database.h:18-28`, `src/database_impl.h:25-30`.
- `quiver::LuaRunner` (hides sol2/lua headers).
- `quiver::BinaryFile`, `quiver::CSVConverter` (hide file I/O).

Pattern:
```cpp
// include/quiver/database.h
class QUIVER_API Database {
public:
    Database(Database&&) noexcept;
    Database& operator=(Database&&) noexcept;
    ~Database();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// src/database_impl.h
struct Database::Impl {
    sqlite3* db = nullptr;
    std::shared_ptr<spdlog::logger> logger;
    std::unique_ptr<Schema> schema;
    // ...
};
```

**Plain value types** (no private deps) follow Rule of Zero — direct members, compiler-generated copy/move/destructor:
- `Element`, `Row`, `Result`, `Migration`, `Migrations`, `ScalarMetadata`, `GroupMetadata`, `DatabaseOptions`, `CSVOptions`, `BinaryMetadata`, `Dimension`, `TimeProperties`.

### Move Semantics

Resource-owning classes delete copy, default move:
```cpp
// include/quiver/database.h:24-29
Database(const Database&) = delete;
Database& operator=(const Database&) = delete;
Database(Database&& other) noexcept;
Database& operator=(Database&& other) noexcept;
```
Definitions: `src/database.cpp:124-125`:
```cpp
Database::Database(Database&& other) noexcept = default;
Database& Database::operator=(Database&& other) noexcept = default;
```

### Factory Methods

Static factories for common construction:
```cpp
// include/quiver/database.h:31-36
static Database from_migrations(const std::string& db_path,
                                const std::string& migrations_path,
                                const DatabaseOptions& options = {});
static Database from_schema(const std::string& db_path,
                            const std::string& schema_path,
                            const DatabaseOptions& options = {});
```
Binary subsystem mirrors with `BinaryFile::open_file()`, `BinaryMetadata::from_toml()`, `BinaryMetadata::from_element()`.

### Internal RAII Guards

Nest-aware transaction guard in `src/database_impl.h` — no-op if an explicit transaction is already active (checked via `sqlite3_get_autocommit()`):
```cpp
{
    Impl::TransactionGuard txn(*impl_);
    // ... writes ...
    txn.commit();
}
```
Write methods (`create_element`, etc.) use this so they compose with explicit `begin_transaction()`/`commit()`/`rollback()` calls without double-beginning.

## C API Memory Management

- Uses `new`/`delete` (not `malloc`/`free`) — matches `std::string`/`std::vector` allocators on Windows runtimes.
- Every allocation has a matching `quiver_{entity}_free_*` in the **same translation unit** ("alloc/free co-location").
  - `src/c/database_read.cpp` — read allocators + `quiver_database_free_integer_array`, `quiver_database_free_float_array`, `quiver_database_free_string_array`, `quiver_database_free_string`, `quiver_database_free_integer_vectors`, etc.
  - `src/c/database_metadata.cpp` — metadata allocators + `quiver_database_free_scalar_metadata`, `quiver_database_free_group_metadata`, and `_array` variants.
  - `src/c/database_time_series.cpp` — time series allocators + `quiver_database_free_time_series_data`.
  - `src/c/binary/binary_file.cpp`, `src/c/binary/binary_metadata.cpp` — binary allocators + their `_free_*` counterparts.
- String allocation uses `quiver::string::new_c_str()` from `src/utils/string.h` (null-terminated, heap-allocated char array).
- Factory functions use out-parameters (`out_db`, `out_values`, `out_count`) and return `quiver_error_t`.
- Close functions are idempotent; passing `nullptr` returns `QUIVER_OK` (see `quiver_database_close`).
- Helper templates in `src/c/database_helpers.h` avoid duplicating marshaling logic: `read_scalars_impl<T>`, `read_vectors_impl<T>`, `free_vectors_impl<T>`, `copy_strings_to_c`, `convert_scalar_to_c`, `convert_group_to_c`, `to_c_data_type`.

## Schema Conventions

All test schemas live in `tests/schemas/valid/` and `tests/schemas/invalid/`. Invalid-schema fixtures exist for negative tests (duplicate attributes, missing FK actions, wrong label types, missing Configuration, etc.).

### Required Elements

**`Configuration` table required** in every schema:
```sql
CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;
```

### Collections

```sql
CREATE TABLE MyCollection (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL,
    -- scalar attributes
) STRICT;
```

### Group Tables — `{Collection}_{category}_{name}`

- **Vector:** `{Collection}_vector_{name}` with `(id, vector_index)` composite PK.
- **Set:** `{Collection}_set_{name}` with `UNIQUE(id, value)`.
- **Time series:** `{Collection}_time_series_{name}` with dimension column prefixed `date_` (e.g., `date_time`), composite PK `(id, date_*)`.
- **Time series files:** `{Collection}_time_series_files` — singleton table with `data_file TEXT`, `metadata_file TEXT`.

All tables use `STRICT` and `PRAGMA foreign_keys = ON;`. Foreign keys **always** use `ON DELETE CASCADE ON UPDATE CASCADE`. See `tests/schemas/valid/collections.sql` for the canonical example.

## Comments

- No `@file` / banner headers. Files begin with includes or the conditional guard.
- Section dividers use 76-char equal-sign lines (test files) when splitting related groups of tests:
  ```cpp
  // ============================================================================
  // Schema error tests
  // ============================================================================
  ```
- Inline comments explain non-obvious mechanics ("Pre-resolve pass: resolve all FK labels before any writes" in `src/database_create.cpp:14`).
- No JSDoc-style annotations in C++. Docstrings used in Python (triple-quoted) and Julia (`"""` above function).
- Avoid redundant comments; the code is expected to be self-documenting (philosophy line in `CLAUDE.md`: "Codebase optimized for human readability, not machine parsing").

## Function Design

- **Parameters:** `const std::string&` for strings, value types by value (`int64_t id`), options structs by const reference with default (`const DatabaseOptions& options = {}`).
- **Return values:** `std::vector<T>` for all-elements reads, `std::optional<T>` for `_by_id` reads (single-element nullable), `int64_t` for generated IDs, `void` for mutations.
- **Designated initializers** used for options structs (C++20): `quiver::Database db(":memory:", {.read_only = false, .console_level = quiver::LogLevel::Off});`
- **Trailing return types** avoided (disabled in `.clang-tidy`).

## Module Design

- Public C++ API exported via `QUIVER_API` macro (declared in `include/quiver/export.h`) and `QUIVER_C_API` in `include/quiver/c/common.h`.
- C API wraps C++ — logic resides in C++ layer; C API is a thin marshaling layer.
- Bindings remain thin — no business logic duplicated in Julia/Dart/Python/JS/Lua wrappers.
- Each binding package mirrors C++ file structure:
  - Julia: `bindings/julia/src/database_create.jl`, `database_read.jl`, etc.
  - Dart: `bindings/dart/lib/src/database_create.dart`, etc.
  - Python: `bindings/python/src/quiverdb/database.py` (all in one class via `Database(DatabaseCSVExport, DatabaseCSVImport)` mixin), plus `_helpers.py`, `_loader.py`, `_c_api.py`, `_declarations.py`.
  - JS: `bindings/js/src/create.ts`, `read.ts`, etc.

## Philosophy (from `CLAUDE.md`)

- **Human-Centric:** Codebase optimized for human readability.
- **Status:** WIP project; breaking changes acceptable; no backwards compatibility required.
- **Clean code over defensive code:** Assume callers obey contracts; avoid excessive null checks.
- **ABI stability:** Pimpl only when hiding private dependencies; plain value types use Rule of Zero.
- **C++20 target:** Use modern language features when they simplify logic (designated initializers, `std::variant`, structured bindings, `std::optional`, concepts).
- **Intelligence:** Logic in C++; bindings stay thin.
- **Error messages:** Defined once in C++/C API; bindings surface them.
- **Ownership:** RAII strictly; ownership of pointers/resources must be explicit and unambiguous.
- **Simple solutions over complex abstractions.** Delete unused code; do not deprecate.
- **Self-updating:** Keep `CLAUDE.md` up to date with codebase changes.

---

*Convention analysis: 2026-04-18*
