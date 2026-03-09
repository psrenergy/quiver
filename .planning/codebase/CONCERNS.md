# Codebase Concerns

**Analysis Date:** 2026-03-08

## Tech Debt

**SQL Identifier Injection Risk (Medium Severity):**
- Issue: Collection names, attribute names, and group names are interpolated directly into SQL strings throughout the C++ layer. While `require_collection` and `require_column` guard against unknown names (by checking schema metadata), a hypothetical schema that defines a collection with a specially crafted name would pass those guards. The `is_safe_identifier` check (alphanumeric + underscore only) is only applied internally during PRAGMA queries in `Schema::query_columns/query_foreign_keys/query_indexes`, not at the write/read SQL construction sites.
- Files: `src/database_read.cpp`, `src/database_create.cpp`, `src/database_update.cpp`, `src/database_delete.cpp`, `src/database_csv_export.cpp`, `src/database_csv_import.cpp`
- Impact: Attacker-controlled schema definitions (via `from_schema`) could embed SQL metacharacters in table or column names, leading to query corruption. Legitimate use where schema comes from trusted sources is safe today.
- Fix approach: Apply `is_safe_identifier` validation from `src/schema.cpp` (line 11) in `require_collection` and `require_column`, or add a schema-load-time validation that all table and column names match the safe identifier pattern.

**CSV Import Disables Foreign Keys Globally:**
- Issue: `Database::import_csv` calls `PRAGMA foreign_keys = OFF` and then `PRAGMA foreign_keys = ON` directly, bypassing the normal connection-level FK enforcement. This is done twice: once for collection import (line 370) and once for group import (line 602). If an exception occurs between these two calls in the group import path, FK state may remain disabled until the next call restores it.
- Files: `src/database_csv_import.cpp` (lines 370–473, 602–670)
- Impact: After a failed CSV import, a subsequent operation on the same connection could silently violate referential integrity until the next CSV import or connection close. In practice the rollback path does restore FK state (line 476), but the exception path for group import relies on the caller reaching that restore.
- Fix approach: Wrap FK disabling in a RAII guard that restores the state in its destructor, ensuring the pragma is always re-enabled even on unexpected exceptions.

**`lua_runner.cpp` is the Largest Source File (868 lines):**
- Issue: `src/lua_runner.cpp` contains all Lua bindings in a single monolithic file. The file repeats the same conversion pattern (read scalar → make table, read vector → make nested table, etc.) across all data types with hand-written per-type lambdas.
- Files: `src/lua_runner.cpp`
- Impact: High maintenance burden when adding new C++ Database methods — each must be manually mirrored with Lua conversion boilerplate. Risk of divergence between C++ API and Lua bindings.
- Fix approach: Extract shared conversion helpers (e.g., `vector_to_lua_table<T>`) into a reusable template. Consider splitting into `lua_runner_read.cpp`, `lua_runner_update.cpp`, etc., mirroring the C API file structure.

**`database_impl.h` Contains Complex Implementation Logic (370 lines):**
- Issue: `src/database_impl.h` is a private header included by all database translation units. It contains `Impl::insert_group_data`, `Impl::resolve_element_fk_labels`, `Impl::resolve_fk_label`, `TransactionGuard`, and all transaction helper methods — amounting to ~280 lines of non-trivial implementation in a header.
- Files: `src/database_impl.h`
- Impact: Every compilation unit that includes `database_impl.h` recompiles all this logic. Slows incremental builds slightly. Also makes the implementation harder to navigate since the "real" code is split between `.h` and scattered `.cpp` files.
- Fix approach: Move the bodies of `insert_group_data` and `resolve_element_fk_labels` into a `database_helpers.cpp` file, keeping only declarations in the header.

**`QUIVER_REQUIRE` Macro Capped at 9 Arguments:**
- Issue: `src/c/internal.h` defines `QUIVER_REQUIRE_1` through `QUIVER_REQUIRE_9` as discrete macros with hand-expanded repetition (lines 39–127). The variadic dispatcher is hardcoded for a maximum of 9 arguments.
- Files: `src/c/internal.h`
- Impact: Adding a C API function with 10+ pointer arguments would silently fail to validate the 10th argument (or require adding another REQUIRE_10 macro). Error-prone boilerplate.
- Fix approach: Replace with a fold expression or X-macro that is not argument-count-limited. Alternatively, use a helper template function. Practical risk is low since few functions exceed 9 pointer parameters currently.

