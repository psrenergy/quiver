# Codebase Concerns

**Analysis Date:** 2026-02-26

## Tech Debt

**Lua binding is entirely hand-written with no generation:**
- Issue: `src/lua_runner.cpp` is 990 lines of manual sol2 lambda wrappers covering every database method. Each new C++ API method requires a parallel Lua binding added by hand. The pattern for every scalar/vector/set read method is nearly identical boilerplate.
- Files: `src/lua_runner.cpp`
- Impact: High synchronization risk between C++ API and Lua bindings. When a method is added to `Database`, there is no tooling to detect that the Lua binding is missing. The file will keep growing linearly with API surface.
- Fix approach: Generate Lua bindings from C API like Julia/Dart/Python generators do (`bindings/julia/generator/generator.bat`). The repetitive patterns are machine-derivable.

**Lua empty array in `table_to_element` is silently dropped:**
- Issue: In `src/lua_runner.cpp` line 253, `table_to_element` skips arrays where `arr.size() == 0`. An empty `{}` Lua table for a vector attribute is silently not set on the element rather than setting an empty vector.
- Files: `src/lua_runner.cpp` lines 252-274
- Impact: Lua callers cannot clear a vector to empty via `create_element` or `update_element` — the field is simply ignored. This is a behavioral inconsistency versus the C++ API which can accept an empty `std::vector`.
- Fix approach: Detect empty arrays and call `element.set(k, std::vector<...>{})`. Requires inferring the target column type from schema since the first element is not available for type detection.

**CSV import silently drops self-referencing FK values:**
- Issue: In `src/database_csv_import.cpp` lines 403-407, self-referencing foreign key columns are set to `nullptr` (SQL NULL) during the first insert pass, then resolved in a second pass (lines 441-469). If the second pass update fails or is skipped, the NULL value persists with no error.
- Files: `src/database_csv_import.cpp` lines 403-407, 441-469
- Impact: A self-FK import failure may produce rows with NULL parent references instead of the correct ID, violating data integrity without a clear error.
- Fix approach: Validate that the second-pass UPDATE successfully sets a non-NULL value for all rows where the CSV cell was non-empty. Throw if any UPDATE produces 0 affected rows.

**`is_date_time_column` convention is implicit and undocumented in schema:**
- Issue: DateTime detection uses `col.name.starts_with("date_")` as defined in `include/quiver/data_type.h` line 39. This convention is applied in `src/schema.cpp` line 356, `src/database_internal.h` line 72, and `src/database_csv_import.cpp` lines 341, 593, 648. There is no schema-level `DATE_TIME` column type — it is entirely inferred from naming.
- Files: `include/quiver/data_type.h`, `src/schema.cpp`, `src/database_internal.h`, `src/database_csv_import.cpp`
- Impact: A column named `date_of_birth` or `date_created` will be treated as DateTime even if the developer intends it as plain TEXT. This is a silent semantic imposition that can cause unexpected datetime parsing errors during CSV import.
- Fix approach: Add explicit `DATE_TIME` as a storable SQLite column type alias (e.g., using `TEXT` affinity with a `CHECK` or a column comment). Or document the `date_` prefix convention prominently and add schema validator enforcement.

**Python `_c_api.py` is hand-maintained and diverges from generator output:**
- Issue: `bindings/python/src/quiverdb/_c_api.py` (330 lines, 96 function declarations) is hand-written. `bindings/python/src/quiverdb/_declarations.py` (507 lines, 99 function declarations) is generated output used only as reference. These two files can and do diverge when the C API changes.
- Files: `bindings/python/src/quiverdb/_c_api.py`, `bindings/python/src/quiverdb/_declarations.py`
- Impact: The Python binding silently uses stale CFFI declarations. Any new C API function added since the last manual sync is unavailable to Python callers without error at import time.
- Fix approach: Use `_declarations.py` as the actual CFFI source of truth, or add a CI step that diffs the two files to detect drift.

