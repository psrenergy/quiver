# Phase 1: Sidecar Reader and Attribute Meaning - Pattern Map

**Mapped:** 2026-09-20
**Files analyzed:** 8 (2 new source, 1 new header/impl of same pair, 3 modified source, 1 new test, 2 build files)
**Analogs found:** 8 / 8

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|---|---|---|---|---|
| `src/ui_config.h` | utility (internal header, no public counterpart) | file-I/O -> in-memory config | `src/csv_read.h` | exact |
| `src/ui_config.cpp` | utility (TOML parser) | file-I/O -> transform | `src/binary/binary_metadata.cpp` (`from_toml_content`) — pattern to copy; its bare-`.value()` throwing style is the anti-pattern to avoid | role-match (structure), anti-match (error handling) |
| `src/database_impl.h` (modify) | model (struct member) | in-memory state | `Database::Impl` itself, `schema`/`type_validator` members (`:59-71`) | exact |
| `src/database.cpp` (modify `from_migrations`) | controller/service glue | event-driven (post-migration hook) + warn-and-degrade | `src/database.cpp:69-86` (file-sink fallback try/catch) for error posture; `from_migrations` body itself (`:240-258`) for injection site | exact |
| `src/database_describe.cpp` (modify `write_collection_section` + 2 callers) | service (report renderer) | transform (schema+ui -> text) | itself, existing scalar loop `:62-72` and callers `:104`/`:114` | exact (self-modification) |
| `src/CMakeLists.txt` (modify) | config | batch (build source list) | existing `QUIVER_SOURCES` entries, e.g. `csv_read.cpp` line | exact |
| `tests/test_database_ui_metadata.cpp` | test | request-response (public API calls) | `tests/test_binary_metadata.cpp` (public-API-only toml precedent) + `tests/test_migrations.cpp` (temp-dir fixture idiom) | exact (combination of two analogs) |
| `tests/CMakeLists.txt` (modify) | config | batch (test source list) | existing entries, e.g. `test_binary_metadata.cpp` line | exact |

## Pattern Assignments

### `src/ui_config.h` (utility, no public counterpart)

**Analog:** `src/csv_read.h`

**Header-comment convention** (lines 1-9):
```cpp
#ifndef QUIVER_SRC_CSV_READ_H
#define QUIVER_SRC_CSV_READ_H

// Internal CSV reader wrapping vincentlaucsb/csv-parser for the Lua-only db:read_csv /
// db:read_csv_stream bindings (src/lua_runner.cpp). No public include/quiver/ counterpart, no
// QUIVER_API, no C API, no FFI binding: Julia/Dart/Python/JS already have native CSV libraries,
// and Lua needs this specifically because `io` is deliberately absent from its sandbox (root
// CLAUDE.md design decisions). ...
```
Copy this shape exactly for `src/ui_config.h`'s own header comment: state which layers deliberately
have no counterpart (no FFI consumer; toml++ is PRIVATE-linked on `quiver` per `src/CMakeLists.txt`)
and why. Header must expose plain `std` types only (`std::map`/`std::string`/`std::optional`) — no
`toml::` symbol may appear here; that stays confined to the `.cpp`.

**Type constraint (from CONTEXT.md, non-negotiable):** the enum map is
`std::map<int64_t, std::string>` named `enum_labels` — not a new type, not `details`.

---

### `src/ui_config.cpp` (utility, TOML parser)

**Analog (pattern to copy):** `src/binary/binary_metadata.cpp::from_toml_content` (lines 229-282)

**Parse + optional-checked array read** (lines 232-247):
```cpp
toml::table tbl = toml::parse(content);

std::vector<std::string> dimensions;
if (auto* arr = tbl["dimensions"].as_array()) {
    for (auto& elem : *arr) {
        if (auto val = elem.value<std::string>()) {
            dimensions.push_back(*val);
        }
    }
}
```
Use this exact shape (`as_array()` null-checked, `value<T>()` optional-checked) for every field:
`ui/<collection>.toml`'s top-level `id`, its `attribute` array, each attribute's
`id`/`label`/`tooltip`/`enum`, and `enum.toml`'s per-vocabulary `id`/`label` entries.

**ANTI-PATTERN — do not copy this part of the same file** (line 270):
```cpp
std::string initial_datetime_str = tbl["initial_datetime"].value<std::string>().value();
// bare .value() on the optional throws std::bad_optional_access on a missing key, uncaught.
```
D-09 forbids this throwing posture anywhere in `ui_config.cpp`. Every read must degrade to
`std::nullopt`/skip, never throw past the outer catch.

