# Codebase Concerns

**Analysis Date:** 2026-02-09

## Tech Debt

**Blob Module Incomplete Implementation:**
- Issue: The blob module contains stubbed/empty function bodies that serve as placeholders
- Files: `src/blob/blob.cpp` (lines 7-9)
- Impact: Functions `open_reader()` and `open_writer()` are declared but have no implementation, making blob file I/O non-functional. This blocks time series file operations that may depend on blob reading/writing
- Fix approach: Implement the blob I/O functions properly or remove them if they're not being used. The deprecation comment indicates these were previously removed and should be inlined elsewhere - verify if they're actually needed

**Deprecated Functions Still Referenced:**
- Issue: `blob.cpp` contains a comment noting that deprecated functions should be "used inline when needed" but no clear guidance on where they were moved or if they're callable
- Files: `src/blob/blob.cpp` (lines 11-12), `src/blob/blob_csv.cpp` (contains only a stub Impl struct)
- Impact: Maintenance burden and potential confusion about what's supported in blob operations
- Fix approach: Either complete the blob implementation or remove it entirely if not used. Update CLAUDE.md with actual blob support status

**BlobCSV Stub Implementation:**
- Issue: `BlobCSV::Impl` struct exists only as a placeholder with no implementation
- Files: `src/blob/blob_csv.cpp` (entire file)
- Impact: CSV blob serialization is non-functional
- Fix approach: Complete implementation or remove if this feature is not yet required

## Known Issues & Regressions

**Issue #52 - Label Validation in Collections:**
- Symptoms: Schema validation fails when creating collections without proper label column handling
- Files: `tests/schemas/issues/issue52/1/up.sql`, `tests/test_issues.cpp`
- Trigger: Creating a collection from the issue52 migration schema should throw an error
- Status: Test exists to verify the error is raised, but the underlying cause hasn't been documented. Label column validation in `src/schema_validator.cpp` may need review

**Issue #70 - Time Series Creation:**
- Symptoms: Time series element creation may have issues with mixed column types or naming
- Files: `bindings/dart/test/issues_test.dart` (lines 26-43)
- Trigger: Creating elements with nested time series data involving date_time arrays and multiple value columns
- Status: Test exists and passes, but the issue remains relevant for edge cases

## Security Considerations

**SQL Injection Risk in Schema Queries:**
- Risk: Several functions build SQL strings using string concatenation instead of parameterized queries when querying PRAGMA statements
- Files: `src/schema.cpp` (lines 296, 331, 360, 374), `src/database.cpp` (line 427)
- Current mitigation: Table names and pragmas come from internal schema discovery, not user input. However, `set_version()` uses string concatenation with user-supplied integer
- Recommendations:
  - Replace `PRAGMA table_info()`, `PRAGMA foreign_key_list()`, `PRAGMA index_list()`, `PRAGMA index_info()` with parameterized equivalents or use SQLite's meta-query tables
  - For `set_version()`, use parameterized query: `PRAGMA user_version = ?` instead of concatenating the version number

**execute_raw() Public API:**
- Risk: Public method `Database::execute_raw()` at `src/database.cpp:448` accepts arbitrary SQL without parameterization
- Files: `src/database.cpp` (lines 448-456)
- Current mitigation: Documented as accepting raw SQL - caller responsibility
- Recommendations: Document this as a low-level API and strongly recommend using `execute()` with parameterized queries for production code. Add warnings in CLAUDE.md about SQL injection

**Lua Scripting Sandbox:**
- Risk: `LuaRunner` allows Lua scripts to call database methods directly without validation
- Files: `src/lua_runner.cpp` (lines 23-xxx), C++ bindings for all Database methods
- Current mitigation: Scripts run in the same process context as the calling code
- Recommendations: Document security implications in CLAUDE.md. Consider adding a scripting policy for production use

## Performance Bottlenecks

**Large database.cpp File:**
- Problem: Main database implementation is 1934 lines, making it difficult to navigate and modify
- Files: `src/database.cpp`
- Cause: Contains all Database methods, Impl struct, transaction handling, and numerous read/write operations in a single file
- Improvement path: Consider splitting into logical modules (e.g., database_lifecycle.cpp, database_crud.cpp, database_queries.cpp) while maintaining Pimpl pattern

**Large C API Wrapper:**
- Problem: C API database wrapper is 1612 lines with heavy use of helper templates
- Files: `src/c/database.cpp`
- Cause: FFI bridge between C++ and C APIs requires extensive boilerplate for type conversion
- Improvement path: Template metaprogramming could reduce code duplication in read/free operations

**Memory Allocation in Read Operations:**
- Problem: Read operations for scalar/vector/string data allocate separate arrays that must be freed by caller
- Files: `src/c/database.cpp` (helper templates lines 12-76)
- Cause: C API design requires out-parameters for variable-length results
- Improvement path: Document memory management clearly. Consider providing utility functions to simplify common patterns like reading and immediately converting to Dart/Julia native types

**Lua Binding Overhead:**
- Problem: All Database methods are bound individually to Lua with lambdas
- Files: `src/lua_runner.cpp` (lines 23-100+)
- Cause: Sol2 requires explicit method binding for each public method
- Improvement path: This is acceptable but document that Lua is slower than C/C++ for performance-critical operations