**`data_type_from_string` throws on unknown SQLite column types:**
- Issue: `include/quiver/data_type.h` line 20 throws `std::runtime_error("Unknown data type: " + type_str)` for any column type that is not exactly `INTEGER`, `REAL`, `TEXT`, or `DATE_TIME`. SQLite allows many type affinities (e.g., `VARCHAR`, `NUMERIC`, `BLOB`, `BOOLEAN`). Opening an existing SQLite database with non-standard column types will fail entirely.
- Files: `include/quiver/data_type.h` lines 11-21, `src/schema.cpp` line 348
- Impact: Third-party databases or databases using SQLite type affinities for compatibility cannot be opened via Quiver at all. The error appears as a schema load failure rather than a column-level warning.
- Fix approach: Fall back to `DataType::Text` for unrecognised types and log a warning, rather than throwing. Optionally add `NUMERIC` -> `Real` and `VARCHAR` -> `Text` mappings.

## Known Bugs

**`quiver_database_free_time_series_files` will crash on NULL path entries:**
- Symptoms: Calling `quiver_database_free_time_series_files` after a `read_time_series_files` where some columns have NULL paths (optional values stored as `nullptr`) causes a crash.
- Files: `src/c/database_time_series.cpp` lines 447-460
- Trigger: `read_time_series_files` stores `nullptr` for optional path entries (line 405). `free_time_series_files` calls `delete[] paths[i]` unconditionally for all `i` without checking for NULL.
- Workaround: None at API level. Callers must not call `free_time_series_files` if any path was NULL (inconsistent with the "free what you receive" contract).

**Null values in time series numeric columns are silently converted to zero:**
- Symptoms: When `quiver_database_read_time_series_group` processes a row where a numeric column value is a null `Value` variant (i.e., `std::nullptr_t`), line 133 sets `arr[r] = 0` (integer) or line 147 sets `arr[r] = 0.0` (float). The caller has no way to distinguish a stored `0` from a missing value.
- Files: `src/c/database_time_series.cpp` lines 125-148
- Trigger: Any time series group where a value column contains SQL NULL.
- Workaround: Avoid NULL values in time series numeric columns. The C++ API itself returns `Value` which holds `std::nullptr_t`, so the information is present before the C API layer loses it.

**Issue 70: Configuration table without `label` column fails migrations silently (Dart only tested):**
- Symptoms: A migration schema where `Configuration` lacks the `label` column (as in `tests/schemas/issues/issue70/`) is tested only in `bindings/dart/test/issues_test.dart`. The C++ regression test `tests/test_issues.cpp` only covers issue 52. If the issue recurs in other bindings, it would be undetected.
- Files: `tests/test_issues.cpp`, `bindings/dart/test/issues_test.dart`
- Trigger: Running `from_migrations` with the issue70 schema in C++, Julia, or Python.

## Security Considerations

**Column and attribute names concatenated directly into SQL strings:**
- Risk: Functions like `read_scalar_integers` (`src/database_read.cpp` line 9) build SQL as `"SELECT " + attribute + " FROM " + collection`. The `require_collection` and `require_column` checks verify the names exist in the schema, but they do not validate that the names are safe SQL identifiers. A schema column with a name like `value; DROP TABLE--` (crafted via a malicious schema file) would produce valid-looking SQL that executes injected statements.
- Files: `src/database_read.cpp` lines 9, 26, 43; `src/database_create.cpp` lines 27-44; `src/database_update.cpp` lines 28-79
- Current mitigation: Schema validation (`src/schema_validator.cpp`) enforces collection names without underscores, but no `is_safe_identifier` check runs on column names or attribute names at query time. `src/schema.cpp` line 11 has `is_safe_identifier` used only for PRAGMA queries.
- Recommendations: Call `is_safe_identifier` on collection and attribute/column name arguments before constructing any SQL string. This is a defence-in-depth measure since schema is expected to be trusted, but it costs nothing.

