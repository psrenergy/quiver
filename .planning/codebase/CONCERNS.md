# Codebase Concerns

**Analysis Date:** 2026-02-27

## Tech Debt

**Blob subsystem is a dead stub:**
- Issue: `src/blob/` contains stub implementations with empty function bodies. `blob.cpp` has `open_reader` and `open_writer` that return nothing. `blob_csv.cpp` and `blob_metadata.cpp` are empty. The `BlobMetadata` header references `Dimension` which is unresolved.
- Files: `src/blob/blob.cpp`, `src/blob/blob_csv.cpp`, `src/blob/blob_metadata.cpp`, `include/quiver/blob/blob.h`, `include/quiver/blob/blob_csv.h`, `include/quiver/blob/blob_metadata.h`
- Impact: The blob subsystem is included in CMake's format/tidy targets (`CMakeLists.txt` lines 84-85) but not in the actual build target. It is unfinished dead code. The public headers declare API that cannot be used.
- Fix approach: Either implement the blob subsystem fully or remove all files in `src/blob/` and `include/quiver/blob/` and clean up the CMakeLists.txt format target references.

**Python binding lacks LuaRunner:**
- Issue: Python has no `LuaRunner` class or Lua scripting exposure. Julia, Dart, and C++ all provide Lua integration. Python is missing the feature entirely.
- Files: `bindings/python/src/quiverdb/` (no `lua_runner.py` present)
- Impact: Python users cannot execute Lua scripts against the database. The feature gap is not documented anywhere in the Python binding.
- Fix approach: Add `bindings/python/src/quiverdb/lua_runner.py` wrapping `quiver_lua_runner_new`, `quiver_lua_runner_run`, `quiver_lua_runner_get_error`, and `quiver_lua_runner_free` from the C API. Follow the same pattern as `bindings/dart/lib/src/lua_runner.dart`.

**Python `_c_api.py` is maintained by hand:**
- Issue: `_c_api.py` contains hand-written CFFI `cdef` declarations. `_declarations.py` is the auto-generated reference copy. If the C API changes, both files must be manually kept in sync. The generator output goes to `_declarations.py`, not `_c_api.py`.
- Files: `bindings/python/src/quiverdb/_c_api.py`, `bindings/python/src/quiverdb/_declarations.py`, `bindings/python/generator/`
- Impact: C API changes not manually reflected in `_c_api.py` cause silent runtime failures or crashes with CFFI ABI mode.
- Fix approach: Evaluate whether `_c_api.py` can be made the direct generator output, eliminating the dual-file maintenance burden.

**`QUIVER_BUILD_C_API` defaults to OFF:**
- Issue: The top-level `CMakeLists.txt` line 17 sets `QUIVER_BUILD_C_API OFF` by default. Building the core library without the C API disables all language bindings. The `scripts/build-all.bat` forces it ON, but direct `cmake` invocations will miss it.
- Files: `CMakeLists.txt` line 17
- Impact: New contributors may build without the C API, producing a library that powers no bindings, with no error or warning surfaced.
- Fix approach: Change the default to `ON`, or add a `message(STATUS ...)` that clearly warns when C API is disabled.

**`read_scalar_integers/floats/strings` lack ORDER BY:**
- Issue: The three bulk read methods in `src/database_read.cpp` execute `SELECT attribute FROM collection` without `ORDER BY`. SQLite does not guarantee row order without explicit ordering.
- Files: `src/database_read.cpp` lines 9, 26, 43
- Impact: Callers receive values in indeterminate order. If callers expect results to correspond positionally to `read_element_ids` results, they will get mismatched data on tables with deletions or fragmented rowids.
- Fix approach: Add `ORDER BY rowid` or `ORDER BY id` to the three queries, consistent with `read_element_ids` at `src/database_read.cpp` line 206.