**`database_csv_import.cpp` is 689 Lines with Complex Control Flow:**
- Issue: The `import_csv` function handles collection import and group import in a single large function body with multiple nested validation passes, two distinct code paths (collection vs group), and inline error handling. The two-pass pattern (validate then import) is repeated for both paths.
- Files: `src/database_csv_import.cpp`
- Impact: Hard to test edge cases in isolation. Bug risk increases with complexity. The validation and import phases are interleaved with table-type-switching logic.
- Fix approach: Split into `import_csv_collection` and `import_csv_group` private helpers, each with a clear validate → delete → insert structure.

**Thread-Local Error State for C API:**
- Issue: The C API stores the last error in a `static thread_local std::string g_last_error` (`src/c/common.cpp`, line 8). The `quiver_lua_runner` uses a separate per-instance `std::string last_error` field (`src/c/lua_runner.cpp`, line 11) instead of the shared thread-local, creating two different error reporting mechanisms in the same API layer.
- Files: `src/c/common.cpp`, `src/c/lua_runner.cpp`
- Impact: Callers who check `quiver_get_last_error()` after a Lua failure will get the wrong error (or empty string) if they do not also call `quiver_lua_runner_get_error`. Inconsistent error reporting pattern.
- Fix approach: Have `quiver_lua_runner_run` also write to `quiver_set_last_error()` in addition to the runner's own field, so callers can use either mechanism consistently.

## Known Bugs

**`open_database` Not Exposed as a High-Level Binding (Julia, Dart, Python):**
- Symptoms: `quiver_database_open` (opens an existing database without a schema) exists in the C API (`include/quiver/c/database.h` line 33, `src/c/database.cpp` line 11). The low-level `c_api.jl` wraps it (`bindings/julia/src/c_api.jl` line 96). However, none of the three binding layers expose a user-facing `open_database` / `Database.open` / `from_existing` function. Users who want to open a pre-existing database without re-applying a schema must work around this.
- Files: `bindings/julia/src/database.jl`, `bindings/dart/lib/src/database.dart`, `bindings/python/src/quiverdb/database.py`
- Trigger: Any caller that wants to open an existing database for read-only use or without running migrations.
- Fix approach: Add `open_database(path; kwargs...)` to Julia, `Database.open(path)` factory to Dart, and `Database.open(path)` static method to Python, each calling through to `quiver_database_open`.

**Issue 70 Regression Test Only in Dart:**
- Symptoms: `tests/schemas/issues/issue70/` exists with a migration schema. The Dart test suite has a regression test (`bindings/dart/test/issues_test.dart` lines 26–44). The C++ core test file (`tests/test_issues.cpp`) only contains the Issue 52 test and has no test for Issue 70.
- Files: `tests/test_issues.cpp`, `bindings/dart/test/issues_test.dart`
- Impact: Issue 70 regression is not verified at the C++ or Julia or Python layers — only Dart catches a recurrence.
- Fix approach: Add `TEST_F(IssuesFixture, Issue70)` to `tests/test_issues.cpp` and corresponding tests in the Julia and Python test suites.

## Security Considerations

**Schema Identifier Names Not Validated at Schema Load Time:**
- Risk: Any database schema that is loaded via `from_schema` or `from_migrations` with table or column names containing SQL metacharacters (e.g., `"`, `;`, spaces) would bypass the current guard (`require_collection`/`require_column` check for schema membership) and still produce malformed SQL at write time. The `is_safe_identifier` check only guards PRAGMA-level introspection in `Schema::query_columns`.
- Files: `src/schema_validator.cpp`, `src/database_read.cpp`, `src/database_create.cpp`
- Current mitigation: Schema validator validates structural correctness but does not enforce that all identifiers are safe (alphanumeric + underscore). Collection names are validated to not contain underscores (`src/schema_validator.cpp` line 60), which partially constrains them — but column names have no such check.
- Recommendations: Add an `is_safe_identifier` call inside `SchemaValidator::validate_collection` and `validate_vector_table` for all column names. This would fully close the risk at schema load time rather than at query time.

