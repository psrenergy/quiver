# Codebase Concerns

**Analysis Date:** 2026-02-25

## Tech Debt

**Lua Integration Binding Complexity:**
- Issue: `src/lua_runner.cpp` (1184+ lines) contains repetitive, manual bindings for 100+ database methods using sol2 lambda wrappers. Each method family (read_scalar_*, read_vector_*, read_set_*, etc.) repeats similar conversion patterns.
- Files: `src/lua_runner.cpp`, `src/c/lua_runner.cpp`
- Impact: Difficult to maintain, error-prone when adding new C++ methods. Binding synchronization with C API requires manual duplication. High risk of inconsistency between binding and C++ API.
- Fix approach: Generate Lua bindings from C API using a template or macro system to avoid manual repetition. Consider using sol2's automatic binding features more extensively.

**Blob Subsystem is Entirely Unimplemented:**
- Issue: The entire `src/blob/` directory and `include/quiver/blob/` headers define `Blob`, `BlobCSV`, `BlobMetadata`, `Dimension`, `TimeProperties`, and `BlobTimeSeries` classes with full public APIs, but all implementation files are empty stubs. `blob.cpp` defines only an Impl struct skeleton. `blob_csv.cpp`, `blob_metadata.cpp`, `dimension.cpp`, and `time_properties.cpp` each contain only empty anonymous namespaces.
- Files: `src/blob/blob.cpp`, `src/blob/blob_csv.cpp`, `src/blob/blob_metadata.cpp`, `src/blob/dimension.cpp`, `src/blob/time_properties.cpp`, `include/quiver/blob/blob.h`, `include/quiver/blob/blob_csv.h`, `include/quiver/blob/blob_metadata.h`, `include/quiver/blob/dimension.h`, `include/quiver/blob/time_properties.h`
- Impact: Any code path that references `Blob`, `BlobCSV`, or `BlobMetadata` methods will link to dead code or fail. No tests exist for the blob subsystem. The `CLAUDE.md` does not document blob operations, indicating it is not yet part of the stable API. Headers expose methods that do not work.
- Fix approach: Either implement the blob subsystem completely or remove the headers and source stubs until implementation is ready. If removing, update `include/quiver/quiver.h` to omit blob includes.

**Vector/Set String Memory Management Asymmetry:**
- Issue: C API string vector/set read functions (`quiver_database_read_vector_strings`, `quiver_database_read_set_strings` in `src/c/database_read.cpp`) allocate nested 3D char arrays manually. The free function `quiver_database_free_string_vectors` assumes a specific allocation pattern.
- Files: `src/c/database_read.cpp`
- Impact: Risk of misaligned allocations/deallocations if allocation logic changes. Caller must track multiple levels of pointers (`char***`, `size_t*`). Easy to leak memory if free is called incorrectly or skipped.
- Fix approach: Provide a helper struct wrapping the 3D array with RAII semantics to manage cleanup automatically.

**Repetitive Error Message Patterns in Schema Validator:**
- Issue: `src/schema_validator.cpp` contains 15+ similar validation error messages following patterns like "Vector table 'X' must have..." repeated across `validate_vector_table()`, `validate_set_table()`, `validate_time_series_files_table()`.
- Files: `src/schema_validator.cpp`
- Impact: Difficult to update error messages globally. Minor inconsistencies in wording. Increases maintenance burden for schema changes.
- Fix approach: Extract validation error strings into named constants or a small error catalog. Use string formatting.

**C API Type Safety in Parameterized Queries:**
- Issue: `src/c/database_query.cpp` `convert_params()` uses `switch` on `int` parameter types without bounds checking. Caller can pass invalid type values.
- Files: `src/c/database_query.cpp`
- Impact: Invalid type enum values silently fall to a default case that throws at runtime with a generic message. No compile-time validation.
- Fix approach: Validate parameter types against enum range before processing; reject out-of-range types early with a clear error message.

**Schema Naming Rules Are Fragile String Matching:**
- Issue: `src/schema.cpp` `is_collection()`, `is_vector_table()`, `is_set_table()`, `is_time_series_table()` classify tables by substring search on table name. `is_collection()` uses `table.find('_') == std::string::npos`. Any table name with an underscore that is not a group table will be silently misclassified.
- Files: `src/schema.cpp` lines 89-115
- Impact: A user-named collection `My_Items` would be classified as a non-collection and the schema validator would reject it. The restriction is enforced by the validator, but the error message ("collection names cannot contain underscores") may surprise users who expect SQL naming freedom.
- Fix approach: Document the constraint clearly in schema conventions. The current behavior is intentional by design but should appear explicitly in CLAUDE.md schema rules.