**Column ordering in metadata iterates `std::map` alphabetically, not schema order:**
- Issue: `TableDefinition::columns` is a `std::map<std::string, ColumnDefinition>` (see `include/quiver/schema.h` line 42). Iterating it in metadata and time series functions returns columns in alphabetical order, not the order defined in the SQL schema.
- Files: `include/quiver/schema.h` line 42, `src/database_metadata.cpp` lines 45, 71, 88, `src/database_time_series.cpp` lines 44, 69
- Impact: Multi-column group metadata (time series, vectors) may present value columns in alphabetical rather than schema-definition order, confusing callers that depend on positional column ordering. The C API's columnar time series output depends on this ordering.
- Fix approach: Replace `std::map` with a structure that preserves insertion order (e.g. `std::vector<std::pair<std::string, ColumnDefinition>>` with a lookup helper). Schema loading via `PRAGMA table_info` returns columns in definition order — that order is lost when inserted into `std::map`.

## Known Bugs

**`update_element` and `delete_element` silently succeed for nonexistent IDs:**
- Symptoms: Calling `db.update_element("Collection", 999, ...)` or `db.delete_element("Collection", 999)` on a nonexistent ID returns success without error. SQLite executes the SQL and reports 0 rows affected, but the library never calls `sqlite3_changes()`.
- Files: `src/database_update.cpp` line 46, `src/database_delete.cpp` line 10
- Trigger: Call either operation with an ID that does not exist in the collection.
- Workaround: Callers must validate existence before calling via `read_scalar_integer_by_id` or similar.

**`import_csv` disables foreign keys globally without crash recovery:**
- Symptoms: `import_csv` calls `PRAGMA foreign_keys = OFF` before DELETE/INSERT, then re-enables in catch/finally. If the process crashes between disable and re-enable, the database session is left with foreign keys disabled on next open (SQLite resets PRAGMAs per connection, so this is actually safe across reconnects, but within a single session a crash leaves state inconsistent).
- Files: `src/database_csv_import.cpp` lines 276-283, 370-376, 602-609
- Trigger: Process crash or kill signal during `import_csv` execution.
- Workaround: None at the library level. Callers should make backups before import.

## Security Considerations

**SQL injection via user-controlled collection and attribute names:**
- Risk: Methods like `read_scalar_integers`, `update_element`, `create_element`, and all group operations build raw SQL strings by concatenating `collection`, `attribute`, `vector_table`, and `ts_table` names. Example: `"SELECT " + attribute + " FROM " + collection` in `src/database_read.cpp` line 9.
- Files: `src/database_read.cpp` lines 9, 26, 43, 61, 74, 87; throughout `src/database_create.cpp`, `src/database_update.cpp`, `src/database_time_series.cpp`
- Current mitigation: `is_safe_identifier` (alphanumeric + underscore only) is applied during schema loading in `src/schema.cpp` lines 329, 367, 399, 417. The `require_collection` and `require_column` guards validate that names exist in the loaded schema before SQL construction. Names cannot be injected unless the schema itself is compromised. The `query_string/integer/float` methods accept raw SQL from callers with no sanitization — this is by design and is a trust boundary.
- Recommendations: Add a doc comment in `include/quiver/database.h` near the `query_string/integer/float` declarations stating that these accept arbitrary SQL and must only be called with trusted or application-controlled input.

**`quiver_database_path` returns a pointer into internal C++ string storage:**
- Risk: `src/c/database.cpp` line 51 returns `db->db.path().c_str()`. This pointer is valid only as long as the `quiver_database_t` is alive and the path string is not modified. The caller receives a raw `const char*` into C++ object internals with no documented lifetime.
- Files: `src/c/database.cpp` lines 48-53
- Current mitigation: The path string is set once at construction and never changed, so the pointer is stable for the lifetime of the database object. However this is not documented in `include/quiver/c/database.h`.
- Recommendations: Add a lifetime comment to `include/quiver/c/database.h`. Consider using `strdup_safe` and a matching `free` function, consistent with all other string-returning C API functions.

## Performance Bottlenecks

**Schema validation runs on every database open:**
- Problem: `impl_->load_schema_metadata()` runs `Schema::from_database()` (multiple PRAGMA queries per table) and `SchemaValidator::validate()` on every `from_schema` or `from_migrations` call.
- Files: `src/database_impl.h` lines 127-132, `src/database.cpp` lines 363, 391, `src/schema_validator.cpp`
- Cause: No schema caching. For schemas with many tables this adds measurable startup cost.
- Improvement path: For the current use case (small schemas) this is acceptable. If schemas grow large, cache the parsed schema to avoid repeated PRAGMA queries.

