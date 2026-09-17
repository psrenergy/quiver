# Coding Conventions

**Analysis Date:** 2026-09-17

## Error Handling

All errors in the C++ library follow exactly 3 message patterns. Every error lives in the C++/C API layer; bindings surface these messages verbatim through their FFI boundary without crafting their own (exception: pre-FFI type-marshalling errors for nullable types in bindings — those cannot be diagnosed by the core because the value never reaches it, and they name the offending `collection.attribute`).

**Pattern 1 — Precondition failure:**
```
Cannot {operation}: {reason}
```
Example: `"Cannot create_element: element must have at least one scalar attribute"`
Used for: Type mismatches, validation failures, missing required arguments. The caller's operation name is threaded through so the message reports the public method they called (e.g., `Cannot update_element: ...`).

**Pattern 2 — Not found:**
```
{Entity} not found: {identifier}
```
Example: `"Scalar attribute not found: 'value' in collection 'Items'"`
Example: `"Element not found: 42 in collection 'Config'"`
Used for: Missing element by id, missing label, missing column/attribute.

**Pattern 3 — Operation failure:**
```
Failed to {operation}: {reason}
```
Example: `"Failed to open database: unable to open the database file"`
Used for: I/O errors, SQLite errors, library-level failures.

Exception: The binary/expression subsystem's metadata validation throws descriptive messages that predate this scheme (e.g., `"Number of labels must be positive, got 0"`); new code should follow the three patterns.

### C API Error Channel

One global error channel via `quiver_get_last_error()` (no per-handle errors). All C API entry points that execute C++ logic wrap calls in try-catch:
```cpp
quiver_error_t quiver_some_function(quiver_database_t* db) {
    QUIVER_REQUIRE(db);
    try {
        // operation...
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}
```

Trivial functions that cannot throw (e.g., `in_transaction()`, pointer-read getters) skip the wrapper.

## Cross-Layer Naming Transformations

All public C++ methods are exposed through every layer with consistent naming. The transformation rules are mechanical:

| Layer | Rule | Example |
|-------|------|---------|
| C++ | snake_case | `create_element` |
| C API | Prefix `quiver_database_` | `quiver_database_create_element` |
| Julia | Same name, `!` suffix for mutators | `create_element!` |
| Dart | snake_case → camelCase, named constructors | `Database.fromSchema()`, `createElement()` |
| Python | Same snake_case, `@staticmethod` for factories, `**kwargs` for create/update | `Database.from_schema()`, `create_element()` |
| JS | snake_case → camelCase (includes `exportCsv` not `exportCsv`) | `Database.fromSchema()`, `createElement()` |
| Lua | Exactly 1:1 with C++ (colon method syntax) | `db:create_element()` |

**Factory method pattern:** `from_schema`, `from_migrations` → named constructors in Dart/JS (`Database.fromSchema()`), `@staticmethod` in Python, plain functions in Julia/Lua.

**Label-addressed writes:** Every id-addressed write has a `_by_label` counterpart. Spelled out in every layer including ones with overloading capability (C++, Julia, Lua) — one name everywhere, no dispatch. The label form resolves and delegates to the id form; never re-implements it.

## Method Naming Convention

All public Database methods follow: `verb_[category_]type[_by_id]`

**Verbs:** create, read, update, upsert, delete, get, list, has, query, describe, export, import

**Category:** optional; groups (vector, set, time_series), attribute type (scalar), file types

**Type:** singular/plural matches return cardinality
- `read_scalar_integers` (returns vector of values)
- `read_scalar_integer_by_id` (returns optional single value)

**Suffix `_by_id`:** Only when both "all elements" and "single element" variants exist (all read operations and element count).

**Suffix `_by_label`:** Label-addressed write (create/update/delete/relation). Every `_by_label` delegates to its id counterpart.

Examples:
- `create_element(collection, element)` → `create_element_by_label`
- `update_element(collection, id, element)` → `update_element_by_label(collection, label, element)`
- `read_scalar_integers(collection, attribute)` (all elements)
- `read_scalar_integer_by_id(collection, attribute, id)` (single element)
- `list_vector_groups(collection)` (plural — returns vector of group names)
- `get_scalar_metadata(collection, attribute)` (singular metadata type)

## C++ Code Style

### Pimpl vs Value Types

**Pimpl:** Classes hiding private dependencies (sqlite3, lua headers, file I/O):
- `Database`, `LuaRunner`, `BinaryFile`

**Value types (Rule of Zero):** Everything else — direct members, compiler-generated copy/move/destructor:
- `Element`, `Row`, `Result`, `Migration`, `Migrations`, `GroupMetadata`, `ScalarMetadata`, `CSVOptions`, `Dimension`, `TimeProperties`, `Expression`, `BinaryMetadata`

