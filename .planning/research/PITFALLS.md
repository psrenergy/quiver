# Pitfalls Research

**Domain:** C++20 library refactoring with C FFI and multi-language bindings (Julia, Dart, Lua)
**Researched:** 2026-02-09
**Confidence:** HIGH (based on direct codebase analysis plus domain expertise)

## Critical Pitfalls

### Pitfall 1: SQL Injection via String Concatenation in Schema Queries

**What goes wrong:**
Table and column names are interpolated directly into SQL strings throughout `database.cpp`. Lines like `"SELECT " + attribute + " FROM " + collection` (line 990), `"UPDATE " + collection + " SET " + attribute + " = ?"` (line 1182), and `"DELETE FROM " + table + " WHERE id = ?"` (line 809) construct SQL from user-supplied collection/attribute names. The same pattern exists in `schema.cpp` where `PRAGMA table_info(` + table + `)` (line 296) and `PRAGMA foreign_key_list(` + table + `)` (line 331) concatenate table names directly into PRAGMA calls.

**Why it happens:**
SQLite parameterized queries (`?` placeholders) only work for values, not for identifiers (table names, column names). Developers correctly parameterize values but forget that identifiers also need sanitization. The `Schema` class acts as a trusted intermediary since table names come from `sqlite_master`, but `collection` and `attribute` parameters in the `Database` public API come from callers.

**How to avoid:**
Validate all collection and attribute names against the loaded `Schema` before constructing SQL. The `require_collection` check already exists for some methods but only verifies existence, not that the string is safe. Add an identifier validation function (e.g., reject anything containing SQL metacharacters: `;`, `'`, `"`, `--`, spaces) or exclusively use allowlisted identifiers from the schema's `TableDefinition::columns` map. Since the Schema already has the full list of valid tables and columns, the fix is to always resolve user-supplied names against that allowlist before interpolation.

**Warning signs:**
- Any `"SELECT " + userInput` or `"UPDATE " + userInput` pattern where `userInput` is not validated against the schema
- Methods that skip `require_schema` / `require_collection` checks (e.g., `read_scalar_relation` at line 951 does manual checks)
- New CRUD methods added without identifier validation

**Phase to address:**
Phase 1 (C++ core restructuring). Must be resolved before any API surface changes, as every read/update/delete method is affected. Add a centralized `validate_identifier()` helper that all SQL-building methods call.

---

### Pitfall 2: C API Memory Ownership Ambiguity During Refactoring

**What goes wrong:**
The C API has complex memory ownership patterns with multiple allocation/free pairs: `quiver_free_integer_array`, `quiver_free_float_array`, `quiver_free_string_array`, `quiver_free_integer_vectors`, `quiver_free_float_vectors`, `quiver_free_string_vectors`, `quiver_free_time_series_data`, `quiver_free_time_series_files`, `quiver_free_scalar_metadata`, `quiver_free_group_metadata`, `quiver_free_scalar_metadata_array`, `quiver_free_group_metadata_array`, `quiver_string_free`. During refactoring, renaming or moving these functions, or changing the allocation strategy (e.g., switching from `new[]` to a different allocator), creates mismatched alloc/free pairs that produce silent memory corruption or leaks.

**Why it happens:**
C API memory management is inherently fragile. Each `read_*` function allocates via `new[]` in `src/c/database.cpp`, and the matching `free_*` deallocates via `delete[]`. If a refactoring changes one side without the other, or if a new C API function reuses an existing free function with a different allocation layout, the mismatch is invisible at compile time. The bindings (Julia finalizer, Dart Arena) each call specific free functions and will crash or leak if the contract changes.

**How to avoid:**
1. Document each alloc/free pair as a formal contract in the C header with comments.
2. When splitting `src/c/database.cpp` (1612 lines), keep all free functions co-located with their corresponding alloc functions in the same translation unit.
3. Add a test for each alloc/free pair that verifies allocation, usage, and deallocation in sequence.
4. Never change an allocation strategy without auditing all four consumers (C tests, Julia, Dart, Lua).

