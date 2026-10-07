---
last_mapped_commit: cf9ca58aceab305fe6a08461f78da8556a6c95d0
last_mapped_at: 2026-10-07
---
<!-- refreshed: 2026-10-07 -->

# Architecture

**Analysis Date:** 2026-10-07

## System Overview

```text
Host applications
  Julia                 Dart                    Python                   Bun/TypeScript
  bindings/julia/src/    bindings/dart/lib/       bindings/python/src/     bindings/js/src/
       \                    |                       |                       /
        +-------------------+-----------------------+----------------------+
                                    |
                                    v
                 Opaque C ABI / libquiver_c
                 include/quiver/c/ + src/c/
                                    |
                                    v
                 C++20 core / libquiver                 CLI
                 include/quiver/ + src/ <----------- src/cli/main.cpp
                      |          |          |
                      |          |          +-- Direct sol2 Lua sandbox
                      |          |              src/sandbox/
                      |          +-- Binary files / lazy expressions
                      |              src/binary/ + src/expression/
                      +-- SQLite / schema / migrations
                          src/database.cpp + src/schema.cpp

Internal file services: src/csv/ + src/xlsx/ + src/ui_metadata.cpp
Outputs: SQLite databases, .qvr binary data + .toml metadata, CSV, JSON return strings
```

The core is a library rather than a server. `src/CMakeLists.txt` builds `quiver`, optionally builds the shared C wrapper `quiver_c`, and builds `quiver_cli`. Application calls execute synchronously in the caller's process; there is no HTTP request layer or application service container.

## Component Responsibilities

| Component | Responsibility | File |
|-----------|----------------|------|
| Database facade | Public lifecycle, factories, CRUD, metadata, transactions, query, CSV and migrations | `include/quiver/database.h` |
| Database implementation | SQLite handle, logger, schema cache, FK resolution, validated group writes, transaction guard | `src/database_impl.h` |
| Statement execution | Prepare, count parameters, bind variants, step rows, materialize results | `src/database.cpp` |
| Schema introspection | Table/column/index/FK metadata and group naming/classification | `src/schema.h`, `src/schema.cpp` |
| Schema/type validation | Enforce Quiver table conventions and accepted scalar/array values | `src/schema_validator.cpp`, `src/type_validator.cpp` |
| C ABI | Opaque handles, status codes, allocations, presence masks and host-neutral marshalling | `include/quiver/c/`, `src/c/` |
| Native errors | One thread-local last-error channel | `src/c/common.cpp` |
| Lua runtime | Persistent sol2 state, direct C++ method bindings, return encoding, run cleanup | `src/sandbox/sandbox.cpp`, `src/sandbox/internal.h` |
| Lua path policy | Resolve paths relative to database directory and enforce containment | `src/sandbox/path_policy.cpp` |
| Binary file | Owned binary stream, metadata, calendar coordinates and writer registry | `src/binary/binary_file.cpp`, `src/binary/binary_utils.h` |
| Expression engine | Lazy node composition, broadcasting, aggregation and save evaluation | `src/expression/expression.cpp`, `include/quiver/expression/expression_node.h` |
| CSV services | One shared parser and emitter behind database and Lua consumers | `src/csv/csv_read.cpp`, `src/csv/csv_write.cpp` |
| XLSX service | Internal OpenXLSX reader and strict XML/cell interpretation | `src/xlsx/xlsx_read.cpp` |
| UI sidecars | Optional collection annotations used by text descriptions | `src/ui_metadata.cpp`, `src/database_describe.cpp` |

## Pattern Overview

**Overall:** Layered native library with a C++ domain facade, an opaque C boundary, thin language adapters, and an embedded scripting adapter.

**Key Characteristics:**