`BinaryMetadata` is a deliberate exception: user-declares its destructor (defaulted out-of-line), which suppresses compiler-generated moves — moves fall back to copies.

### RAII and Ownership

- Delete copy, default move for resource types:
  ```cpp
  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;
  Database(Database&&) = default;
  Database& operator=(Database&&) = default;
  ```
- Ownership of pointers/resources is explicit and unambiguous.
- `TransactionGuard` is nest-aware RAII: a guard becomes a no-op if an explicit transaction is already active (checked via `sqlite3_get_autocommit()`).

### Philosophy

- **Human-Centric:** Codebase optimized for readability, not machine parsing.
- **Clean code over defensive code:** Assume callers obey contracts; avoid excessive null checks.
- **Simple solutions over complex abstractions:** Delete unused code, do not deprecate.
- **Thin bindings:** Logic resides in C++ layer; bindings are thin marshaling layers.

### Logging

Per-database logger instance via spdlog (debug/info/warn/error levels). Never use global `spdlog::` functions. Example:
```cpp
impl_->logger->debug("Opening database: {}", path);
```

## Formatting and Linting

### Tools
- **C++:** `scripts/format.bat` runs clang-format, linting via `scripts/tidy.bat` (run-clang-tidy)
- **Dart:** `dart format` (invoked by scripts/format.bat)
- **Python:** `ruff` format and lint (invoked by scripts/format.bat)
- **JS/Bun:** `biome` format (invoked by scripts/format.bat)
- **Julia:** `JuliaFormatter` (invoked by scripts/format.bat)

### C++ Style Configuration

**`.clang-format`:**
- Based on LLVM style, modified for project conventions
- Line ending: LF (UseCRLF=false)
- Indent: 4 spaces, no tabs
- Column limit: 120
- C++20 standard
- NamespaceIndentation: None (no indent in namespace blocks)
- BreakConstructorInitializers: BeforeColon
- AllowShortFunctionsOnASingleLine: Inline (inline functions only)

**`.clang-tidy`:**
- Static analysis configuration; main build runs `run-clang-tidy` over `build/compile_commands.json`
- Skips `src/binary` (binary subsystem has load-bearing optimizations documented in `src/CLAUDE.md`)

### Pre-Commit Hooks

**`.pre-commit-config.yaml`:**
- Trailing whitespace removal
- End-of-file fixer
- YAML/JSON validation
- Mixed line ending fix (enforce LF)
- File size limit (>1 MB files flagged)
- clang-format (C++ files under `include/`, `src/`, `tests/`)
- cppcheck (C++ static analysis)
- cmake-format (CMake files)

### Line Endings

**`.gitattributes` enforces LF:**
```
*.cpp text eol=lf
*.h text eol=lf
*.dart text eol=lf
*.jl text eol=lf
*.py text eol=lf
```

**Caution:** `.bat` files are CRLF — unix tools (sed, etc.) silently convert them to LF and can break them. Restore CRLF if touched.

## Binding-Specific Conventions

### Dart (lib/src/database.dart)

- **Marshaling idiom:** Every method allocates through `package:ffi` `Arena` and releases in `finally`.
- **Typed column dispatch:** Dispatch on first non-null cell, then convert **every** cell individually. Never cast the rest as a type (defers the check to iteration).
- **Integer-for-REAL coercion:** An `int` (and a `bool`) is accepted into a REAL column.
- **Per-method boilerplate is the house style** — don't collapse into parameterized helpers.

### Python (src/quiverdb/)

- **CFFI ABI-mode:** No compiler required at install; `_c_api.py` declarations must match C headers exactly.
- **API shape:** `create_element`/`update_element` accept `**kwargs` (no `Element` class exposed). `LogLevel` is an `IntEnum`.
- **Positional-only parameters:** Methods like `update_element_by_label(collection, label, /, **kwargs)` use `/` to prevent a kwarg `label=` collision.
- **Per-method boilerplate is the house style** — don't collapse into parameterized helpers.
- **Null strings:** Pass `ffi.NULL` for a nullable string argument, never `b""` (clear vs lookup semantics).

### Julia (src/database_*.jl)

- **`GC.@preserve`:** All refs from `marshal_params` and any `Ref`s passed as pointers must stay inside a `GC.@preserve refs ...` block spanning the ccall.
- **Nullability-aware return types:** `read_scalar_{integers,floats,strings}` return concrete `Vector{T}` for NOT NULL columns and `Vector{Optional{T}}` for nullable ones (schema-driven, not data-driven).
- **`INTEGER PRIMARY KEY` (e.g., `id`):** Reported `not_null` by the C++ core, so `read_scalar_integers(db, c, "id")` returns a concrete `Vector{Int64}`.
- **One marshaller per group writer:** `_update_group_columns(db, update, ...)` shared by all four group writers and their `_by_label` forms.
- **Callback-first overloads for `do` syntax:** `from_schema(path) do db ... end` and similar — returns callback result, calls `close!` from `finally`.
- **Null strings:** Pass `Ptr{Cchar}(C_NULL)` for a nullable string argument, never `""` (clear vs lookup semantics).