**Log File Disclosure via World-Readable Log:**
- Risk: `src/database.cpp` line 72 creates a `quiver_database.log` file in the same directory as the database file, using `spdlog::sinks::basic_file_sink_mt` with `truncate=true`. The log contains database paths, operation names, and potentially element IDs. Log permissions are whatever the process umask allows — no explicit permission restriction is set.
- Files: `src/database.cpp` (lines 69–80)
- Current mitigation: None beyond OS-level umask.
- Recommendations: Use `spdlog::sinks::rotating_file_sink_mt` to cap log size, and consider setting explicit file permissions (0600) on the log file at creation.

## Performance Bottlenecks

**`read_scalars_by_id` / `read_vectors_by_id` Issue N+1 Queries:**
- Problem: The convenience helpers `read_scalars_by_id`, `read_vectors_by_id`, `read_sets_by_id` (Julia: `bindings/julia/src/database_read.jl` lines 372–430; Python: `bindings/python/src/quiverdb/database.py` lines 1263–1325) issue one SQL query per attribute per element. For a collection with 10 scalar attributes, `read_scalars_by_id` issues 10 separate `SELECT attr FROM collection WHERE id = ?` queries.
- Files: `bindings/julia/src/database_read.jl`, `bindings/python/src/quiverdb/database.py`, `bindings/dart/lib/src/database_read.dart`
- Cause: Implemented by calling individual `read_scalar_*_by_id` functions in a loop over `list_scalar_attributes`.
- Improvement path: Implement a single `SELECT * FROM collection WHERE id = ?` query at the C++ layer and expose it as `read_element_scalars_by_id`, then call that from the convenience helpers.

**Schema Load on Every Database Open:**
- Problem: `Database::Impl::load_schema_metadata()` is called every time a database is opened. It runs `Schema::from_database` (multiple PRAGMA queries: table names, column info, foreign keys, index info for each table), then constructs a `TypeValidator`. For schemas with many tables (dozens of collections each with multiple group tables), this is O(N) PRAGMA round-trips at open time.
- Files: `src/database_impl.h` (line 286–291), `src/schema.cpp` (lines 294–330)
- Cause: No schema caching between opens. Each `Database` object re-introspects the entire SQLite schema.
- Improvement path: For read-only databases, the schema is immutable after open — consider caching the serialized schema state. For write-enabled, document that schema is refreshed only on open.

**CSV Import Full Read into Memory:**
- Problem: `import_csv` reads the entire CSV file into a `std::string content` before parsing (`src/database_csv_import.cpp` line 41). For large CSV files (multi-GB), this doubles memory usage: once for the string, once for the rapidcsv `Document`.
- Files: `src/database_csv_import.cpp` (lines 36–115)
- Cause: Required for the trailing-comma stripping logic which needs to see the header before processing subsequent rows.
- Improvement path: Stream-parse the header separately to detect trailing columns, then stream-parse rows one at a time during import.

**Time Series: Full Row Materialization in C API:**
- Problem: `quiver_database_read_time_series_group` in `src/c/database_time_series.cpp` (line 73) calls `db.read_time_series_group(collection, group, id)` which returns `vector<map<string, Value>>` (row-major), then immediately transposes to columnar format for the C caller. This produces two full copies of the data in memory simultaneously during the conversion.
- Files: `src/c/database_time_series.cpp` (lines 85–184)
- Cause: The C++ layer uses row-major storage; the C API exposes columnar output. Conversion is done after full materialization.
- Improvement path: Add a columnar read path in the C++ `Database` class to avoid the transpose overhead.

## Fragile Areas

**`Schema::get_parent_collection` Uses First Underscore Only:**
- Files: `src/schema.cpp` (lines 118–124)
- Why fragile: `get_parent_collection` splits on the first `_` in a table name. This works because collection names cannot contain underscores (enforced by `SchemaValidator::validate_collection_names`). However, if that validation is ever relaxed or bypassed, collection names with underscores would produce incorrect parent derivation, silently breaking all group table routing for those collections.
- Safe modification: Do not relax the collection name underscore restriction without also updating `get_parent_collection` and the group table naming convention.
- Test coverage: `test_schema_validator.cpp` tests the validation, but does not test `get_parent_collection` directly with edge cases.