- Keep database logic and user-facing native diagnostics in `src/database_*.cpp`; bindings marshal data and surface native errors (`AGENTS.md`).
- Use Pimpl where implementation dependencies must stay private: `Database`, `Sandbox` and `BinaryFile`; keep ordinary metadata/value types as plain structs or value classes (`include/quiver/database.h`, `include/quiver/sandbox.h`, `include/quiver/binary/binary_file.h`).
- Split one public facade by operation area instead of introducing service abstractions: lifecycle in `src/database.cpp`, create/read/update/delete and specialized operations in adjacent `src/database_*.cpp` files.
- Lua is inside the C++ core and binds C++ objects directly; it does not use the C ABI (`src/sandbox/sandbox.cpp`).
- Preserve intentional per-binding exceptions and data-shape asymmetries recorded in `AGENTS.md`.

## Layers

**Public C++ domain API:**

- Purpose: Own database behavior and binary/expression abstractions.
- Location: `include/quiver/`, `src/`, `src/binary/`, `src/expression/`.
- Contains: `Database`, `Element`, `Value`, `Row`, `Result`, metadata, migrations, binary streams and expressions.
- Depends on: SQLite, logging and private parsing/runtime dependencies declared in `src/CMakeLists.txt`.
- Used by: C ABI, direct C++ callers, Lua binders and CLI (`src/c/internal.h`, `src/sandbox/`, `src/cli/main.cpp`).

**C ABI adapter:**

- Purpose: Expose an ownership-explicit, host-neutral FFI surface.
- Location: `include/quiver/c/`, `src/c/`.
- Contains: C signatures, opaque `*_t` handles, output-pointer factories, enum/status translation, alloc/free pairs and row/column conversion.
- Depends on: C++ domain API; concrete handles own domain values in `src/c/internal.h`.
- Used by: Julia, Dart, Python and Bun loaders (`bindings/julia/src/c_api.jl`, `bindings/dart/lib/src/ffi/bindings.dart`, `bindings/python/src/quiverdb/_c_api.py`, `bindings/js/src/loader.ts`).

**Host-language adapters:**

- Julia: `bindings/julia/src/Quiver.jl` includes procedural wrappers and generated `C` module; mutation uses `!`; binary/expression wrappers share `AbstractExpression`.
- Dart: `bindings/dart/lib/src/database.dart` owns the handle and includes `part` files containing extensions; `bindings/dart/lib/src/ffi/library_loader.dart` supplies native bindings.
- Python: `bindings/python/src/quiverdb/database.py` owns the handle and includes CSV behavior through two mixins; CFFI ABI declarations are hand-maintained in `_c_api.py`; public element writes accept keyword arguments.
- Bun: `bindings/js/src/index.ts` imports operation modules that attach implementations to `Database.prototype`; `bindings/js/src/database.ts` owns lifecycle and typed declarations; `loader.ts` contains the hand-written FFI table.
- Keep adapters thin; existing DateTime, Boolean, composite and transaction helpers are deliberate conveniences (`AGENTS.md`, binding-local `AGENTS.md` files).

**Embedded scripting adapter:**

- Purpose: Provide controlled Lua access to a borrowed database and selected file operations.
- Location: `src/sandbox/`.
- Contains: One `Database` usertype, operation binders, `quiver` namespace, conversion helpers, path policy and return encoder.
- Depends on: C++ API and internal CSV/XLSX readers; sol2 and Lua are private dependencies (`src/CMakeLists.txt`).
- Used by: `Sandbox` host wrappers and `quiver_cli` (`src/c/sandbox.cpp`, `src/cli/main.cpp`).

**Internal format/metadata services:**

- Purpose: Hide parser dependencies from sol2 translation units and public API.
- Location: `src/csv/`, `src/xlsx/`, `src/ui_metadata.cpp`.
- Contains: Pimpl CSV/XLSX readers, plain CSV writer/emitter and optional TOML UI metadata loader.
- Depends on: csv-parser, OpenXLSX/pugixml or toml++; none of their types are public API.
- Used by: Database import/export, Lua file operations and description rendering (`src/database_csv_import.cpp`, `src/database_csv_export.cpp`, `src/sandbox/csv.cpp`, `src/sandbox/xlsx.cpp`, `src/database_describe.cpp`).