## Fragile Areas

**Schema Validation and Discovery:**
- Files: `src/schema_validator.cpp`, `src/schema.cpp`
- Why fragile: Complex interdependencies between table naming conventions (Collection, Collection_vector_*, Collection_set_*, Collection_time_series_*) and schema detection logic. Any change to naming conventions requires updates in multiple places
- Safe modification:
  1. Add comprehensive tests for each naming pattern in `tests/test_schema_validator.cpp`
  2. Use consistent helper functions like `is_vector_table()`, `is_set_table()`, `is_time_series_table()` everywhere
  3. Document the naming rules clearly in CLAUDE.md (already done)
- Test coverage: Good - dedicated test file exists

**Time Series Dimension Column Detection:**
- Files: `src/database.cpp` (line 147-156), `src/schema.cpp` (DateTime type detection)
- Why fragile: Dimension column detection relies on:
  1. Column name starting with `date_` (case-sensitive)
  2. Or column type being DateTime
  3. Special datetime column detection in `is_date_time_column()`
- Safe modification: Add explicit schema validation that time series tables have exactly one dimension column, and document this requirement
- Test coverage: `test_database_time_series.cpp` covers basic operations but not all edge cases

**Transaction Guard and Error Recovery:**
- Files: `src/database.cpp` (lines 237-257, 202-235)
- Why fragile: TransactionGuard::rollback() is called in destructor and swallows errors (line 230-231). If rollback fails, the database may be in an inconsistent state but the error is logged but not propagated
- Safe modification:
  1. Document that rollback failures are non-recoverable errors
  2. Consider adding a flag to force close database if rollback fails
  3. Add test cases for transaction failure scenarios
- Test coverage: Need more comprehensive transaction failure tests

**Type Validator and Type Coercion:**
- Files: `src/type_validator.cpp` (lines 36-48)
- Why fragile: Allows double values in INTEGER columns and strings in TEXT/INTEGER/DateTime columns. This flexibility can mask type errors
- Safe modification: Document the type coercion rules clearly. Consider stricter validation mode for production
- Test coverage: `test_database_create.cpp` and `test_c_api_database_create.cpp` should include type coercion tests

## Scaling Limits

**In-Memory Database Logging:**
- Current capacity: No practical limit - spdlog handles it
- Limit: In-memory databases use console-only logging (no file sink)
- Scaling path: For very large in-memory databases, consider option to log to file despite being :memory: database

**Migration Version Numbers:**
- Current capacity: Limited to positive int64_t (up to 9,223,372,036,854,775,807)
- Limit: Practical limit is filesystem namespace - directory names for migration folders
- Scaling path: No action needed - int64_t provides sufficient capacity

**Foreign Key Cascade Operations:**
- Current capacity: Depends on SQLite query depth and recursion limits
- Limit: Very large delete operations could hit SQLite's internal limits
- Scaling path: Monitor delete operations on collections with many foreign key references

## Missing Critical Features

**Blob Module Non-Functional:**
- Problem: Time series files feature relies on blob reading/writing which is stubbed
- Blocks: `read_time_series_files()`, `update_time_series_files()` operations may not fully work with actual file data
- Impact: Cannot use time series file operations in production
- Severity: High if blob support is planned; Low if feature is being deferred

**Query Result Bounds Checking:**
- Problem: `Result::operator[]` (line 33 in `src/result.cpp`) performs bounds checking but `Result::at()` may throw on out-of-bounds access
- Impact: Inconsistent behavior between operator[] and at()
- Recommendation: Document the API contract clearly - which method to use for safe vs unsafe access

## Test Coverage Gaps

**SQL Injection Prevention:**
- What's not tested: Parameterized query handling with special characters, quotes, and SQL keywords in string values
- Files: `tests/test_database_query.cpp`
- Risk: Without explicit injection tests, regressions could be introduced
- Priority: Medium

**Transaction Failure Scenarios:**
- What's not tested: Rollback failures, database corruption recovery, partial commit failures
- Files: `tests/test_database_lifecycle.cpp`
- Risk: Unknown behavior when transactions fail
- Priority: High (impacts data integrity)

**Memory Management in C API:**
- What's not tested: Memory leak detection, double-free prevention, proper cleanup of string arrays
- Files: `tests/test_c_api_*.cpp`
- Risk: Memory leaks in C API bindings, particularly for scalar/vector result sets
- Priority: High (impacts binding stability)

**Blob Operations:**
- What's not tested: No functional tests exist for blob reading/writing
- Files: No test_blob.cpp exists
- Risk: Entire blob subsystem may be broken without detection
- Priority: High (feature is non-functional)

**Schema Edge Cases:**
- What's not tested:
  - Collections with underscores in names (explicitly forbidden, but no test verifies this)
  - Multiple time series groups in single collection
  - Foreign key chains beyond immediate parent
- Files: `tests/test_schema_validator.cpp`
- Risk: Unexpected behavior with complex schemas
- Priority: Medium

**Error Message Quality:**
- What's not tested: Specific error message content (only that errors are thrown)
- Files: All test_database_* files
- Risk: User-facing error messages may be unhelpful
- Priority: Low (but improves user experience)

---

*Concerns audit: 2026-02-09*