**`Blob::next_dimensions` Time Dimension Reset Logic:**
- Files: `src/blob/blob.cpp` (lines 210–241)
- Why fragile: The "adjust time dimensions which were reset to 1" correction logic (lines 226–238) is sensitive to the ordering of dimensions in the `dimensions` vector and the `parent_dimension_index` values. An incorrect `parent_dimension_index` in a `BlobMetadata` will produce silent incorrect dimension iteration — values wrap incorrectly without raising an exception.
- Safe modification: Always validate `parent_dimension_index` is in bounds and refers to a time dimension before calling `next_dimensions`. `BlobMetadata::validate()` catches structural issues but not logical ordering mistakes post-construction.
- Test coverage: `tests/test_blob_time_properties.cpp` tests basic time properties; `tests/test_blob_csv.cpp` tests CSV round-trips. No tests deliberately corrupt `parent_dimension_index` to verify error detection.

**`TransactionGuard` Silently No-ops Inside Explicit Transactions:**
- Files: `src/database_impl.h` (lines 337–365)
- Why fragile: `TransactionGuard` checks `sqlite3_get_autocommit()` to decide if it owns the transaction. If user code calls `begin_transaction()` followed by a mutating operation, the `TransactionGuard` inside that operation becomes a no-op. If the user's outer transaction is later rolled back, the partial write from the inner operation is correctly undone. However, if the user calls `commit()` on the outer transaction and an inner write fails mid-operation (before calling `guard.commit()`), the outer transaction is committed with incomplete state — no exception is thrown at the outer level.
- Safe modification: Write operations should be called inside explicit transactions when atomicity across multiple operations is needed. Do not mix explicit `begin_transaction()` with operations that fail partway through and expect rollback at the outer level without explicit error checking.
- Test coverage: `tests/test_database_transaction.cpp` covers basic transaction control. Nested transaction scenarios are partially covered but not exhaustively.

**`compute_time_dimension_initial_values` Throws `std::logic_error` for Unimplemented Paths:**
- Files: `src/blob/blob_metadata.cpp` (lines 37, 46)
- Why fragile: `TimeFrequency::Yearly` and `TimeFrequency::Weekly` as inner time dimensions throw `std::logic_error` with the message "YEARLY frequency not implemented" / "WEEKLY frequency not implemented". This means `BlobMetadata::from_element()` and any path that calls `compute_time_dimension_initial_values` with those frequencies is partially unimplemented. The error only appears at runtime.
- Safe modification: Never use `Yearly` or `Weekly` as an inner (non-outermost) time dimension. This constraint is not documented in the public `TimeFrequency` enum or `Dimension` headers.
- Test coverage: No test exercises the error paths for these frequencies.

## Scaling Limits

**Large Collections: Full-Table Reads With No Pagination:**
- Current capacity: All `read_scalar_*`, `read_vector_*`, `read_set_*`, and `read_time_series_group` functions return the full result set. No LIMIT/OFFSET or cursor-based pagination is exposed.
- Limit: Memory-bound by available RAM. For collections with millions of rows or large vector/time series data, a single read call materializes everything.
- Scaling path: Add optional `limit`/`offset` parameters to read functions, or a cursor/iterator API. Currently not planned per the WIP status.

**SQLite Default WAL Mode Not Configured:**
- Current capacity: SQLite defaults to DELETE journal mode. No `PRAGMA journal_mode = WAL` or `PRAGMA synchronous` configuration is applied at database open (`src/database.cpp` lines 116–117 only enable foreign keys).
- Limit: Under concurrent read/write workloads, DELETE journal mode serializes all readers with writers, limiting throughput.
- Scaling path: Add WAL mode as a `DatabaseOptions` flag or enable it unconditionally at open time for file-based databases.

## Dependencies at Risk

**sol2 (Lua Binding Library):**
- Risk: `sol2` is a header-only library used extensively in `src/lua_runner.cpp`. The CLAUDE.md notes recent activity around dropping `std::ranges` and `std::format` for Clang compatibility (commit `d99e2f4`), suggesting active ABI/compatibility work. sol2's own compatibility with different Lua versions and C++20 compilers varies.
- Impact: A compiler toolchain upgrade or Lua version change could require sol2 updates that break `lua_runner.cpp` binding signatures.
- Migration plan: No immediate action needed. Monitor sol2 releases when upgrading compiler or Lua.