**`read_time_series_group` in C API executes two database calls:**
- Problem: `quiver_database_read_time_series_group` in `src/c/database_time_series.cpp` line 86-87 calls `get_time_series_metadata` then `read_time_series_group` — two separate database operations for what could be one.
- Files: `src/c/database_time_series.cpp` lines 85-87
- Cause: The metadata call is needed to build the column type list for the C API's columnar output format, but this information is already in the loaded in-memory schema.
- Improvement path: Read column type information directly from `Impl::schema` rather than re-querying the database via `get_time_series_metadata`.

**`import_csv` replaces semicolons via full string copy:**
- Problem: `src/database_csv_import.cpp` lines 57-62 replace every `;` with `,` in the entire file content string before parsing. For large CSV files this copies the entire content twice in memory.
- Files: `src/database_csv_import.cpp` lines 40-116
- Cause: rapidcsv does not natively handle `sep=` headers, so detection and replacement is done manually.
- Improvement path: Use rapidcsv's `SeparatorParams` directly with the detected separator character rather than replacing file content.

## Fragile Areas

**`find_dimension_column` uses column name heuristic, not schema constraint:**
- Files: `src/database_internal.h` lines 68-77, `src/schema.cpp` (the `is_date_time_column` helper)
- Why fragile: The time series dimension column is identified by checking if `col.type == DataType::DateTime` or if `is_date_time_column(col_name)` returns true (the latter checks for a `date_` name prefix). Any column named with a `date_` prefix (e.g. `date_notes TEXT`) would be incorrectly identified as the dimension column.
- Safe modification: Always use the `date_` prefix strictly for the dimension column. The schema validator does not enforce this naming rule beyond requiring at least one dimension column be discoverable.
- Test coverage: All time series tests in `tests/test_database_time_series.cpp` use `date_time` as the dimension column name; edge cases with other `date_`-prefixed columns are untested.

**`resolve_fk_label` silently skips FK resolution if schema lookup fails:**
- Files: `src/database_impl.h` lines 95-125
- Why fragile: When resolving FK labels for array columns, if `find_all_tables_for_column` returns matches but `schema->get_table(match.table_name)` returns null for all of them, `resolve_table` stays null and values pass through without FK resolution — silently skipping validation for malformed schemas.
- Safe modification: Throw if any match from `find_all_tables_for_column` has a table name not present in the loaded schema.

**`QUIVER_REQUIRE` macro is capped at 9 arguments:**
- Files: `src/c/internal.h` lines 28-116
- Why fragile: `QUIVER_REQUIRE` supports 1-9 arguments via manual overloads. If any C API function needs to validate more than 9 pointer parameters, adding a 10th will silently mismatch the macro dispatch. The current maximum use is 9 parameters (`quiver_database_read_time_series_group` at `src/c/database_time_series.cpp` lines 82-83).
- Safe modification: Before adding new C API functions, count the pointer parameters and ensure they total 9 or fewer, or extend `src/c/internal.h` with a `QUIVER_REQUIRE_10` overload.

**`is_collection` detection relies on absence of underscore:**
- Files: `src/schema.cpp` lines 89-94
- Why fragile: `is_collection` returns true for any table name without an underscore (plus the hardcoded `Configuration` exception). Any table added to the database by SQLite internals or third-party tools that has an underscore-free name would be misclassified as a collection. The converse — collection names cannot contain underscores — is enforced by the schema validator but only during validation.
- Safe modification: This is a documented constraint enforced at schema load time. No change needed for normal usage.

## Scaling Limits

**No WAL mode enabled (single-writer bottleneck):**
- Current capacity: One writer at a time; SQLite default DELETE journal mode.
- Limit: Multiple processes or threads opening the same database file simultaneously will block on writes, and concurrent readers are blocked during writes.
- Scaling path: Add `PRAGMA journal_mode = WAL;` to `Database::Database()` in `src/database.cpp` after the foreign keys pragma. WAL allows concurrent reads during writes without blocking.