## Data Flow

### Primary Request Path: Create an Element from Python

1. `Database.create_element(collection, **kwargs)` builds the internal `Element` and out-id pointer, then calls CFFI (`bindings/python/src/quiverdb/database.py:206`, `bindings/python/src/quiverdb/element.py`).
2. `quiver_database_create_element` validates boundary pointers and calls the owned C++ database; exceptions become status plus last-error text (`src/c/database_create.cpp:7`).
3. The core requires collection/schema, resolves scalar FK labels, validates scalar values and prepares every routed array before writing (`src/database_create.cpp:5`, `src/database_impl.h`).
4. `TransactionGuard` owns a transaction only when SQLite is in autocommit; the scalar INSERT runs through `Impl::execute`, then validated group rows are inserted (`src/database_create.cpp`, `src/database_impl.h`).
5. `Impl::execute` prepares a statement, enforces exact parameter count, trims bound strings and binds `Value` alternatives; RAII finalizes the statement (`src/database.cpp:133`).
6. The row id crosses the C ABI back to Python; the temporary element is destroyed in `finally`; a failure raises `QuiverError` using native last-error text (`bindings/python/src/quiverdb/database.py`, `bindings/python/src/quiverdb/_helpers.py`).

### Open, Schema and Migrations

1. The constructor opens SQLite with requested read-only/read-write flags and enables foreign keys without loading metadata (`src/database.cpp:96`).
2. A schema-dependent call enters `Impl::require_schema`; `Schema::from_database` introspects SQLite, `SchemaValidator` validates, and the completed schema is published only after success (`src/database_impl.h`, `src/schema.cpp`).
3. `from_schema` recreates a file-backed database and applies/validates SQL inside a transaction (`src/database.cpp:294`, `src/database.cpp:478`).
4. `from_migrations` opens the database, discovers numerically named positive version directories, applies pending `up.sql` files with per-migration transactions and updates `PRAGMA user_version` (`src/database.cpp:246`, `src/database.cpp:390`, `src/migrations.cpp`).
5. `from_migrations` also loads optional sibling `ui/` sidecars after migration processing, including an already-up-to-date reopen; malformed optional sidecars warn and degrade (`src/database.cpp:264`, `src/ui_metadata.cpp`).
6. `validate_migrations` uses an in-memory database, applies every up migration, runs every required down migration in reverse order and refuses a round trip leaving tables (`src/database.cpp:268`, `src/database.cpp:442`).

### Nullable Bulk and Group Reads

1. Scalar readers and `read_element_ids` order by collection `rowid` (`src/database_read.cpp`).
2. Vector/set bulk readers LEFT JOIN groups onto the collection and select `g.id` separately from the value, distinguishing no group row from a NULL cell (`src/database_internal.h:31`).
3. C++ returns optional scalars and nested optional cells; the C ABI copies numeric data with presence masks and string data with null pointers (`src/c/database_helpers.h`).
4. Host wrappers decode positionally and free native allocations; Julia consults schema nullability for concrete versus optional arrays (`bindings/julia/src/database_read.jl`, `bindings/python/src/quiverdb/database.py`, `bindings/dart/lib/src/database_read.dart`, `bindings/js/src/read.ts`).
5. Lua creates tables with `nil` holes directly from C++ reads; `read_element_ids` is scalar/bulk element-count authority, while a vector/set inner table has no exact trailing-NULL count authority (`src/sandbox/internal.h`, `src/sandbox/database_read.cpp`).

### Time-Series and Group Shape Translation