**Outer warn-and-degrade posture to copy instead** — analog `src/database.cpp:69-86`:
```cpp
std::shared_ptr<spdlog::sinks::basic_file_sink_mt> file_sink = nullptr;
try {
    const auto log_file_path = (db_dir / "quiver_database.log").string();
    file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file_path, true);
    file_sink->set_level(spdlog::level::debug);
    std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
    auto logger = std::make_shared<spdlog::logger>(logger_name, sinks.begin(), sinks.end());
    logger->set_level(spdlog::level::debug);
    return logger;
} catch (const spdlog::spdlog_ex& ex) {
    const auto logger = std::make_shared<spdlog::logger>(logger_name, console_sink);
    logger->set_level(spdlog::level::debug);
    logger->warn("Failed to create file sink: {}. Logging to console only.", ex.what());
    return logger;
}
```
Apply identically in the `ui_config.cpp` loader (or in `database.cpp` around the call, per D-09):
outer `try { ... } catch (const std::exception& ex) { logger->warn(...); return {}; /* empty UiConfig */ }`.
The inner per-file catch (D-09's second half, one malformed `ui/*.toml` costs only that
collection) has no direct analog in the repo — it is new code: wrap each file's parse in its own
try/catch inside the same loop, same warn-and-continue shape.

**Path resolution analog:** `src/lua_runner.cpp` `resolve_sandboxed_path` already uses
`fs::weakly_canonical` — reuse that idiom (not the exact function, which also sandbox-checks) for
D-16's `fs::weakly_canonical(migrations_path).parent_path() / "ui"`.

**No existing in-repo precedent for the locale-value (string-or-table.en) accessor or for
`enum.toml`'s dynamically-keyed top-level iteration** — RESEARCH.md's Pattern 3 and Pattern 4 are
new code, not adapted code; write them fresh using the same optional-checked toml++ style shown
above (`is_table()`/`as_table()`/`value<T>()`, never bare `.value()`).

---

### `src/database_impl.h` (modify — add one member)

**Analog:** existing members on `Database::Impl` (lines 59-71)
```cpp
struct Database::Impl {
    sqlite3* db = nullptr;
    std::string path;
    std::shared_ptr<spdlog::logger> logger;
    // Loaded lazily by require_schema: the Database(path, options) constructor opens an existing
    // database without reading its schema, and every metadata/CRUD path goes through
    // require_schema. mutable so the const readers (get_*_metadata, describe, ...) can trigger it.
    mutable std::unique_ptr<Schema> schema;
    mutable std::unique_ptr<TypeValidator> type_validator;
    bool dry_run = false;
```
Add the new member in the same style but **non-mutable** (per D-11 — populated eagerly in
`from_migrations`, not lazily): a plain value member, e.g. `UiConfig ui_config;` (default-constructed
= empty = "no ui/" state), with a one-line comment explaining why it is not `mutable` (unlike
`schema`/`type_validator` above it) and why it must not be hooked onto `require_schema()`/
`load_schema_metadata()`.

---

### `src/database.cpp` (modify `from_migrations`)

**Analog:** `from_migrations` itself (lines 240-258) — injection site is right after `db.migrate_up(migrations_path);` and before `return db;`:
```cpp
Database Database::from_migrations(const std::string& db_path,
                                   const std::string& migrations_path,
                                   const DatabaseOptions& options) {
    namespace fs = std::filesystem;
    if (options.read_only) {
        throw std::runtime_error("Cannot from_migrations: read_only mode (use Database constructor to open existing)");
    }
    if (!fs::exists(migrations_path)) {
        throw std::runtime_error("Migrations path not found: " + migrations_path);
    }
    if (!fs::is_directory(migrations_path)) {
        throw std::runtime_error("Cannot from_migrations: path is not a directory: " + migrations_path);
    }
    auto db = Database(db_path, options);
    db.migrate_up(migrations_path);
    return db;
}
```
Insert the UI load call between `db.migrate_up(migrations_path);` and `return db;`, e.g.
`db.impl_->ui_config = load_ui_config(migrations_path, db.impl_->logger);` — the loader itself does
the try/catch/warn/degrade (D-09), so this call site stays a single unconditional line, matching
the "still one call site, no refactor" framing in CONTEXT.md.

**Do NOT hook into `load_schema_metadata`** — the two early returns that make this wrong:
```cpp
// src/database.cpp:398-401 and :406-409 (verified in RESEARCH.md, not re-quoted here — both
// return before impl_->load_schema_metadata() is reached)
```

---

### `src/database_describe.cpp` (modify `write_collection_section` + both callers)

**Analog:** the file's own current code (lines 56-116)

**Signature + scalar loop, exact append point** (lines 56, 62-72):
```cpp
void write_collection_section(std::ostream& out, const Schema& schema, const std::string& collection, int64_t count) {
    ...
    for (const auto& name : table_def->column_order) {
        const auto& col = table_def->columns.at(name);
        out << "    - " << name << " (" << data_type_to_string(col.type) << ")";
        if (col.primary_key) {
            out << " PRIMARY KEY";
        }
        if (col.not_null && !col.primary_key) {
            out << " NOT NULL";
        }
        out << "\n";
    }
```
Per CONTEXT.md D-08, change the signature to:
```cpp
void write_collection_section(std::ostream& out, const Schema& schema,
                              const std::string& collection, int64_t count,
                              const UiConfig* ui, bool with_tooltip);
```
Replace the bare `out << "\n";` on the loop's last line with clause-emission logic (D-01/D-04/
D-05/D-06/D-07) that builds up to three `"; keyword body"` strings and appends them before the same
final `"\n"`. Each clause self-contained with its own leading `"; "` — no shared separator logic.