**Warning signs:**
- A `quiver_free_*` function is moved to a different file than its corresponding `quiver_read_*` or `quiver_database_*` function
- A new read function reuses an existing free function without verifying allocation compatibility
- Binding tests pass but valgrind/AddressSanitizer shows leaks or use-after-free

**Phase to address:**
Phase 2 (C API restructuring). When splitting `src/c/database.cpp`, the alloc/free pairs must move together. Each new C API file should be self-contained for its memory lifecycle.

---

### Pitfall 3: Binding Desynchronization After C API Signature Changes

**What goes wrong:**
The Julia (`c_api.jl`, 491 lines), Dart (`bindings.dart`), and Lua (`lua_runner.cpp`) bindings each independently declare the C API function signatures. Julia uses `@ccall` with explicit type annotations. Dart uses `ffigen` to generate `bindings.dart`. Lua uses sol2 usertype bindings. When a C API function signature changes (e.g., parameter order, type change, new parameter), any binding not regenerated will silently call the C function with wrong types, causing stack corruption, segfaults, or data corruption.

**Why it happens:**
There is no compile-time enforcement between the C header declarations and the binding declarations. Julia's `@ccall` and Dart's FFI bindings are text-based reproductions of C signatures. A parameter type change (e.g., `int32_t` to `size_t`, or adding an `out_count` parameter) is invisible to the binding code until runtime. The generators exist (`bindings/julia/generator/generator.bat`, `bindings/dart/generator/generator.bat`) but must be manually triggered.

**How to avoid:**
1. After every C API header change, immediately regenerate all bindings using the generator scripts before running any other tests.
2. Add a CI step that regenerates bindings and diffs against checked-in files; fail if they differ.
3. When changing function signatures, change all four layers atomically: C++ -> C API -> regenerate Julia -> regenerate Dart -> update Lua manually.
4. Run `scripts/test-all.bat` after every C API change, not just `quiver_c_tests.exe`.

**Warning signs:**
- C API tests pass but Julia/Dart tests fail with segfaults or garbage data
- A function was added to `include/quiver/c/database.h` but is not present in `c_api.jl`
- `bindings.dart` file timestamp is older than the C API header it wraps
- `quiver_element_set_array_integer` uses `int32_t count` in C but Julia declares `Int32` while the Dart binding might assume `int` (platform-dependent size)

**Phase to address:**
Every phase that touches C API signatures. Must be a gate criterion: no phase is complete until all bindings are regenerated and all binding tests pass.

---

### Pitfall 4: Blob Type Stub Silently Corrupting Data

**What goes wrong:**
The `execute()` method in `database.cpp` (line 359) throws `std::runtime_error("Blob not implemented")` when encountering `SQLITE_BLOB` column types. If any schema evolution introduces BLOB columns, or if a migration adds one, every read operation on tables containing that column will throw, but the column can be written via `execute_raw()` without error. This creates databases with unreadable data.

**Why it happens:**
The blob stub was likely a placeholder that was never revisited. The `SQLITE_BLOB` case in the `execute()` switch statement (line 358-359) is the only type that throws rather than producing a `Value`. The `Value` type (`std::variant<std::nullptr_t, int64_t, double, std::string>`) has no blob variant. Since the schema validator does not reject schemas containing BLOB columns, nothing prevents them from being created.

**How to avoid:**
1. Add BLOB to the `Value` variant (e.g., as `std::vector<uint8_t>`) or explicitly reject BLOB columns in the `SchemaValidator`.
2. If deferring blob support, make the `SchemaValidator` reject schemas containing BLOB-typed columns with a clear error message during `apply_schema()` / `migrate_up()`, rather than failing at read time.
3. Never leave stub `throw` statements in data paths without a corresponding validation gate that prevents reaching them.

**Warning signs:**
- A `tests/schemas/valid/` schema file containing BLOB columns passes validation but fails at read time
- User reports "Blob not implemented" error after migrating an existing database
- The `export.h` / `data_type.h` files have no Blob enum value