**Hand-Written Python CFFI Declarations (`_c_api.py`):**
- Risk: `bindings/python/src/quiverdb/_c_api.py` contains hand-written `ffi.cdef()` declarations that must be kept manually in sync with `include/quiver/c/` headers. A reference copy exists in `_declarations.py` but the CFFI-active declarations in `_c_api.py` are maintained separately.
- Impact: If a C API signature changes (new parameter, renamed enum value) and `_c_api.py` is not updated, Python will silently use the stale declaration, causing incorrect behavior or crashes at call sites rather than an import-time error.
- Migration plan: The generator at `bindings/python/generator/generator.bat` should be run whenever C API headers change. Enforce this in CI or add a diff check between `_declarations.py` (generator output) and `_c_api.py` (active declarations).

## Missing Critical Features

**No `Database.open` (Open Existing Database) in High-Level Bindings:**
- Problem: `quiver_database_open` exists in the C API to open a pre-existing database file without running a schema or migrations. None of Julia, Dart, or Python expose this at the high-level API surface.
- Blocks: Users who want read-only access to a pre-existing database or who manage schema separately.
- Related files: `include/quiver/c/database.h` (line 33), `bindings/julia/src/database.jl`, `bindings/dart/lib/src/database.dart`, `bindings/python/src/quiverdb/database.py`

**Yearly and Weekly Frequencies Unimplemented as Inner Time Dimensions:**
- Problem: `TimeFrequency::Yearly` and `TimeFrequency::Weekly` throw `std::logic_error` when used as inner (non-outermost) time dimensions in `BlobMetadata`. The enum values exist and are parsed by `frequency_from_string`, so a valid TOML metadata file could trigger the error.
- Blocks: Blob files with Yearly/Weekly inner time dimension hierarchies cannot be used.
- Related files: `src/blob/blob_metadata.cpp` (lines 37, 46), `include/quiver/blob/time_properties.h`

## Test Coverage Gaps

**C++ Tests: No Direct Coverage of `read_element_ids`:**
- What's not tested: `Database::read_element_ids` is defined in `src/database_read.cpp` (line 159). It is used internally by `import_csv` helpers but has no direct test case in `tests/test_database_read.cpp`.
- Files: `src/database_read.cpp`, `tests/test_database_read.cpp`
- Risk: Regression in element ID ordering or return type could go unnoticed.
- Priority: Low

**C++ Tests: Issue 70 Not Covered:**
- What's not tested: The issue 70 regression schema in `tests/schemas/issues/issue70/` is exercised only by the Dart test (`bindings/dart/test/issues_test.dart`). `tests/test_issues.cpp` has no Issue70 test case.
- Files: `tests/test_issues.cpp`
- Risk: C++ core regression for this issue would not be caught.
- Priority: Medium

**C API Tests: No Coverage for `quiver_database_open`:**
- What's not tested: `quiver_database_open` (open existing database) is not tested in `tests/test_c_api_database_lifecycle.cpp`. Only `from_schema` and `from_migrations` lifecycle paths are covered.
- Files: `tests/test_c_api_database_lifecycle.cpp`
- Risk: A regression in the `open` path would not be caught by the C API test suite.
- Priority: Medium

**Python Tests: `read_vector_group_by_id` and `read_set_group_by_id` Have Minimal Coverage:**
- What's not tested: `read_vector_group_by_id` and `read_set_group_by_id` in `bindings/python/src/quiverdb/database.py` are tested only for the happy path and empty case in `bindings/python/tests/test_database_read_vector.py` and `test_database_read_set.py`. Multi-column groups and type coercion edge cases are not tested.
- Files: `bindings/python/tests/test_database_read_vector.py`, `bindings/python/tests/test_database_read_set.py`
- Risk: Multi-column group reads with mixed types could produce incorrect results silently.
- Priority: Low

**Julia Tests: No Regression for Time Dimension Initial Value Logic:**
- What's not tested: The `compute_time_dimension_initial_values` logic in `src/blob/blob_metadata.cpp` for `TimeFrequency::Hourly` with various parent frequencies is not tested for edge cases (e.g., initial datetime at midnight, leap year boundary, day-of-week boundary).
- Files: `bindings/julia/test/test_blob_metadata.jl`, `tests/test_blob_metadata.cpp`
- Risk: Silent off-by-one in initial hour values for non-daily parent frequencies.
- Priority: Medium

---

*Concerns audit: 2026-03-08*