**Python Bindings DatabaseOptions Not Exposed:**
- Issue: `bindings/python/src/quiverdb/database.py` `from_schema()` and `from_migrations()` hardcode default options (log level, read_only). There is no way for Python users to configure `DatabaseOptions` at the binding level — the options struct is constructed but `console_level` and `read_only` are always default values.
- Files: `bindings/python/src/quiverdb/database.py` lines 21-52
- Impact: Python callers cannot set log level to `OFF` during tests (unlike C++/Julia/Dart callers), resulting in console noise during Python test runs. Cannot open databases read-only from Python.
- Fix approach: Accept optional `console_level` and `read_only` parameters in `from_schema` and `from_migrations`, apply them to the options struct before calling the C API.

## Known Bugs

**Null Pointer Crash When Reading Vectors/Sets Without Schema:**
- Symptoms: Calling `read_vector_*`, `read_set_*`, `update_vector_*`, or `update_set_*` on a `Database` opened without a schema (no `from_schema`, no `from_migrations`) causes a null pointer dereference crash instead of throwing a proper `std::runtime_error`.
- Files: `src/database_read.cpp`, `src/database_update.cpp` — methods call `impl_->schema->find_vector_table()` / `impl_->schema->find_set_table()` where `impl_->schema` is null.
- Trigger: `Database db(":memory:"); db.read_vector_integers("X", "y");` — segfault.
- Workaround: Always use `from_schema()` or `from_migrations()`. Tests in `tests/test_database_errors.cpp` document this with comments like "Note: read_vector_* methods without schema cause segfault... These tests are skipped until the library adds proper null checks."
- Fix: Add `require_schema()` check at the top of every `read_vector_*`, `read_set_*`, `update_vector_*`, and `update_set_*` method (same guard used in `read_scalar_*` methods).

**Time Series Read Falls Back Silently on NULL Values:**
- Symptoms: In `src/database_time_series.cpp` `read_time_series_group()` (lines 94-107), value extraction tries `get_integer`, `get_float`, `get_string` in order. If none matches (e.g., actual SQL NULL), the row is stored with `nullptr`. This is correct, but integer-valued string columns (edge case: SQLite type affinity confusion) could extract as integer instead of string without warning.
- Files: `src/database_time_series.cpp` lines 94-107
- Trigger: Time series table with mixed SQLite type affinities.
- Workaround: Rely on strict schema enforcement to avoid mixed types in time series tables.

## Security Considerations

**Parameterized Query Parameters Not Type-Validated at Entry:**
- Risk: Caller passes mismatched type array (e.g., `param_types` says INTEGER but `param_values` contains `float*`). No validation that array lengths match before dereference.
- Files: `src/c/database_query.cpp`
- Current mitigation: Lua layer type-checks before calling C API; C layer trusts caller. No runtime bounds checking in C API for `param_types`/`param_values` arrays.
- Recommendations: Add explicit validation that `param_count` matches actual array lengths. Validate each `param_types[i]` is within valid enum range before accessing `param_values[i]`.

**SQL String Concatenation in Update and Create Operations:**
- Risk: `src/database_update.cpp` builds UPDATE SQL by concatenating column names directly without escaping. `src/database_create.cpp` does the same for INSERT SQL. If column names contain SQL keywords or special characters, could produce invalid SQL or parse failure.
- Files: `src/database_update.cpp` lines 28-41, `src/database_create.cpp` lines 27-42
- Current mitigation: Schema validator enforces `is_safe_identifier()` which restricts names to alphanumeric + underscore. Assumes schema is trusted.
- Recommendations: Current mitigation is sufficient given schema is trusted input. Document the `is_safe_identifier` guarantee explicitly.

**Thread-Local Error Storage Race Window:**
- Risk: `src/c/common.cpp` uses `thread_local std::string g_last_error`. In single-threaded environments this is correct, but if two operations fail on the same thread (e.g., inside a callback), the first error is overwritten before it can be read.
- Files: `src/c/common.cpp`
- Current mitigation: Each thread has its own storage, preventing cross-thread corruption.
- Recommendations: Document the single-error-per-thread limitation. Callers must read the error immediately after each failing call.