- C++ group/time-series rows use `std::vector<std::map<std::string, Value>>`; host writers and time-series group readers use column-oriented `{column: [values]}` (`include/quiver/database.h`, `src/c/database_helpers.h`, `src/c/database_time_series.cpp`).
- Vector/set whole-group host readers deliberately return rows, while their writers take columns; preserve this asymmetry (`src/c/database_read.cpp`, `src/c/database_update.cpp`, `AGENTS.md`).
- C ABI group conversion shares `marshal_group_rows_to_c` / `unmarshal_group_columns_to_rows`; per-cell masks preserve NULL values and a NULL input mask denotes dense data (`src/c/database_helpers.h`, `src/c/database_time_series.cpp`).
- Time-series dimensions derive from primary-key columns, with DATE_TIME identified within those dimensions; validate all rows before deleting/replacing data (`src/database_internal.h`, `src/database_time_series.cpp`).
- Lua time-series writers use dimension columns as row-count authority and missing value cells as NULL; host FFI writers retain equal-length validation (`src/sandbox/database_time_series.cpp`, `bindings/js/src/group-columns.ts`, `bindings/python/src/quiverdb/database.py`, `bindings/dart/lib/src/database_update.dart`, `bindings/julia/src/database_update.jl`).
- Set readers consistently use group `rowid` order, which is not a promised insertion-order contract; vectors use `vector_index` (`src/database_read.cpp`, `AGENTS.md`).

### Lua Script and File Operations

1. `Sandbox::Impl` borrows `Database&`, opens only base/string/table/math/coroutine/utf8, creates one Database usertype and installs all operation binders (`src/sandbox/sandbox.cpp`).
2. `Sandbox::run` evaluates text-only Lua and converts sol2 failures into a C++ runtime error (`src/sandbox/sandbox.cpp`).
3. File-touching binders call `resolve_sandbox_path` before accessing CSV, XLSX, binary data or migration paths; it canonicalizes paths and requires strict containment within the database directory (`src/sandbox/path_policy.cpp`).
4. Whole-file and streaming readers drive the same internal reader per format; CSV/XLSX cells arrive as strings (`src/sandbox/csv.cpp`, `src/sandbox/xlsx.cpp`, `src/csv/csv_read.h`, `src/xlsx/xlsx_read.h`).
5. The first Lua return is JSON-encoded in C++; no return yields an empty string, explicit nil yields `null`, sparse tables become objects; depth, UTF-8, size and key collisions are validated (`src/sandbox/return_json.cpp`).
6. Run-scope cleanup closes registered CSV writers and binary handles even if Lua globals retain them, then collects garbage (`src/sandbox/sandbox.cpp`).
7. C ABI and host Sandbox wrappers return the JSON string unchanged (`src/c/sandbox.cpp`, binding `sandbox` modules).

### Binary Expression Evaluation

1. `BinaryFile` and `Expression` implement `AbstractExpression`; operators create shared lazy nodes rather than computing datasets (`include/quiver/expression/abstract_expression.h`, `src/expression/expression.cpp`).
2. A binary file supplies a fresh path-based leaf, so expression evaluation never borrows the caller's open handle (`src/binary/binary_file.cpp`, `src/expression/expression_file.cpp`).
3. `save` collects input files, rejects input/output path collision, opens inputs, traverses calendar-aware dimensions and computes/writes one row at a time (`src/expression/expression.cpp:42`, `src/binary/iteration.cpp`).
4. Scope guards close inputs, while the output `BinaryFile` uses RAII; metadata sidecars define dimensions, units, labels and calendar properties (`src/expression/expression.cpp`, `src/binary/binary_metadata.cpp`).

**State Management:**

- Database instance state is SQLite handle, schema cache, logger, dry-run flag and optional UI metadata (`src/database_impl.h`).
- Lua state persists across `run` calls; run-owned file handles are closed after each call (`src/sandbox/sandbox.cpp`).
- Expression nodes share ownership and cache mutable row buffers; binary file handles own stream state (`include/quiver/expression/expression_node.h`, `src/binary/binary_file.cpp`).
- Host native loaders retain process/module state separately from database handles (`bindings/js/src/loader.ts`, `bindings/python/src/quiverdb/_c_api.py`, `bindings/julia/src/c_api.jl`, `bindings/dart/lib/src/ffi/library_loader.dart`).

## Key Abstractions

**Value and Element:**

