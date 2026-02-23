# Codebase Concerns

**Analysis Date:** 2026-02-23

## Tech Debt

**Lua Integration Binding Complexity:**
- Issue: `src/lua_runner.cpp` (1184 lines) contains repetitive, manual bindings for 100+ database methods using sol2 lambda wrappers. Each method family (read_scalar_*, read_vector_*, read_set_*, etc.) repeats similar conversion patterns.
- Files: `src/lua_runner.cpp`, `src/c/lua_runner.cpp`
- Impact: Difficult to maintain, error-prone when adding new C++ methods. Binding synchronization with C API requires manual duplication. High risk of inconsistency between binding and C++ API.
- Fix approach: Generate Lua bindings from C API using a template or macro system to avoid manual repetition. Consider using sol2's automatic binding features more extensively or generate bindings from schema.

**Vector/Set String Memory Management Asymmetry:**
- Issue: C API string vector/set read functions (`quiver_database_read_vector_strings`, `quiver_database_read_set_strings` in `src/c/database_read.cpp`) allocate nested 3D char arrays manually. Free function `quiver_database_free_string_vectors` assumes specific allocation pattern.
- Files: `src/c/database_read.cpp` lines 114-180, 216-252
- Impact: Risk of misaligned allocations/deallocations if allocation logic changes. Caller must track multiple levels of pointers (char***, size_t*). Easy to leak memory if free is called incorrectly or skipped.
- Fix approach: Provide a helper struct wrapping the 3D array with RAII semantics to manage cleanup automatically.

**Repetitive Error Message Patterns in Schema Validator:**
- Issue: `src/schema_validator.cpp` contains 15+ similar validation error messages following patterns like "Vector table 'X' must have..." repeated across `validate_vector_table()`, `validate_set_table()`, `validate_time_series_files_table()`.
- Files: `src/schema_validator.cpp` lines 103-222
- Impact: Difficult to update error messages globally. Minor inconsistencies in wording. Increases maintenance burden for schema changes.
- Fix approach: Extract validation error strings into named constants or an error catalog. Use string formatting.

**C API Type Safety in Parameterized Queries:**
- Issue: `src/c/database_query.cpp` `convert_params()` function uses `switch` on `int` parameter types without bounds checking. Caller can pass invalid type values.
- Files: `src/c/database_query.cpp` lines 11-35
- Impact: Invalid type enum values silently coerce to default case ("Unknown parameter type"), then throw at runtime. No compile-time validation.
- Fix approach: Validate parameter types against enum range before processing; reject out-of-range types early with clear error message.

## Known Bugs

**No known open bugs reported in code.**

## Security Considerations

**Parameterized Query Parameters Not Type-Validated at Entry:**
- Risk: Caller passes mismatched type array (e.g., param_types says INTEGER but param_values contains float*). No validation that array lengths match before dereference.
- Files: `src/c/database_query.cpp` lines 108-195
- Current mitigation: sol2/Lua layer type checks before calling C API; C layer trusts caller. No runtime bounds checking in C API for param_types/param_values arrays.
- Recommendations: Add explicit validation that param_count matches actual array lengths. Validate each param_types[i] is within valid enum range before accessing param_values[i].

**SQL String Concatenation in Update Operations:**
- Risk: `src/database_update.cpp` lines 28-41 build UPDATE SQL by concatenating column names directly without escaping. If column names contain SQL keywords or special characters, could produce invalid SQL (not injection, but parse failure).
- Files: `src/database_update.cpp` lines 28-41, `src/database_create.cpp` similar pattern
- Current mitigation: Schema validator ensures table/column names are valid identifiers. Assumes schema is trusted.
- Recommendations: Use parameterized SQL even for column names where possible, or enforce stricter validation on schema column names.

**Thread-Local Error Storage:**
- Risk: `src/c/common.cpp` uses `thread_local std::string g_last_error` to store error messages. In multi-threaded environments where multiple database instances run in parallel, error messages could be overwritten or misread.
- Files: `src/c/common.cpp`
- Current mitigation: Each thread has its own error storage, but if two calls race within same thread, one error is lost.
- Recommendations: Use atomic or mutex-protected error stack per thread, or return errors inline in C API (breaking change). Document thread-safety limitations clearly.

## Performance Bottlenecks

**Repeated Schema Lookups in Batch Operations:**
- Problem: `src/database_update.cpp` lines 51-72 call `impl_->schema->find_all_tables_for_column()` for each attribute being updated. For large Elements with many arrays, this results in N schema scans.
- Files: `src/database_update.cpp`, `src/database_create.cpp` (similar pattern)
- Cause: No caching of schema state; each attribute triggers fresh table lookup.
- Improvement path: Cache schema query results for the operation scope, or pre-build a lookup map once per collection.