**Foreign Keys Disabled During CSV Import:**
- Risk: `src/database_csv.cpp` `import_csv()` (lines 501, 595, 827) disables foreign key enforcement (`PRAGMA foreign_keys = OFF`) during bulk import to allow delete+reinsertion. If the import process crashes mid-transaction, the database is left in an intermediate state with FK enforcement disabled.
- Files: `src/database_csv.cpp` lines 501-510, 595-710
- Current mitigation: FK enforcement is re-enabled in both success and catch paths. Transaction rollback would restore data consistency.
- Recommendations: PRAGMA setting is connection-scoped and not rolled back with transaction rollback in SQLite. Verify that the re-enable in catch paths is sufficient, and document this behavior.

## Performance Bottlenecks

**Repeated Schema Lookups in Batch Operations:**
- Problem: `src/database_update.cpp` calls `impl_->schema->find_all_tables_for_column()` for each attribute being updated. For Elements with many arrays, this results in N full schema scans.
- Files: `src/database_update.cpp`, `src/database_create.cpp` (similar pattern)
- Cause: No caching of schema state; each attribute triggers a fresh table scan over all tables.
- Improvement path: Cache schema query results for the operation scope, or pre-build a lookup map once per collection.

**Full DELETE + INSERT for Vector/Set/TimeSeries Updates:**
- Problem: `src/database_update.cpp` deletes all existing rows for an ID, then re-inserts. For large vectors, this causes unnecessary churn.
- Files: `src/database_update.cpp` lines 79, 109, 138
- Cause: Full replacement is simpler to implement than differential update.
- Improvement path: Implement differential update (insert new, delete old) or upsert strategy. Benchmark before committing.

**Vector/Set String Conversion Overhead in Lua:**
- Problem: `src/lua_runner.cpp` allocates Lua tables by iterating nested vectors incrementally. Each inner element creates a new Lua table entry without pre-sizing.
- Files: `src/lua_runner.cpp`
- Cause: No pre-allocation of Lua table size; incremental allocation triggers potential re-hashing.
- Improvement path: Use `lua.create_table(outer_size, inner_size)` with reserves to avoid re-hashing. Investigate sol2 batch insert APIs.

**Time Series Data Row-to-Column Marshaling:**
- Problem: `src/c/database_time_series.cpp` `quiver_database_read_time_series_group()` builds columnar arrays from row-oriented data with multiple allocations and per-element copies.
- Files: `src/c/database_time_series.cpp` lines 85-145
- Cause: C API expects columnar data while C++ layer uses row-oriented maps.
- Improvement path: Batch-allocate each column before filling, avoiding incremental append. Consider returning row-oriented data as an option.

**CSV Import Full-Scan Row Validation:**
- Problem: `src/database_csv.cpp` `import_csv()` iterates all rows twice: once for validation (lines 539-592), once for insertion (lines 615-664). For large CSVs, this doubles elapsed time.
- Files: `src/database_csv.cpp`
- Cause: Validation pass separated from insert pass for early error detection.
- Improvement path: Single-pass validation-and-insert with rollback on error, or validate a configurable sample only.

## Fragile Areas

**Schema Validator State Management:**
- Files: `src/schema_validator.cpp`
- Why fragile: Validator maintains `collections_` member across multiple validation phases. If validation order changes or new validation is added between existing checks, `collections_` state may be inconsistent.
- Safe modification: Add assertion that `collections_` is populated before each validation phase that relies on it. Add tests for table ordering edge cases.
- Test coverage: Schema validation has tests in `tests/test_schema_validator.cpp` and `tests/test_database_errors.cpp`, but edge cases around table ordering and multi-collection schemas are not exhaustively covered.

**TransactionGuard Ownership Logic:**
- Files: `src/database_impl.h` lines 178-206
- Why fragile: `owns_transaction_` flag tracks whether the guard should rollback on error. If exception handling changes in nested operations, the guard may incorrectly believe it owns the transaction when it does not (or vice versa).
- Safe modification: Add clear documentation of ownership semantics. Add tests for nested explicit + auto-transaction scenarios. Consider using `std::optional<bool>` to make ownership state explicit.
- Test coverage: `tests/test_database_transaction.cpp` covers basic cases, but nested transaction edge cases (explicit `begin_transaction()` while a TransactionGuard is active) need more test scenarios.

**Lua Type Conversion Boundary:**
- Files: `src/lua_runner.cpp`
- Why fragile: Manual sol2 bindings assume C++ `Value` types have consistent semantics. If `Value` type changes (e.g., adds new variant), all 100+ bindings must be reviewed and updated.
- Safe modification: Generate bindings or use sol2 reflection to auto-bind. Add compile-time static asserts checking `std::variant_size<Value>` against expected count.
- Test coverage: `tests/test_c_api_lua_runner.cpp` has basic Lua binding tests, but comprehensive type coverage across all read/write/metadata methods is missing.