- Purpose: Host-independent SQLite scalar alternatives and separated scalar/array input.
- Examples: `include/quiver/value.h`, `include/quiver/element.h`, `src/element.cpp`.
- Pattern: Plain value containers; NULL is `nullptr`, INTEGER is `int64_t`, REAL is `double`, text/date-time is `std::string`.

**Schema and GroupMetadata:**

- Purpose: Derive behavior from actual SQLite tables and their naming/key conventions.
- Examples: `src/schema.h`, `include/quiver/attribute_metadata.h`, `src/database_metadata.cpp`.
- Pattern: Introspection-backed metadata; group tables are named `{collection}_{vector|set|time_series}_{group}`.

**TransactionGuard and Dry Run:**

- Purpose: Own rollback for autocommit operations and compose with caller-owned transactions.
- Examples: `src/database_impl.h`, `src/database.cpp:356`.
- Pattern: Scope rollback unless committed; no nested SAVEPOINTs. Dry runs live on Database and absorb public begin/commit/rollback calls.

**AbstractExpression and ExpressionNode:**

- Purpose: Compose file-backed arithmetic, comparisons, Boolean operations, selection and aggregation.
- Examples: `include/quiver/expression/abstract_expression.h`, `include/quiver/expression/expression_node.h`.
- Pattern: Protected base copy/move prevents slicing; concrete values hold shared nodes; evaluation occurs at save.

## Entry Points

**Native C++ API:** `include/quiver/quiver.h` / `include/quiver/database.h`; called by embedding applications for factories and database operations.

**C FFI API:** `include/quiver/c/database.h`, `include/quiver/c/sandbox.h`; called by host adapters; factories return owned opaque handles through output pointers.

**Host packages:** `bindings/julia/src/Quiver.jl`, `bindings/dart/lib/quiverdb.dart`, `bindings/python/src/quiverdb/__init__.py`, `bindings/js/mod.ts`; export each binding's public API.

**Lua runtime:** `src/sandbox/sandbox.cpp`; invoked through `Sandbox::run`, exposes the borrowed database as `db` and metadata/expression helpers under `quiver`.

**CLI:** `src/cli/main.cpp`; parses database/script paths, schema or migration creation, read-only options and dry-run scope, then prints returned JSON.

## Architectural Constraints

- **Threading:** Calls are synchronous; SQLite is built thread-safe/serialized, but Quiver adds no database-wide mutex around schema caches, transaction state, Lua state or expression buffers. Do not infer whole-object concurrency safety from SQLite's setting (`cmake/Dependencies.cmake`, `src/database_impl.h`, `src/sandbox/sandbox.cpp`, `include/quiver/expression/expression_node.h`).
- **Global state:** Atomic logger id and one-time SQLite initialization in `src/database.cpp`; thread-local C error string in `src/c/common.cpp`; unguarded process-wide binary write registry in `src/binary/binary_file.cpp`.
- **Circular imports:** The abstract-expression header uses forward declarations instead of including the node header, which includes BinaryFile and returns to the abstract base. Preserve this dependency break (`include/quiver/expression/abstract_expression.h`, `include/quiver/expression/expression_node.h`). JS operation modules attach prototypes and import `Database`; keep public initialization through `bindings/js/src/index.ts`.
- **Ownership:** C buffers are copied into host data and released using matching native free functions, never host deallocators. Sandbox borrows Database and must be destroyed first (`src/c/database_helpers.h`, `src/c/sandbox.cpp`, `src/sandbox/sandbox.cpp`).
- **Transaction semantics:** Validate all possible input before the first write. SQLite constraint/trigger errors can leave earlier writes in a caller-owned transaction because nested guards do not own rollback; preserve the accepted no-SAVEPOINT design (`src/database_impl.h`, `AGENTS.md`).
- **Read consistency:** Multiple aligned attribute reads require an enclosing read transaction if concurrent writes are possible; positional ordering alone does not make separate statements one snapshot (`src/database_read.cpp`, `AGENTS.md`).
- **API coverage:** Bind public C++ behavior through C and every applicable host. Binary/expression are Julia/Lua-only; standalone CSV/XLSX readers/writer are Lua-only internal services; Lua whole-group vector/set readers are deliberately absent (`AGENTS.md`).
- **Sandbox scope:** No file operations for in-memory databases; no os/io/package/debug, no dofile/loadfile, no binary chunks. File-path confinement is Lua policy, not a restriction on native host file APIs (`src/sandbox/sandbox.cpp`, `src/sandbox/path_policy.cpp`).
- **Generated interfaces:** Regenerate Julia/Dart FFI from C headers; manually synchronize Python CFFI and JS symbol signatures (`bindings/julia/generator/`, `bindings/dart/pubspec.yaml`, `bindings/python/src/quiverdb/_c_api.py`, `bindings/js/src/loader.ts`).