**Vector/Set String Conversion Overhead:**
- Problem: `src/lua_runner.cpp` lines 114-150 (and similar for sets) allocate Lua tables by iterating nested vectors twice: once to create outer table, once to populate. Each inner element creates new Lua table.
- Files: `src/lua_runner.cpp` lines 114-150, 1019-1034
- Cause: No pre-allocation of Lua table size; incremental allocation for each nested table.
- Improvement path: Use `lua.create_table(outer_size, inner_size)` with reserves to avoid re-hashing. Investigate sol2 batch insert APIs.

**Full DELETE + INSERT for Vector/Set Updates:**
- Problem: `src/database_update.cpp` lines 75-120 delete all existing vector rows for an ID, then re-insert. For large vectors, this causes churn.
- Files: `src/database_update.cpp` lines 76, 112-116
- Cause: Simpler to implement full replacement than differential update.
- Improvement path: Implement differential update (insert new, delete old) or upsert strategy. Benchmark before committing.

**Time Series Data Marshaling Complexity:**
- Problem: `src/c/database_time_series.cpp` lines 85-145 builds columnar arrays from row-oriented data. Multiple allocations and per-element copies.
- Files: `src/c/database_time_series.cpp` lines 85-145
- Cause: C API expects columnar (array of arrays) while C++ layer works with rows.
- Improvement path: Optimize memory layout conversion; consider returning row-oriented data as option, or batch allocate entire column before filling.

## Fragile Areas

**Schema Validator State Management:**
- Files: `src/schema_validator.cpp`
- Why fragile: Validator maintains `collections_` member across multiple validation phases. If validation order changes or new validation is added between existing checks, `collections_` state may be inconsistent.
- Safe modification: Add validation that `collections_` is properly initialized before each validation phase. Add assertions on state invariants.
- Test coverage: Schema validation has dedicated tests in `tests/test_database_errors.cpp`, but edge cases around table ordering and rename scenarios not covered.

**TransactionGuard Ownership Logic:**
- Files: `src/database_impl.h` lines 99-125
- Why fragile: `owns_transaction_` flag tracks whether guard should rollback on error. If exception handling changes in nested operations, guard may incorrectly believe it owns the transaction when it doesn't.
- Safe modification: Add clear documentation of ownership semantics. Add tests for nested explicit + auto-transaction scenarios. Consider using `std::optional<bool>` to make ownership explicit.
- Test coverage: `tests/test_database_transaction.cpp` covers basic cases, but nested transaction edge cases (explicit begin while guard active) need more scenarios.

**Lua Type Conversion Boundary:**
- Files: `src/lua_runner.cpp` (entire file)
- Why fragile: Manual sol2 bindings assume C++ Value types have consistent semantics. If Value type changes (e.g., adds new variant), all 100+ bindings must be reviewed.
- Safe modification: Generate bindings or use sol2 reflection to auto-bind. Add compile-time checks for Value type compatibility.
- Test coverage: `tests/test_c_api_lua_runner.cpp` has basic Lua binding tests, but comprehensive type coverage across all read/write/metadata methods missing.

**CSV Export Option Enum Conversions:**
- Files: `src/c/database_csv.cpp`, `src/database_csv.cpp`
- Why fragile: CSV export options use C API enums (DateFormatSpec, EnumFormatSpec) that must be synchronized between C API headers and internal converters. Adding new enum values requires updates in multiple places.
- Safe modification: Add generator script to emit enum converters from single source. Add compile-time enum count validation.
- Test coverage: `tests/test_c_api_database_csv.cpp` covers enum labels and formatting, but edge cases around invalid option combinations not tested.

## Scaling Limits

**In-Memory Logger Registration:**
- Current capacity: Logger counter is `std::atomic<uint64_t>` allowing ~2^64 databases before rollover.
- Limit: At scale, unique logger names avoid collisions but leaking loggers (not cleaned up) could accumulate unbounded.
- Scaling path: Implement logger cleanup when Database is destroyed. Track active loggers and reuse freed names.

**Schema Object Memory:**
- Current capacity: Schema is parsed once at open and held in memory. Assumes single schema per database.
- Limit: Very large schemas (1000+ tables) could cause memory pressure or parsing slowdown during open.
- Scaling path: Lazy-load schema tables on demand, or support incremental schema updates without full re-parse.