### JS (bindings/js/src/index.ts)

- **String-based datetime surface:** No DateTime wrapper classes — all datetimes are ISO 8601 strings.
- **Query API shape:** `queryString`, `queryInteger`, `queryBoolean`, `queryFloat`, `queryDateTime` take an optional positional `List<Object?>? parameters` (no separate `*Params` methods).
- **Bun FFI loader:** Hand-written symbol table in `bindings/js/src/loader.ts` (no ffigen). Workarounds documented in `bindings/js/CLAUDE.md`.

### Lua (via src/lua_runner.cpp)

- **Filesystem sandbox:** File-touching operations (`db:open_file`, `db:export_csv`, etc.) are sandboxed to the database file's directory. Paths outside are rejected; `:memory:` databases reject all file operations.
- **Standard libraries enabled:** `base`, `string`, `table`, `math`, `coroutine`, `utf8` (pure computation only). `os`, `io`, `package`, `debug` are unloaded.
- **Disabled functions:** `dofile` and `loadfile` are nil'd (no loading Lua source from disk). String-form `load` stays available.
- **Lua boolean:** `true`/`false` are INTEGER 1/0 on every write path. No boolean **readers** (root design decision).
- **Return value encoding:** Script return is JSON-encoded via `to_lua_table` overloads (no binding gets a JSON dependency).
- **JSON null handling:** Non-finite numbers → `null`; table with `nil` holes → object (not array).

## Schema Conventions

### Configuration Table (Required)
Every schema must have:
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

### Vector Tables
Named `{Collection}_vector_{name}`:
```sql
CREATE TABLE Items_vector_values (
    id INTEGER NOT NULL REFERENCES Items(id) ON DELETE CASCADE ON UPDATE CASCADE,
    vector_index INTEGER NOT NULL,
    value REAL NOT NULL,
    PRIMARY KEY (id, vector_index)
) STRICT;
```

### Set Tables
Named `{Collection}_set_{name}`:
```sql
CREATE TABLE Items_set_tags (
    id INTEGER NOT NULL REFERENCES Items(id) ON DELETE CASCADE ON UPDATE CASCADE,
    value TEXT NOT NULL,
    UNIQUE (id, value)
) STRICT;
```

### Time Series Tables
Named `{Collection}_time_series_{name}` with dimension column (ordering, ISO 8601 TEXT):
```sql
CREATE TABLE Items_time_series_data (
    id INTEGER NOT NULL REFERENCES Items(id) ON DELETE CASCADE ON UPDATE CASCADE,
    date_time TEXT NOT NULL,
    value REAL NOT NULL,
    PRIMARY KEY (id, date_time)
) STRICT;
```

### Foreign Keys
Always `ON DELETE CASCADE ON UPDATE CASCADE` for parent references.

## Scalar Typing Policy

One unified policy shared by `value_matches_type` (`database_internal.h`, time-series writes) and `TypeValidator::validate_value` (`type_validator.cpp`, scalar create/update):

- int64 accepted for INTEGER **and** REAL columns (int-for-REAL coercion)
- double accepted for REAL **only** (float into INTEGER rejected)
- string accepted for TEXT / INTEGER (FK label) / DATE_TIME columns
- **Keep these two implementations in sync** — they are the only two type gates.

## DATE_TIME Content Validation

DATE_TIME string validated on write and stored verbatim. Accepted grammar:
```
YYYY-MM-DD optionally followed by THH:MM:SS or  HH:MM:SS
- All fields fixed-width and zero-padded
- Year: 0001–9999
- Calendar day must exist (no Feb 31)
- No leap seconds
```

Any other form (`2005`, `2024-01`, `2024-02-31`, `2024-1-5`, trailing garbage) is a Pattern 1 rejection naming the column.

One predicate `datetime::is_valid_iso8601` (`src/utils/datetime.h`) enforces this in all three layers:
1. `TypeValidator::validate_value` (scalar create/update, inherited by array writes)
2. `validate_time_series_row` (time-series writes)
3. `parse_datetime_import` (CSV import — custom-date-format branch parses with caller's format, then validates result)

Keep the three parsers (Julia `string_to_date_time`, Python `_parse_datetime`, Dart `stringToDateTime`) accepting exactly the same set — that intersection is what the core write gate guarantees.

---

*Convention analysis: 2026-09-17*