**Phase to address:**
Phase 1 (C++ core restructuring). Either implement blob support or add the rejection gate to `SchemaValidator`. This must happen before any file decomposition since the fix touches `database.cpp`, `schema_validator.cpp`, and potentially `value.h`.

---

### Pitfall 5: TransactionGuard Destructor Silently Swallowing Rollback Failures

**What goes wrong:**
The `TransactionGuard` destructor in `database.cpp` (lines 249-253) calls `rollback()` when `committed_` is false. The `rollback()` method (lines 224-235) catches its own errors and logs them but does not throw. If the SQLite connection is in a bad state (e.g., disk full, corrupted WAL), the rollback silently fails and the database is left in an inconsistent state with a half-applied transaction. The caller receives the original exception from the operation that failed, never learning that the rollback also failed.

**Why it happens:**
Destructors cannot throw in C++ (well, they should not), so the design is correct in principle. But the caller pattern assumes rollback always succeeds. If `begin_transaction()` succeeds and then an operation fails, the `TransactionGuard` destructor calls `rollback()`, which logs the failure and returns. The database connection may now be in an indeterminate transaction state.

**How to avoid:**
1. After a failed rollback, set a flag on the `Impl` that marks the connection as unhealthy (e.g., `impl_->db_healthy = false`).
2. Make `is_healthy()` check this flag, so subsequent operations fail fast with "Database connection in unhealthy state after failed rollback" rather than producing further corruption.
3. Alternatively, close and reopen the SQLite connection on rollback failure, since the connection state is unknown.

**Warning signs:**
- Log messages showing "Failed to rollback transaction" followed by subsequent successful operations
- Data corruption that only appears after disk pressure or I/O errors
- `is_healthy()` returns true even after a logged rollback failure

**Phase to address:**
Phase 1 (C++ core restructuring). Address alongside the TransactionGuard refactoring when splitting `database.cpp`.

---

### Pitfall 6: Naming Inconsistency Cascade Across Four Layers

**What goes wrong:**
Renaming a function for consistency (e.g., `read_scalar_integers` -> `read_integers` or `delete_element_by_id` -> `delete_by_id`) requires coordinated changes across: (1) C++ header + implementation, (2) C API header + implementation, (3) Julia generator + generated code + wrapper code, (4) Dart generator + generated code + wrapper code, and (5) Lua sol2 bindings. Missing any one layer creates a silent link failure (Dart/Julia) or compile error (C++/C). Partial renames where some layers use old names and others use new names create confusion.

**Why it happens:**
The project has five separate layers that each independently declare the same logical API. There is no single source of truth that generates all layers. The Julia and Dart generators parse C headers but the Lua bindings in `lua_runner.cpp` are hand-written sol2 registrations. A rename in the C header propagates to Julia/Dart via generators but requires manual Lua updates. And the higher-level Julia/Dart wrapper files (e.g., `database_read.jl`, `database_read.dart`) have their own idiomatic names that are not auto-generated.

**How to avoid:**
1. Create a naming convention document that maps each logical operation to its name in each layer: C++ method, C function, Julia function, Dart method, Lua method.
2. Rename in atomic batches: all five layers in a single commit.
3. Run `scripts/test-all.bat` after every rename.
4. Consider making the Lua bindings table-driven (a registration list) rather than individually hand-written lambda registrations, so additions/renames are less error-prone.

**Warning signs:**
- C++ tests pass, C tests pass, but Julia or Dart tests fail on "symbol not found"
- Two layers using different names for the same operation (e.g., Julia says `read_time_series_group` but Dart says `readTimeSeriesGroupById`)
- Lua bindings expose fewer methods than the C API header declares

**Phase to address:**
Phase 3 (naming standardization). This should be a dedicated phase with a checklist of every function across all five layers. Do not interleave naming changes with functional changes.

---

### Pitfall 7: File Decomposition Breaking Pimpl Encapsulation