**Callers, exact current form** (lines 95-116):
```cpp
std::string Database::describe() const {
    impl_->require_schema();
    std::ostringstream out;
    out << "Database: " << impl_->path << "\n";
    out << "Version: " << current_version() << "\n";
    for (const auto& collection : impl_->schema->collection_names()) {
        out << "\n";
        write_collection_section(out, *impl_->schema, collection, number_of_elements(collection));
    }
    return out.str();
}

std::string Database::describe_collection(const std::string& collection) const {
    impl_->require_collection(collection, "describe_collection");
    std::ostringstream out;
    write_collection_section(out, *impl_->schema, collection, number_of_elements(collection));
    return out.str();
}
```
Both calls gain two trailing args: `describe()` passes `&impl_->ui_config, false`;
`describe_collection()` passes `&impl_->ui_config, true`. Do **not** touch `summarize_collection`
(`:118-183` — Phase 2 scope, explicitly excluded).

---

## Shared Patterns

### Warn-and-degrade error posture (D-09)
**Source:** `src/database.cpp:69-86`
**Apply to:** `src/ui_config.cpp`'s outer load function and its per-file inner loop.
Never copy `src/binary/binary_metadata.cpp:270`'s bare `.value()` throwing style — that file is the
documented anti-pattern for this phase.

### Optional-checked toml++ reads
**Source:** `src/binary/binary_metadata.cpp:232-247`
**Apply to:** every value read in `ui_config.cpp` — `as_array()` null-checked, `value<T>()`
optional-checked, never bare `.value()`.

### Internal `src/`-local component with no public header
**Source:** `src/csv_read.h:1-9`
**Apply to:** `src/ui_config.h`/`.cpp` pair — same "no `include/quiver/` counterpart, no
`QUIVER_API`, no C API, no binding" convention and header-comment shape.

### Build registration
**Source:** `src/CMakeLists.txt` `QUIVER_SOURCES` list (e.g. the `csv_read.cpp` entry) and
`tests/CMakeLists.txt`'s `quiver_tests` source list (e.g. the `test_binary_metadata.cpp` entry).
**Apply to:** add `ui_config.cpp` to `QUIVER_SOURCES` (mandatory — tomlplusplus is PRIVATE on
`quiver`, per D-10) and `test_database_ui_metadata.cpp` to `tests/CMakeLists.txt`.

### Temp-dir fixture idiom
**Source:** `tests/test_migrations.cpp:12-32` (`MigrationsTestFixture::SetUp`/`TearDown`, temp dir
under `fs::temp_directory_path()`, cleaned before and after) and `:194-199` (a minimal
migrations-tree-building test using `fs::create_directories` + `std::ofstream`).
**Apply to:** `tests/test_database_ui_metadata.cpp`'s fixture, extended to also write a sibling
`ui/*.toml` tree (no files committed under `tests/schemas/ui/` per D-12 — temp dirs only).

### Public-API-only test style
**Source:** `tests/test_binary_metadata.cpp:1-40` — includes only public headers
(`quiver/binary/binary_metadata.h`, `quiver/element.h`), builds TOML content as a raw string
literal (`make_valid_toml()`), asserts on the resulting object — zero `toml::` includes.
**Apply to:** `tests/test_database_ui_metadata.cpp` — drive everything through
`Database::from_migrations` + `describe()`/`describe_collection()`, never include `<toml++/...>` or
call into `ui_config.h` directly.

## No Analog Found

| File | Role | Data Flow | Reason |
|---|---|---|---|
| Locale-value accessor (`label`/`tooltip`/enum-entry `label`: string-or-table.en) | utility function inside `ui_config.cpp` | transform | RESEARCH.md confirms zero in-repo precedent for this exact string-or-table branch; `binary_metadata.cpp` only reads flat scalars/arrays. Write fresh using the same optional-checked toml++ primitives shown above. |
| `enum.toml` dynamic-top-level-key iteration | utility function inside `ui_config.cpp` | transform | No fixed-key array to look up (`toml::table` must be iterated as `(key, node&)` pairs); new code, confirmed against 4 corpus repos in RESEARCH.md Pattern 4. |

## Metadata

**Analog search scope:** `src/`, `src/binary/`, `tests/`, root `CMakeLists.txt`/`src/CMakeLists.txt`/`tests/CMakeLists.txt`
**Files scanned:** `src/csv_read.h`, `src/binary/binary_metadata.cpp`, `src/database_describe.cpp`, `src/database_impl.h`, `src/database.cpp`, `tests/test_migrations.cpp`, `tests/test_binary_metadata.cpp`, `tests/CMakeLists.txt`, `src/CMakeLists.txt`
**Pattern extraction date:** 2026-09-20