**CSV Import Self-Referencing FK Resolution:**
- Files: `src/database_csv.cpp` lines 666-695
- Why fragile: Self-referencing FK columns are handled in a second pass after initial INSERT. The logic assumes `label` column is always present at `csv_col_index["label"]`, and that `self_label_to_id` lookup will always succeed for values just inserted. Any schema change to label semantics would break this silently.
- Safe modification: Add explicit error handling for missing label column in self-FK pass. Add a dedicated test for self-referencing FK import roundtrip.
- Test coverage: No test exists specifically for self-referencing FK import.

**Time Series Column Order Non-Determinism:**
- Files: `src/database_time_series.cpp` lines 68-73, `src/database_metadata.cpp` lines 44-49
- Why fragile: Time series row construction iterates over `table_def->columns` which is a `std::map<std::string, ColumnDefinition>` (alphabetically ordered). Dimension column is always placed first, but value columns come in alphabetical order. If tests or callers assume schema-definition order, they may encounter unexpected column ordering.
- Safe modification: Document that value columns are returned in alphabetical order. Tests should not depend on column index position.
- Test coverage: Time series read tests in `tests/test_database_time_series.cpp` use column name access (safe), but some C API tests in `tests/test_c_api_database_time_series.cpp` may assume positional column access.

## Scaling Limits

**In-Memory Logger Registration:**
- Current capacity: Logger counter is `std::atomic<uint64_t>` allowing ~2^64 databases before rollover.
- Limit: At scale, unique logger names avoid collisions but spdlog stores loggers in a global registry. Long-running processes creating many short-lived `Database` instances could accumulate loggers without cleanup.
- Scaling path: Register logger cleanup in `Impl::~Impl()` using `spdlog::drop(logger_name)` to prevent registry growth.

**Schema Object Memory:**
- Current capacity: Schema is parsed once at open and held in memory. Single schema per database instance.
- Limit: Very large schemas (1000+ tables) could cause memory pressure or parsing slowdown at open due to multiple PRAGMA queries per table.
- Scaling path: Lazy-load schema tables on demand, or support incremental schema updates without full re-parse.

**Time Series Columnar Array Allocation:**
- Current capacity: `quiver_database_read_time_series_group()` allocates one array per column in memory. For 100+ columns and 10k rows, ~1MB per call.
- Limit: Streaming time series data (millions of rows) will exhaust memory.
- Scaling path: Implement pagination/streaming API returning chunks of rows. Support external column stores.

**CSV Import Full In-Memory Load:**
- Current capacity: `import_csv()` reads the entire CSV into a `rapidcsv::Document` (in-memory string matrix) before processing.
- Limit: CSVs larger than available RAM will fail.
- Scaling path: Implement streaming CSV parser that processes rows in batches without loading the entire file.

## Dependencies at Risk

**sol2 (C++/Lua Binding):**
- Risk: sol2 is header-only with complex template metaprogramming. If Lua version changes (e.g., 5.4 to 5.5), sol2 compatibility must be verified. sol2 repo shows infrequent updates.
- Impact: Breaking changes in sol2 could require rewriting all Lua bindings.
- Migration plan: Monitor sol2 releases. Consider forking critical headers if upstream becomes unmaintained. Alternative: switch to SWIG or generated FFI approach.

**spdlog (Logging):**
- Risk: spdlog uses aggressive C++ features. Compiler version compatibility could be an issue as standards evolve.
- Impact: If the compiler drops C++17 support or spdlog adopts newer requirements, compilation may fail.
- Migration plan: Track spdlog releases. Have fallback to simpler logging (stdout/stderr) if spdlog becomes unavailable.

**rapidcsv (CSV Parsing):**
- Risk: rapidcsv is a single-header library with minimal maintenance activity.
- Impact: If CSV export/import requirements expand (e.g., custom delimiters beyond comma/semicolon, RFC 4180 edge cases), rapidcsv may not support them without modification.
- Migration plan: Audit rapidcsv features against known CSV edge cases. Consider implementing a custom writer/reader if export format requirements grow.

## Missing Critical Features

**No Automatic Index Creation on Foreign Keys:**
- Problem: Database creates tables but does not automatically index FK columns. Queries on relations or cascading deletes could be slow without user-managed indexes.
- Blocks: Performance optimization for large-scale applications with inter-collection references.
- Recommendation: Add automatic index creation for FK columns during schema application, or expose an index management API.