**What goes wrong:**
The `Database::Impl` struct (defined at line 166 of `database.cpp`) contains the sqlite3 pointer, logger, schema, type_validator, and the `TransactionGuard` inner class. When splitting `database.cpp` (1934 lines) into multiple files (e.g., `database_read.cpp`, `database_create.cpp`, `database_update.cpp`), each new file needs access to the `Impl` struct. If `Impl` is moved to a shared internal header, the Pimpl encapsulation is violated -- sqlite3.h and spdlog headers become transitively included by all files that include the internal header, defeating the purpose of Pimpl.

**Why it happens:**
The entire `Database` class uses a single `Impl` that holds all state. Every method accesses `impl_->db`, `impl_->logger`, `impl_->schema`, or calls `impl_->require_collection()`. Splitting the implementation file without splitting the `Impl` struct forces the internal header to expose everything. But splitting `Impl` into sub-structs creates a different complexity: the transaction guard needs `db`, logging needs `logger`, validation needs `schema`, etc.

**How to avoid:**
1. Create a single `src/database_impl.h` (internal, not in `include/`) that defines `Database::Impl` and is only included by `src/database_*.cpp` files. This is the standard approach: Pimpl hides dependencies from public headers, not from the library's own translation units.
2. Do NOT put `database_impl.h` in `include/quiver/`. It must stay in `src/`.
3. Ensure CMakeLists.txt does not install `database_impl.h`.
4. Each split file includes `database_impl.h` and implements a subset of `Database` methods.

**Warning signs:**
- New files in `include/quiver/` that include `<sqlite3.h>` or `<spdlog/spdlog.h>`
- Build times increase because translation units now parse sqlite3/spdlog headers that were previously hidden
- The public `database.h` header gains new includes

**Phase to address:**
Phase 1 (C++ core file decomposition). This is the first decision to make before any code is moved. Get the internal header structure right, then split.

---

### Pitfall 8: Schema Naming Convention Fragility (`get_parent_collection` Using First Underscore)

**What goes wrong:**
`Schema::get_parent_collection()` in `schema.cpp` (line 112-118) extracts the parent collection by taking the substring before the first underscore: `table.substr(0, table.find('_'))`. If a collection name ever contains an underscore (even though the validator currently rejects them), or if a naming convention like `TimeSeries` is used for a collection that has child tables like `TimeSeries_vector_values`, the parsing breaks. The `is_collection()` method (line 80-85) uses `table.find('_') == std::string::npos` to identify collections, meaning `Configuration` is hardcoded as a special case.

**Why it happens:**
The schema naming convention relies on string parsing rather than storing explicit parent-child relationships. The validator at `schema_validator.cpp` line 60-63 rejects collection names with underscores, but this is a validation rule, not a structural guarantee. If the validator rule is relaxed (e.g., to allow `My_Collection`), the entire table classification system breaks.

**How to avoid:**
1. During schema loading (`Schema::load_from_database`), explicitly build a map of `table -> parent_collection` using foreign key analysis rather than string parsing.
2. Classify tables during loading based on their foreign keys and column structure, not their names.
3. If name-based classification must remain, document it as an invariant and add regression tests for edge cases.
4. Never relax the "no underscores in collection names" rule without first replacing the string-based classification.

**Warning signs:**
- A test schema with a collection name containing underscores passes validation but produces wrong parent collection mappings
- `list_vector_groups()` returns empty for a collection whose vector tables exist
- `find_table_for_column()` fails to find a table that clearly exists in the schema

**Phase to address:**
Phase 1 (C++ core restructuring), specifically when refactoring `schema.cpp`. Consider switching to a structural classification approach.

---

## Technical Debt Patterns