**Thread-local error store means error from concurrent operations on same thread can be lost:**
- Risk: `src/c/common.cpp` line 8 stores `thread_local std::string g_last_error`. In async or coroutine contexts where multiple C API calls interleave on the same OS thread, `quiver_get_last_error()` returns the most recent error, not the error for a specific call. The prior error is silently overwritten.
- Files: `src/c/common.cpp`
- Current mitigation: Thread-local storage is correct for true multi-threaded parallel use. The risk is specific to coroutines/async schedulers (e.g., Dart's event loop, Python asyncio).
- Recommendations: Document this limitation in the C API header. Provide a pattern for users to save/restore error state when interleaving calls.

## Performance Bottlenecks

**Schema loaded entirely on every `migrate_up` and `apply_schema` call:**
- Problem: `load_schema_metadata()` (`src/database_impl.h` lines 127-132) runs `Schema::from_database(db)` which queries `sqlite_master`, then `PRAGMA table_info`, `PRAGMA foreign_key_list`, and `PRAGMA index_list` for every table. This runs on every `from_schema`, `from_migrations`, and at the end of each `migrate_up` call.
- Files: `src/database_impl.h` lines 127-132, `src/schema.cpp` lines 293-326
- Cause: No incremental schema update — full re-parse every time.
- Improvement path: For migrations, build schema incrementally by parsing only the newly applied SQL. For `from_schema`, parse once at open and cache.

**All vector/set read operations fetch full result set into memory:**
- Problem: `read_vector_integers`, `read_vector_floats`, `read_vector_strings`, and all set variants in `src/database_read.cpp` execute a SELECT and load all rows into `std::vector<Row>` in `Database::execute()`. There is no streaming or pagination.
- Files: `src/database_read.cpp` lines 96-174, `src/database.cpp` lines 131-212
- Cause: `execute()` collects all `SQLITE_ROW` results before returning. No cursor-based iteration.
- Improvement path: Add a cursor/iterator API to `Database::execute()` or expose a streaming variant. For the current API, document the memory constraint.

**Full delete-then-reinsert for every vector/set update:**
- Problem: `update_element` in `src/database_update.cpp` lines 76-138 issues `DELETE FROM <table> WHERE id = ?` for every vector/set group, then reinserts all values. This scans and rewrites the entire group even for single-element changes.
- Files: `src/database_update.cpp` lines 76-138
- Cause: Simpler to implement full replacement. The PK structure (id, vector_index) would allow upsert but requires tracking which rows to remove.
- Improvement path: Use INSERT OR REPLACE with a stable PK, or DELETE only rows with indices exceeding the new length and UPSERT the rest.

**`string::trim` applied unconditionally to every string parameter in `execute()`:**
- Problem: `src/database.cpp` line 154 calls `string::trim(arg)` on every string parameter passed to `execute()`. This creates a new `std::string` allocation per string parameter, regardless of whether trimming is needed.
- Files: `src/database.cpp` line 154
- Cause: Defensive trimming for all strings. Likely introduced for CSV import safety.
- Improvement path: Trim only in CSV import paths, not in the core `execute()` function. This avoids silently modifying user-provided string values (a user who intentionally stores a value with leading/trailing whitespace will have it stripped).

## Fragile Areas

**`read_grouped_values_all` depends on consistent ID ordering across result sets:**
- Files: `src/database_internal.h` lines 29-51
- Why fragile: The template groups rows by consecutive equal Ids. If any query returns rows in non-ID-consecutive order (e.g., due to a missing `ORDER BY id`), groups are split incorrectly producing duplicate entries. The `read_vector_*` queries use `ORDER BY id, vector_index` but `read_set_*` queries use only `ORDER BY id` (no secondary sort) — set values within each group have non-deterministic order.
- Safe modification: Add `ORDER BY id, rowid` or a stable column to all set queries. Add an assertion or comment documenting the ID-consecutive dependency.
- Test coverage: Tests exercise correctness of returned values but not ordering stability.

**CSV import disables foreign keys globally during import:**
- Files: `src/database_csv_import.cpp` lines 276, 370, 602
- Why fragile: `PRAGMA foreign_keys = OFF` is a connection-level setting. If another operation runs concurrently on the same database connection between `PRAGMA foreign_keys = OFF` and `PRAGMA foreign_keys = ON`, FK constraints are silently unenforced for that operation too. The `try/catch` that restores `PRAGMA foreign_keys = ON` is correct but any exception thrown inside `execute_raw("PRAGMA foreign_keys = ON")` itself would leave FKs disabled permanently for the connection.
- Safe modification: Use SQLite's `PRAGMA defer_foreign_keys = ON` or savepoint-based FK deferral instead of disabling FKs globally. Add a RAII guard that restores FK state in its destructor.
- Test coverage: FK disabled/enabled state is not directly tested; only the resulting data is checked.

**`quiver_database_open` returns a usable handle with no schema loaded:**
- Files: `src/c/database.cpp` lines 14-34, `src/database_impl.h` line 24 (`schema` member starts as `nullptr`)
- Why fragile: Calling `quiver_database_open` opens a database without applying or loading any schema. `impl_->schema` is `nullptr`. Any operation that calls `require_schema` will throw with "no schema loaded", but operations that skip that check (or where `require_collection` is not called) may dereference a null schema. The Python and Julia bindings do not expose `quiver_database_open` to users, but Dart's `bindings.dart` includes the binding.
- Safe modification: Document clearly that `quiver_database_open` is for bare SQLite access only. Add a guard in all schema-dependent paths that produces a clear error rather than a null dereference.
- Test coverage: `test_c_api_database_lifecycle.cpp` tests `quiver_database_open` but likely does not test schema-dependent operations on the resulting handle.

**Lua `transaction` wrapper swallows rollback errors:**
- Files: `src/lua_runner.cpp` lines 53-68
- Why fragile: The `transaction` implementation catches a Lua error, calls `self.rollback()` inside a bare `catch (...)` that discards any rollback exception, then rethrows the Lua error. If rollback fails (e.g., because the transaction was already committed by the user before the error), the database may be in an inconsistent state but only the Lua script error is surfaced.
- Safe modification: Log rollback failures even when swallowing the exception. Use the same logger instance available elsewhere in `Impl`.
- Test coverage: No test verifies the rollback failure code path in Lua transactions.

## Scaling Limits

**spdlog logger registry grows unbounded for long-lived processes:**
- Current capacity: Each `Database` instance creates a uniquely named logger via `g_logger_counter` (`src/database.cpp` line 47). Loggers are registered in spdlog's global registry.
- Limit: spdlog's default registry does not remove loggers automatically. In a process that repeatedly opens and closes Database instances, the logger registry accumulates entries indefinitely.
- Scaling path: Call `spdlog::drop(logger_name)` in `Database::Impl::~Impl()` to deregister the logger when the database closes.

**Time series columnar read allocates one heap array per column per call:**
- Current capacity: `quiver_database_read_time_series_group` allocates `col_count` separate heap arrays. Each string column allocates `row_count` individual `char*` strings.
- Limit: For 10 columns and 100,000 rows, this is 1,000,000+ individual allocations per read call. Heap fragmentation compounds over repeated calls.
- Scaling path: Use a single arena allocation per call. Pre-compute total string byte count and allocate one contiguous buffer, then assign pointers into it. This converts 1M allocations to 1.

## Dependencies at Risk

**`rapidcsv` is a minimal header-only library with limited active development:**
- Risk: `rapidcsv` has no streaming support, limited delimiter handling, and no RFC 4180 quoting guarantees. The current CSV import in `src/database_csv_import.cpp` works around limitations by pre-processing the content string (lines 41-105) before passing to rapidcsv.
- Impact: CSV files with quoted delimiters inside values, multi-line quoted fields, or embedded newlines may parse incorrectly or crash.
- Migration plan: Replace rapidcsv with a more robust library (e.g., `csv2`, `fast-cpp-csv-parser`) or implement a direct RFC 4180 parser. The pre-processing workaround layer makes future migration easier.

**`sol2` Lua binding requires C++17 and specific Lua versions:**
- Risk: `sol2` is deeply template-heavy and version-sensitive to both Lua and the C++ standard. Upgrading from Lua 5.4 to a hypothetical 5.5 would require auditing all bindings. sol2 major releases (2.x vs 3.x) had breaking API changes.
- Impact: `src/lua_runner.cpp` (990 lines) would need significant rework on any sol2 major version change.
- Migration plan: Pin sol2 version in CMakeLists.txt with explicit version comment. Test against Lua 5.4 specifically. Have a rollback plan before upgrading.

## Missing Critical Features

**No `quiver_database_open` counterpart in Python or Julia (bare open without schema):**
- Problem: Python and Julia bindings do not expose the `quiver_database_open` function for opening an existing database without re-applying a schema. Users who have an existing Quiver database and just want to open it (not create it) must use `from_schema` or `from_migrations` even when the schema is already applied.
- Blocks: Adopters with existing databases who want lightweight re-opens.

**No multi-column vector/set CSV export:**
- Problem: `src/database_csv_export.cpp` generates one CSV file per group, and the SELECT for group export (`lines 245-250`) selects a single `value` column. Multi-column vector/set groups (where `value_columns` has more than one entry) may not export all columns correctly.
- Files: `src/database_csv_export.cpp` lines 185-252
- Blocks: Round-trip CSV export/import for multi-column group schemas.

**No Lua runner in Python bindings:**
- Problem: `bindings/python/src/quiverdb/` has no `lua_runner.py` and `_c_api.py` does not declare `quiver_lua_runner_*` functions. Julia and Dart both expose `LuaRunner`.
- Files: `bindings/python/src/quiverdb/_c_api.py`, `bindings/python/src/quiverdb/database.py`
- Blocks: Python users cannot run Lua scripts against Quiver databases.

## Test Coverage Gaps

**Issue 70 has no C++ or Julia regression test:**
- What's not tested: The `tests/schemas/issues/issue70/` migration (Configuration table without `label` during migrate_up) is only tested in `bindings/dart/test/issues_test.dart`.
- Files: `tests/test_issues.cpp`, `bindings/julia/test/` (no issues test file)
- Risk: The regression could reappear in Julia or the C++ core without detection.
- Priority: Medium

**Multi-column vector/set group operations largely untested:**
- What's not tested: Creating, reading, and updating vector or set groups that have more than one `value_column`. The standard test schemas use single-column groups.
- Files: `tests/test_database_read.cpp`, `tests/test_database_update.cpp`, `tests/test_database_create.cpp`
- Risk: Silent schema mis-routing or incorrect data on multi-column groups.
- Priority: Medium

**`quiver_database_free_time_series_files` null-path crash is untested:**
- What's not tested: Calling `free_time_series_files` when some paths are NULL (i.e., the collection has no path set for a column).
- Files: `src/c/database_time_series.cpp`, `tests/test_c_api_database_time_series.cpp`
- Risk: Crash in production when time series file columns have NULL paths.
- Priority: High

**String trimming side-effect in `execute()` has no explicit test:**
- What's not tested: That storing a string with intentional leading/trailing whitespace via any API call and reading it back returns the original value unchanged.
- Files: `src/database.cpp` line 154
- Risk: Silent data corruption for whitespace-significant string values.
- Priority: Medium

**Lua binding tests do not cover all method groups:**
- What's not tested: Lua bindings for `list_time_series_files_columns`, `read_time_series_files`, `update_time_series_files`, `export_csv`, `import_csv`, and metadata list methods are present in `src/lua_runner.cpp` but not covered in `tests/test_lua_runner.cpp` or `tests/test_c_api_lua_runner.cpp`.
- Files: `tests/test_lua_runner.cpp`, `src/lua_runner.cpp` lines 934-971
- Risk: Lua binding bugs in these methods go undetected until production use.
- Priority: Medium

---

*Concerns audit: 2026-02-26*