## Anti-Patterns

### Routing an Ambiguous Column Instead of a Named Group

**What happens:** Element arrays route by attribute name to every matching group; a shared FK column can reach multiple tables, with a warning (`src/database_impl.h`).
**Why it's wrong:** For a call intending one group, this can replace unrelated rows; per-column reads can also resolve a different group than intended (`src/schema.cpp`, `src/database_read.cpp`).
**Do this instead:** Use `update_vector_group` / `update_set_group` and whole-group readers when exact table identity matters (`src/database_update.cpp`, `src/database_read.cpp`). The name-based API remains an intentional supported surface.

### Treating Lua Sparse Lists as Dense Arrays

**What happens:** SQL NULL becomes `nil`; `#table` and trailing holes do not preserve nullable vector/set row counts (`src/sandbox/internal.h`).
**Why it's wrong:** Scripts or JSON consumers can confuse a missing row with a trailing NULL row (`src/sandbox/return_json.cpp`).
**Do this instead:** Use `read_element_ids` for element alignment, time-series dimensions for row count, or host whole-group readers for exact vector/set row shape (`src/sandbox/database_time_series.cpp`, `src/database_read.cpp`, `AGENTS.md`).

## Error Handling

**Strategy:** Throw native C++ exceptions for validation, lookup and operation failures; catch them at the C ABI or CLI boundary and transport their messages (`src/type_validator.cpp`, `src/c/database_create.cpp`, `src/cli/main.cpp`).

**Patterns:**

- Native diagnostics use `Cannot <operation>: ...` for preconditions, entity-specific not-found text for lookups and `Failed to <operation>: ...` for execution failures (`AGENTS.md`, `src/database.cpp`, `src/database_impl.h`).
- C ABI calls validate pointers with `QUIVER_REQUIRE`, return `QUIVER_OK` / `QUIVER_ERROR`, and publish `e.what()` through `quiver_get_last_error` (`src/c/internal.h`, `src/c/common.cpp`).
- Match every allocation with its native free function and unwind host temporaries with the existing binding pattern (`src/c/database_helpers.h`, `bindings/python/src/quiverdb/database.py`, `bindings/dart/lib/src/database_create.dart`).
- Optional UI sidecars warn and degrade; required database schema and binary metadata fail rather than silently publish partial data (`src/ui_metadata.cpp`, `src/database_impl.h`, `src/binary/binary_metadata.cpp`).

## Cross-Cutting Concerns

**Logging:** Per-database spdlog console and file sinks; file-backed databases write `quiver_database.log` beside the database; logger creation can fall back to console (`src/database.cpp`).

**Validation:** Schema conventions in `src/schema_validator.cpp`; centralized scalar type checks in `src/type_validator.cpp` and `src/database_internal.h`; DATE_TIME grammar in `src/utils/datetime.h`; trust-boundary pointer/Lua type checks in `src/c/internal.h` and `src/sandbox/internal.h`.

**Authentication:** Not applicable to this in-process library. Lua capability restrictions and path confinement govern script access to host resources (`src/sandbox/sandbox.cpp`, `src/sandbox/path_policy.cpp`).

---

*Architecture analysis: 2026-10-07*