Shortcuts that seem reasonable but create long-term problems.

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| String concatenation for SQL identifiers | Simpler code, no identifier quoting needed | SQL injection risk, fragile with special characters | Never -- always validate against schema |
| Duplicated code across `read_vector_*`, `read_set_*`, `update_vector_*`, `update_set_*` | Each method is self-contained and readable | 18+ nearly-identical methods in `database.cpp` that drift apart during edits | Only in initial prototyping; template-refactor during file decomposition |
| Hand-written Lua sol2 bindings | Direct control, no generator dependency | Lua bindings lag behind C API, miss new functions, naming drifts | Acceptable if Lua is low-priority; add tracking checklist |
| `export_to_csv` / `import_from_csv` as empty stubs (lines 1818-1824) | Placeholder for future feature | Bound to C API and all bindings; callers get silent no-op | Only if callers are warned; currently returns `void` with no error |
| `SQLITE_BLOB` throwing runtime_error | Defers blob implementation | Databases with blob columns crash at read time, not at schema load | Never without a validation gate |

## Integration Gotchas

Common mistakes when connecting the layers of this codebase.

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| C++ -> C API | Returning `const char*` pointing to C++ internal `std::string` that gets destroyed (e.g., `quiver_database_path` returns `db->db.path().c_str()` which is valid only while `db` lives) | Document lifetime guarantees; for short-lived strings, use `strdup_safe()` and require caller to free |
| C API -> Julia | Julia `@ccall` type mismatches (e.g., `Int32` vs `Cint` vs `Int64`) silently produce wrong values | Always use Julia's C-compatible types (`Cint`, `Csize_t`, `Cdouble`) and verify against C header |
| C API -> Dart | Dart `Arena` allocation freed before native function reads the data | Ensure Arena is not released until after the C function returns; use `try/finally` blocks consistently |
| C API -> Lua | sol2 lambda captures reference to `Database&` which must outlive `LuaRunner` | Enforce that `LuaRunner` destructor runs before `Database` destructor; document ownership requirement |
| Schema -> PRAGMA | Table names from user input passed to `PRAGMA table_info()` without quoting | In `schema.cpp`, table names come from `sqlite_master` (trusted); in `database.cpp`, they come from callers (untrusted) -- validate before use |

## Performance Traps

Patterns that work at small scale but fail as usage grows.

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| Per-row `INSERT` in loops (create_element, update_vector, update_set) | Slow writes for large vectors/sets/time_series | Use multi-row INSERT or prepared statement reuse within transaction | > 1000 elements per vector/set |
| Full table scan in `read_scalar_*` (no WHERE clause) | Slow reads as table grows | This is by design (read all elements), but consider pagination API | > 100K rows per collection |
| `Schema::from_database` runs N+1 PRAGMA queries (1 per table) | Slow database opening with many tables | Cache schema or batch introspection | > 50 tables in schema |
| `find_table_for_column` iterates all tables sequentially | Slow column routing in create_element with many tables | Build a column-to-table index during schema loading | > 20 group tables per collection |

## Security Mistakes

Domain-specific security issues beyond general web security.

| Mistake | Risk | Prevention |
|---------|------|------------|
| Collection/attribute names interpolated into SQL without validation | SQL injection through malicious collection or attribute names passed by binding callers | Validate all identifiers against the Schema's known tables/columns before SQL construction |
| `execute_raw()` exposed as private method but callable from `migrate_up()` with file contents | Malicious migration SQL could execute arbitrary commands (ATTACH, load_extension) | Disable SQLite extension loading; validate migration SQL if migrations come from untrusted sources |
| `quiver_get_last_error()` returns thread-local string | Error messages from one thread could leak to another if thread-local storage is not correctly initialized | Verify `thread_local` storage works correctly on all target platforms (Windows TLS has nuances with DLLs) |

## "Looks Done But Isn't" Checklist

Things that appear complete but are missing critical pieces.

- [ ] **File decomposition:** Splitting `database.cpp` into `database_read.cpp`, `database_create.cpp`, etc. looks done, but verify that `Database::Impl` is in a private internal header (not public), and that build times actually improved
- [ ] **C API function renaming:** Function renamed in C header, but check: (1) Julia generator re-run, (2) Dart generator re-run, (3) Lua sol2 binding updated, (4) all four test suites pass
- [ ] **New CRUD method added:** C++ implementation exists and C++ tests pass, but check: (1) C API wrapper exists, (2) C API test exists, (3) Julia binding wrapper exists, (4) Dart binding wrapper exists, (5) Lua binding registered
- [ ] **Memory free function added:** `quiver_free_*` function exists in header, but verify: (1) Implementation in `.cpp` matches allocation strategy, (2) Julia finalizer calls it, (3) Dart wrapper calls it, (4) no double-free or leak in any binding
- [ ] **Schema validator updated:** New validation rule added, but verify: (1) invalid schema test in `tests/schemas/invalid/` exercises it, (2) error message is descriptive, (3) existing valid schemas still pass