**Time Series Columnar Array Allocation:**
- Current capacity: `quiver_database_read_time_series_group()` allocates one array per column. For 100+ columns and 10k rows, ~1MB per read.
- Limit: Streaming time series data (millions of rows) will exhaust memory.
- Scaling path: Implement pagination/streaming API returning chunks of rows. Support external column stores (parquet, HDF5).

## Dependencies at Risk

**sol2 (C++/Lua Binding):**
- Risk: sol2 is a header-only library with complex template metaprogramming. If Lua version changes (e.g., 5.4 to 5.5), sol2 compatibility must be verified. sol2 repo shows infrequent updates.
- Impact: Breaking changes in sol2 could require rewriting Lua bindings. Lua API changes could break bindings.
- Migration plan: Monitor sol2 releases. Consider forking critical headers if upstream becomes unmaintained. Alternative: switch to SWIG or pybind11-style generation.

**spdlog (Logging):**
- Risk: spdlog is actively maintained but uses aggressive C++ features. Compiler version compatibility could be an issue.
- Impact: If compiler drops C++17 support, spdlog may not compile.
- Migration plan: Track spdlog releases. Have fallback to simpler logging (stdout/stderr) if spdlog becomes unavailable.

**rapidcsv (CSV Parsing):**
- Risk: rapidcsv is header-only, smaller ecosystem. Minimal maintenance activity.
- Impact: If CSV export requirements expand (e.g., custom delimiters, quote handling), rapidcsv may not support it.
- Migration plan: Audit rapidcsv features against known CSV edge cases. Consider implementing custom CSV writer if export format becomes complex.

## Missing Critical Features

**No Automatic Index Creation:**
- Problem: Database creates tables but does not automatically index foreign keys. Queries on relations or cascading deletes could be slow.
- Blocks: Performance optimization for large-scale applications.
- Recommendation: Add automatic index creation for FK columns during schema application, or expose index management API.

**No Explicit Query Result Streaming:**
- Problem: All read operations return full result sets in memory. No pagination, cursor, or streaming API.
- Blocks: Reading very large datasets (millions of rows) becomes impractical.
- Recommendation: Add cursor-based read API or streaming variant returning results in chunks.

**No Explicit Database Backup/Restore:**
- Problem: SQLite backup API is not exposed. Applications must manually copy .db files.
- Blocks: Online backup capability for production deployments.
- Recommendation: Expose `sqlite3_backup_*` API through C API and bindings.

**Python Bindings Incomplete:**
- Problem: `bindings/python/` exists but appears minimal/abandoned (only stub pyproject.toml, no actual binding code).
- Blocks: Python users cannot use Quiver.
- Recommendation: Complete Python bindings using ctypes or CFFI. Consider auto-generating from C API like Julia/Dart.

## Test Coverage Gaps

**Concurrent Database Access Not Tested:**
- What's not tested: Multiple Database instances on same file, multi-threaded C API usage, race conditions in thread-local error storage.
- Files: `tests/test_c_api_*` and `tests/test_database_*`
- Risk: Concurrency bugs could silently corrupt data or lose errors.
- Priority: High

**CSV Export Edge Cases:**
- What's not tested: Special characters in scalar/group values (quotes, newlines, unicode), very large vectors/sets export, mixed null/non-null values.
- Files: `tests/test_c_api_database_csv.cpp`, `tests/test_database_csv.cpp`
- Risk: CSV export could produce invalid/unparseable files.
- Priority: High

**Time Series File Operations:**
- What's not tested: Reading/writing time series file paths, file cleanup on element delete, concurrent file access.
- Files: Tests sparse; no dedicated time series files test.
- Risk: File leaks or incorrect file references.
- Priority: Medium

**Lua Type Boundary Conversions:**
- What's not tested: Edge cases in converting Lua table structures back to C++ types (malformed tables, missing keys, type mismatches).
- Files: `tests/test_c_api_lua_runner.cpp` basic coverage only.
- Risk: Segfault or undefined behavior from type confusion.
- Priority: Medium

**Transaction Rollback Error Scenarios:**
- What's not tested: Rollback during partial operation (e.g., vector update half-complete), rollback with constraint violations, nested rollback scenarios.
- Files: `tests/test_database_transaction.cpp`
- Risk: Data inconsistency if rollback fails silently.
- Priority: Medium

**Schema Validation Comprehensive:**
- What's not tested: Complex foreign key chains, circular references (if allowed), large schemas with 100+ tables, schema upgrade/downgrade paths.
- Files: `tests/test_database_errors.cpp` covers validation but not exhaustively.
- Risk: Invalid schemas accepted or valid schemas rejected incorrectly.
- Priority: Low

---

*Concerns audit: 2026-02-23*