**No Explicit Query Result Streaming:**
- Problem: All read operations return full result sets in memory. No pagination, cursor, or streaming API exists.
- Blocks: Reading very large datasets (millions of rows) is impractical.
- Recommendation: Add cursor-based read API or streaming variant returning results in chunks.

**No Explicit Database Backup/Restore:**
- Problem: SQLite backup API (`sqlite3_backup_*`) is not exposed. Applications must manually copy .db files (unsafe on live databases).
- Blocks: Online backup capability for production deployments.
- Recommendation: Expose `sqlite3_backup_*` through the C API and bindings.

**Blob Subsystem API Present But Not Implemented:**
- Problem: Headers `include/quiver/blob/` define a full public API for binary blob storage, CSV blob import/export, and dimensional metadata, but no method bodies exist. No documentation describes intended blob semantics.
- Blocks: Any feature depending on binary data storage or time series file access beyond file path references.
- Recommendation: Either implement the blob subsystem or delete all blob headers and sources until implementation is ready. Dead API surface is misleading.

**Python Bindings Missing DatabaseOptions:**
- Problem: `bindings/python/src/quiverdb/database.py` has no way to configure `console_level` or `read_only` at the Python level. All Python databases open with default info-level logging.
- Blocks: Python tests cannot suppress log output. Read-only database access is unavailable from Python.
- Recommendation: Add optional `console_level` and `read_only` kwargs to `from_schema()` and `from_migrations()`.

## Test Coverage Gaps

**Read/Update Vector/Set Without Schema (Null Crash):**
- What's not tested: `read_vector_*`, `read_set_*`, `update_vector_*`, `update_set_*` without a loaded schema. Tests in `test_database_errors.cpp` explicitly skip these cases with comments documenting the segfault risk.
- Files: `tests/test_database_errors.cpp` lines 134-168 (commented out), `src/database_read.cpp`, `src/database_update.cpp`
- Risk: Any code path that creates a `Database` without schema and calls vector/set operations crashes the process.
- Priority: High

**Concurrent Database Access:**
- What's not tested: Multiple Database instances on same file, multi-threaded C API usage, race conditions in thread-local error storage.
- Files: `tests/test_c_api_*`, `tests/test_database_*`
- Risk: Concurrency bugs could silently corrupt data or lose errors.
- Priority: High

**Self-Referencing FK CSV Import Roundtrip:**
- What's not tested: Importing CSV with a self-referencing foreign key column (e.g., parent-child hierarchy in same collection).
- Files: `src/database_csv.cpp` lines 666-695, no corresponding test
- Risk: Self-referencing FK resolution in second-pass could fail silently or produce incorrect IDs.
- Priority: High

**CSV Export/Import Edge Cases:**
- What's not tested: Special characters in values (quotes, newlines, Unicode), very large vectors/sets export, mixed null/non-null values, semicolon-delimited CSV with no sep= header.
- Files: `tests/test_c_api_database_csv.cpp`, `tests/test_database_csv.cpp`
- Risk: CSV export could produce invalid or unparseable files.
- Priority: High

**Blob Subsystem:**
- What's not tested: The entire `src/blob/` and `include/quiver/blob/` subsystem has zero tests.
- Files: `src/blob/blob.cpp`, `src/blob/blob_csv.cpp`, `src/blob/blob_metadata.cpp`, `src/blob/dimension.cpp`, `src/blob/time_properties.cpp`
- Risk: All blob functionality is dead code; no tests would catch if behavior regresses (or is ever implemented).
- Priority: Medium (blocked by implementation)

**Time Series File Operations:**
- What's not tested: Reading/writing time series file paths, partial update of file columns, file path consistency with element lifecycle.
- Files: No dedicated time series files test exists.
- Risk: File path references could become stale or inconsistent with data.
- Priority: Medium

**Lua Type Boundary Conversions:**
- What's not tested: Edge cases in converting Lua table structures back to C++ types (malformed tables, missing keys, type mismatches in array elements).
- Files: `tests/test_c_api_lua_runner.cpp` basic coverage only.
- Risk: Segfault or undefined behavior from type confusion in sol2 conversions.
- Priority: Medium

**Transaction Rollback Under Partial Failure:**
- What's not tested: Rollback during partial vector update (e.g., some rows inserted before constraint violation), rollback with cascading FK operations, nested rollback scenarios.
- Files: `tests/test_database_transaction.cpp`
- Risk: Data inconsistency if rollback partially fails.
- Priority: Medium

---

*Concerns audit: 2026-02-25*