**All results materialized in memory:**
- Current capacity: `Result` stores all rows as `std::vector<Row>`. Time series reads return `std::vector<std::map<std::string, Value>>`.
- Limit: Large datasets (millions of rows) will exhaust memory. There is no streaming or cursor API.
- Scaling path: Add cursor-based read variants that yield rows incrementally. This requires a new public API surface.

## Dependencies at Risk

**sol2 causes heavy compilation on `lua_runner.cpp`:**
- Risk: sol2 is a header-only library causing heavy template instantiations. `src/CMakeLists.txt` lines 51-55 add `/bigobj` (MSVC) or `-Wa,-mbig-obj` (MinGW) flags specifically for `lua_runner.cpp` to avoid object file section count overflow.
- Impact: `lua_runner.cpp` is the slowest file to compile. Any changes trigger a full recompile of the largest translation unit in the project.
- Migration plan: No immediate risk. sol2 is stable. Monitor for sol3 or lighter alternatives if build times become unacceptable.

**rapidcsv version is not pinned:**
- Risk: rapidcsv is fetched via CMake FetchContent without a pinned version. Upstream behavior changes could silently break CSV import/export round-trips.
- Impact: CSV accuracy could regress without any test failure if the library changes its quoting or parsing behavior.
- Migration plan: Pin the rapidcsv version tag or commit hash in the CMake FetchContent declaration in `cmake/Dependencies.cmake`.

## Missing Critical Features

**No `update_element` / `delete_element` existence verification:**
- Problem: Neither operation verifies the element ID exists before executing SQL. Both succeed silently for nonexistent IDs.
- Blocks: Callers cannot distinguish "operation succeeded" from "operation silently did nothing."

**No multi-row query API:**
- Problem: `query_string`, `query_integer`, and `query_float` return only the first row's first column. There is no API for retrieving multiple rows or multiple columns from a user-provided SQL query.
- Blocks: Users who need joins, aggregates across rows, or tabular results must resort to multiple single-row queries or use the lower-level `execute()` method (which is private).

**No streaming/cursor read API:**
- Problem: All read operations materialize all rows before returning. There is no way to iterate results row-by-row.
- Blocks: Use with collections exceeding available memory.

## Test Coverage Gaps

**`update_element` with nonexistent ID is not tested:**
- What's not tested: Calling `update_element("Collection", 99999, elem)` where the ID does not exist. Expected to fail; currently succeeds silently.
- Files: `src/database_update.cpp`, `tests/test_database_update.cpp`
- Risk: Callers may believe an update succeeded when no row was changed.
- Priority: High

**`delete_element` with nonexistent ID is not tested:**
- What's not tested: Calling `delete_element("Collection", 99999)` where the ID does not exist. Currently succeeds silently.
- Files: `src/database_delete.cpp`, `tests/test_database_delete.cpp`
- Risk: Callers may believe a deletion succeeded when no row was removed.
- Priority: High

**Blob subsystem has no tests:**
- What's not tested: Everything in `src/blob/` and `include/quiver/blob/` — the entire blob read/write/CSV/metadata API.
- Files: `src/blob/blob.cpp`, `src/blob/blob_csv.cpp`, `src/blob/blob_metadata.cpp`
- Risk: The stubs compile without signals of incompleteness. No test coverage for when the feature is eventually implemented.
- Priority: Low (stubs only, not buildable)

**Python binding has no LuaRunner tests:**
- What's not tested: Python Lua scripting integration — the feature is absent.
- Files: `bindings/python/tests/`
- Risk: If LuaRunner is added to Python, there is no test baseline.
- Priority: Medium

**`quiver_database_path` pointer lifetime is not tested in C API tests:**
- What's not tested: Using the returned `const char*` from `quiver_database_path` after the database is closed.
- Files: `src/c/database.cpp` lines 48-53, `tests/test_c_api_database_lifecycle.cpp`
- Risk: Use-after-free if bindings cache the raw pointer beyond the database lifetime. Currently only `database.py` in the Python binding calls `quiver_database_path` and immediately decodes it, so there is no current unsafe usage.
- Priority: Medium

---

*Concerns audit: 2026-02-27*