## Recovery Strategies

When pitfalls occur despite prevention, how to recover.

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| SQL injection in identifier | MEDIUM | Add identifier validation function; audit all SQL-building code; no data loss since SQLite STRICT mode limits damage |
| Memory ownership mismatch (C API) | HIGH | Run AddressSanitizer on all test suites; audit every alloc/free pair; fix mismatches; re-test all bindings |
| Binding desynchronization | LOW | Re-run generators; diff output; fix any manual bindings; run test-all.bat |
| Blob stub hit in production | LOW | Add SchemaValidator rejection for BLOB columns; or implement blob support; re-validate existing schemas |
| TransactionGuard rollback failure | HIGH | Add connection health tracking; audit all code paths that use TransactionGuard; add integration tests for I/O failure scenarios |
| Naming cascade incomplete | MEDIUM | Grep for old name across all files; fix remaining references; run test-all.bat; update naming map |
| Pimpl broken by file split | MEDIUM | Move internal header from `include/` to `src/`; verify public header includes unchanged; rebuild and check compile times |
| Schema naming fragility | HIGH | Redesign table classification to use structural analysis; extensive regression testing on all existing schemas |

## Pitfall-to-Phase Mapping

How roadmap phases should address these pitfalls.

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| SQL injection in identifiers | Phase 1: C++ core restructuring | All SQL-building methods validated by `validate_identifier()`; no raw string concatenation of user input into SQL |
| C API memory ownership | Phase 2: C API restructuring | Each alloc/free pair co-located; AddressSanitizer clean on full test suite |
| Binding desynchronization | Every phase touching C API | Generator re-run + diff check after every C API change; `test-all.bat` passes |
| Blob type stub | Phase 1: C++ core restructuring | SchemaValidator rejects BLOB columns OR blob variant added to Value type |
| TransactionGuard failures | Phase 1: C++ core restructuring | Connection health flag set on rollback failure; `is_healthy()` reflects it |
| Naming inconsistency cascade | Phase 3: naming standardization | Naming map document with all five layers; test-all.bat passes |
| Pimpl encapsulation break | Phase 1: C++ file decomposition | `database_impl.h` exists in `src/`, not `include/`; public header includes unchanged |
| Schema naming fragility | Phase 1: C++ core restructuring | `get_parent_collection` tested with edge cases; consider structural classification |

## Sources

- Direct codebase analysis of `src/database.cpp` (1934 lines), `src/c/database.cpp` (1612 lines), `src/schema.cpp`, `src/schema_validator.cpp`, and all binding files
- [C++ Stories - The Pimpl Pattern](https://www.cppstories.com/2018/01/pimpl/) - Pimpl decomposition pitfalls
- [C Wrappers for C++ Libraries and Interoperability](https://caiorss.github.io/C-Cpp-Notes/CwrapperToQtLibrary.html) - C API wrapper patterns
- [FFI Wikipedia](https://en.wikipedia.org/wiki/Foreign_function_interface) - Cross-language FFI type mapping challenges
- [SQLite Injection Payloads](https://github.com/swisskyrepo/PayloadsAllTheThings/blob/master/SQL%20Injection/SQLite%20Injection.md) - SQLite-specific injection techniques
- [Dart C interop](https://dart.dev/interop/c-interop) - Dart FFI binding patterns
- [Real-World Cross-Language Bugs](https://chapering.github.io/pubs/fse25haoran.pdf) - Academic study on cross-language bug patterns

---
*Pitfalls research for: C++20 library refactoring with C FFI and multi-language bindings*
*Researched: 2026-02-09*
