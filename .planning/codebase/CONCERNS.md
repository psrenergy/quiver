# Codebase Concerns

**Analysis Date:** 2026-02-20

## Tech Debt

**Blob subsystem is entirely unimplemented:**
- Issue: All blob-related classes (`Blob`, `BlobCSV`, `BlobMetadata`, `Dimension`, `TimeProperties`) have empty `.cpp` implementations - no methods are implemented at all.
- Files: `src/blob/blob.cpp`, `src/blob/blob_csv.cpp`, `src/blob/blob_metadata.cpp`, `src/blob/dimension.cpp`, `src/blob/time_properties.cpp`
- Impact: Headers are compiled and shipped but calling any blob method results in undefined behavior. No tests exist for blob at all.
- Fix approach: Either implement the blob subsystem fully (with tests) or delete all blob headers and sources until the feature is needed.

**`export_csv` and `import_csv` are empty stubs:**
- Issue: Both methods are declared in the public C++ API, C API, and bound in the Dart binding, but the implementations are empty function bodies that do nothing and return no error.
- Files: `src/database_describe.cpp` lines 91-93, `src/c/database.cpp` lines 133-153, `include/quiver/database.h` lines 175-176, `include/quiver/c/database.h` lines 421-422
- Impact: Callers of `export_csv` / `import_csv` will receive `QUIVER_OK` but no file will be written and no data imported. Silent data loss for callers who depend on this.
- Fix approach: Implement using SQLite's `.csv` output mode or a manual query-and-write approach, or throw `std::runtime_error("Not implemented")` until ready.

**Migration rollback (`migrate_down`) is not exposed:**
- Issue: `Migration::down_sql()` exists and `down.sql` files are present in test schemas, but there is no `migrate_down()` method on `Database`, no C API function, and no binding support for rolling back migrations.
- Files: `src/migration.cpp` line 37, `src/migrations.cpp`, `include/quiver/migration.h`
- Impact: Schema rollbacks are impossible through the library API. Callers who need to downgrade must manipulate the SQLite database directly.
- Fix approach: Add `Database::migrate_down(target_version)`, C API `quiver_database_migrate_down()`, and binding wrappers.

**`describe()` writes to `std::cout` directly:**
- Issue: `Database::describe()` uses raw `std::cout` for output instead of returning a string or using the instance logger.
- Files: `src/database_describe.cpp`
- Impact: Output cannot be captured in tests, redirected to a file, or exposed through the C API / bindings in a useful way. The Lua binding also just calls it and discards the output.
- Fix approach: Change signature to `std::string describe() const` and return the formatted string, or accept an `std::ostream&` parameter.

**`std::map` row representation has implicit column ordering dependency:**
- Issue: `read_time_series_group` returns `std::vector<std::map<std::string, Value>>` and `update_time_series_group` accepts the same type. `std::map` iterates in alphabetical key order, so when `update_time_series_group` extracts `value_columns` from `rows[0]`, the column order is alphabetical — not schema order. The generated INSERT SQL column list matches this alphabetical order, which happens to work because VALUES placeholders match the same iteration, but this is fragile coupling.
- Files: `src/database_time_series.cpp` lines 140-145
- Impact: If column extraction ever uses a different ordering from INSERT placeholder binding, data will be written to wrong columns silently. Currently safe but non-obvious.
- Fix approach: Build the column list from schema metadata (already available as `table_def->columns`) rather than from `rows[0]` keys.

**Schema classification uses fragile naming heuristics:**
- Issue: `Schema::is_collection()` treats any table name without an underscore as a collection. `Schema::get_parent_collection()` extracts the parent name by splitting at the first underscore. This works only because the naming convention forbids underscores in collection names (enforced by schema validator), but any third-party SQLite table with an underscore would be misclassified.
- Files: `src/schema.cpp` lines 89-123
- Impact: If a user attaches a database or applies additional SQL with underscores in table names, the schema introspection will silently misclassify tables and potentially expose them as collections or group tables.
- Fix approach: Consider storing collection names explicitly during `load_schema_metadata()` rather than re-detecting from name patterns at every call.

## Known Bugs

**`read_vector_*` / `read_set_*` / `update_vector_*` / `update_set_*` dereference null schema pointer when called without a loaded schema:**
- Symptoms: These methods call `require_collection()` which calls `require_schema()`, but `require_collection()` calls `schema->has_table()` after checking `schema` for null. However, the test comments in `test_database_errors.cpp` lines 135-137 and 165-167 and 269-271 explicitly state these methods "cause segfault (null pointer dereference) because `impl_->schema->find_vector_table()` is called without null check" — but looking at the code, `require_collection` does have the null check. The test comments appear to document a formerly present bug that may have been partially addressed. The comments themselves are stale and misleading.
- Files: `tests/test_database_errors.cpp` lines 135-137, 165-167, 269-271, 298-300
- Trigger: The comment says "without schema", but the tests themselves do use a loaded schema.
- Fix approach: Remove or update the stale misleading comments in the test file.

**`create_element` with time series data does not validate that the dimension column is provided:**
- Symptoms: When inserting time series rows via `create_element` / `update_element`, the code iterates over user-provided column arrays and inserts them. It does not check whether the dimension column (e.g., `date_time`) was included in the arrays. If missing, SQLite will receive a NOT NULL violation or a primary key constraint violation at runtime.
- Files: `src/database_create.cpp` lines 197-212, `src/database_update.cpp` lines 147-160
- Trigger: Pass a time series attribute array via `Element::set()` without including the dimension column data — the check only exists in `update_time_series_group`, not in the `create_element`/`update_element` path.
- Fix approach: Add a pre-insert check that the dimension column is present among the provided column names when inserting time series rows.

## Security Considerations

**SQL injection via collection name, attribute name, and table name interpolation:**
- Risk: All SQL strings in the C++ layer are built by concatenating `collection`, `attribute`, `table`, and `dim_col` strings directly into SQL without quoting or escaping. Examples: `"SELECT " + attribute + " FROM " + collection`, `"UPDATE " + collection + " SET " + attribute + " = ?"`. While collection and attribute names are validated to exist in the schema (via `require_collection` and `require_column`), they are not validated to be safe SQL identifiers before interpolation. A collection name containing SQL metacharacters (`;`, `--`, spaces, quotes) could alter the query structure.
- Files: `src/database_read.cpp` (all read methods), `src/database_update.cpp` (all update methods), `src/database_create.cpp`, `src/database_relations.cpp`, `src/database_time_series.cpp`
- Current mitigation: `require_collection` confirms the name exists in the schema, which was loaded from the actual SQLite `sqlite_master`. Since schema names originate from the database file itself (not user input at query time), the risk is lower — an attacker would need to control the schema file contents. However, collection names received from callers that bypass this check (e.g., through the Lua runner) have no additional sanitization.
- Recommendations: Apply `is_safe_identifier()` (already defined in `src/schema.cpp` line 11) to collection and attribute names before SQL construction, or use SQLite identifier quoting (`"[" + name + "]"` or `'"' + name + '"'`).

**Lua runner opens `base`, `string`, and `table` libraries — no I/O sandbox:**
- Risk: `sol::lib::base` includes `load`, `loadstring`, `dofile`, and `require`. A script passed to `LuaRunner::run()` can potentially load arbitrary Lua code from the filesystem using `require` or `dofile` if those remain accessible. `sol::lib::io` and `sol::lib::os` are excluded, limiting direct file access, but `require` can load `.so`/`.dll` native extensions.
- Files: `src/lua_runner.cpp` line 18
- Current mitigation: `sol::lib::io` and `sol::lib::os` are not loaded, reducing the attack surface.
- Recommendations: Consider also excluding `sol::lib::package` to prevent `require` from loading native extensions. If Lua scripts are always trusted (internal use only), document this assumption.

## Performance Bottlenecks

**Per-row INSERT in vectors/sets/time series (N+1 write pattern):**
- Problem: All bulk data operations (`create_element`, `update_element`, `update_vector_*`, `update_set_*`, `update_time_series_group`) issue one `execute()` call per row. A vector of 10,000 points causes 10,000 individual `sqlite3_prepare_v2` + `sqlite3_step` calls inside a single transaction.
- Files: `src/database_create.cpp` lines 101-117, `src/database_update.cpp` lines 88-100, `src/database_time_series.cpp` lines 159-181
- Cause: Each `execute()` call prepares the statement from scratch (no statement caching or reuse).
- Improvement path: Cache the prepared statement outside the row loop and rebind parameters per row, or use multi-row `INSERT INTO ... VALUES (?,?),(?,?),(?,?)` syntax for batches.

**Schema re-loaded on every `from_schema` / `from_migrations` open:**
- Problem: `load_schema_metadata()` runs a full `PRAGMA table_info` + `PRAGMA foreign_key_list` + `PRAGMA index_list` + `PRAGMA index_info` query for every table every time a database is opened. For schemas with many tables this adds measurable startup latency.
- Files: `src/database_impl.h` lines 48-53, `src/schema.cpp` lines 293-326
- Cause: No schema caching between open/close cycles of the same database file.
- Improvement path: This is acceptable for a WIP project. Consider caching schema to a sidecar file or using a hash of `sqlite_master` as a cache key if startup time becomes a concern.

**`list_scalar_attributes` / `list_vector_groups` / `list_set_groups` / `list_time_series_groups` return full metadata for every group, including foreign key scan per attribute:**
- Problem: `list_scalar_attributes` scans all foreign keys for each column in a nested loop (O(columns × foreign_keys)). `list_vector_groups`, `list_set_groups`, and `list_time_series_groups` call `get_*_metadata()` individually for each group, each of which iterates all columns again.
- Files: `src/database_metadata.cpp` lines 82-103, 105-124, 126-145
- Cause: No caching of metadata results; full schema iteration per call.
- Improvement path: Acceptable for current scale. Pre-build metadata index at `load_schema_metadata()` time if this becomes a bottleneck.

## Fragile Areas

**`update_time_series_files` column name interpolation into SQL:**
- Files: `src/database_time_series.cpp` lines 287-308
- Why fragile: Column names from the `paths` map are interpolated directly into the INSERT SQL string (`insert_sql += col_name`). They are validated to exist in the table definition (line 273), but not sanitized as SQL identifiers. A column name with special characters in the schema would produce broken SQL.
- Safe modification: After adding identifier validation (see Security section), this is safe. Currently safe in practice since column names come from SQLite's `PRAGMA table_info` output.
- Test coverage: Covered in `tests/test_database_time_series.cpp` for the happy path; no tests for unusual column names.

**`is_date_time_column` convention is the sole mechanism for detecting the time series dimension column:**
- Files: `src/database_internal.h` lines 68-76, `include/quiver/data_type.h` lines 39-41
- Why fragile: If a time series table has no column starting with `date_`, `find_dimension_column()` throws at runtime. Schema validation does not enforce the `date_` prefix requirement for time series tables (the `validate_time_series_files_table` exists but there is no `validate_time_series_table`).
- Safe modification: The schema validator in `src/schema_validator.cpp` line 38 skips time series table validation entirely. Adding a time series validation step (checking for `date_` column) would catch schema authoring errors at open time instead of at first read.
- Test coverage: Time series tests all use schemas with `date_time` columns; no tests exist for a malformed time series schema.

**`read_time_series_group` falls back to null value for unrecognized column types:**
- Files: `src/database_time_series.cpp` lines 92-108
- Why fragile: The column value extraction uses three sequential `get_integer` / `get_float` / `get_string` checks. If all three return nullopt (e.g., BLOB column), the row silently stores `nullptr` for that column. This is consistent with how `Database::execute()` handles unexpected types, but BLOB is explicitly unsupported and throws in `execute()` (line 196). The time series path would not see that throw since it goes through the `Result` abstraction.
- Safe modification: This is safe for correctly-formed STRICT schemas (BLOB is not a STRICT type). The concern only applies to non-STRICT schemas.

## Scaling Limits

**All-element read operations load entire column into memory:**
- Current capacity: Unbounded — all rows returned from `read_scalar_integers` / `read_vector_floats` / etc. are materialized into `std::vector` in memory before returning.
- Limit: For collections with millions of rows or large vector tables, this will exhaust available memory.
- Scaling path: Add cursor/iterator read variants, or pagination parameters (`LIMIT` / `OFFSET`), to support streaming access.

**Single-writer SQLite with no WAL mode configured:**
- Current capacity: One write at a time; readers block on writes in default journal mode.
- Limit: Not suitable for concurrent multi-process access patterns without WAL mode.
- Scaling path: Add `PRAGMA journal_mode = WAL;` to the `Database` constructor after `PRAGMA foreign_keys = ON;` to improve read concurrency.

## Missing Critical Features

**No time series table validation in SchemaValidator:**
- Problem: `SchemaValidator::validate()` explicitly skips time series table validation (comment at line 38: "Time series tables have minimal validation (just file paths)"). No check verifies the `date_` dimension column is present, that the foreign key to the parent collection exists, or that cascade rules are correct.
- Files: `src/schema_validator.cpp` line 38
- Blocks: Invalid time series schemas silently open and only fail at first data operation.

**No `migrate_down` API:**
- Problem: Schema rollback is unavailable through the public API despite `down.sql` files existing in migrations.
- Files: `src/migration.cpp`, `src/migrations.cpp`, `include/quiver/database.h`
- Blocks: Any workflow requiring schema rollback must manipulate the database file directly.

## Test Coverage Gaps

**Blob subsystem has zero tests:**
- What's not tested: All of `Blob`, `BlobCSV`, `BlobMetadata`, `Dimension`, `TimeProperties`
- Files: `include/quiver/blob/`, `src/blob/`
- Risk: Implementations are empty; any future implementation would have no test baseline.
- Priority: Low (depends on whether blob subsystem will be implemented or deleted).

**`export_csv` / `import_csv` are not tested in C++, C API, or binding tests:**
- What's not tested: CSV round-trip correctness, file creation, error handling for bad paths.
- Files: `src/database_describe.cpp`, `src/c/database.cpp`, `bindings/dart/lib/src/database_csv.dart`, `bindings/julia/src/database_csv.jl`
- Risk: Silent no-ops currently succeed; when implemented, regressions in CSV format or path handling would go undetected.
- Priority: Medium — dependent on implementation landing.

**No tests for `read_vector_*` / `read_set_*` / `update_vector_*` / `update_set_*` with no schema loaded:**
- What's not tested: Behavior when `impl_->schema` is null and these methods are called (test comments in `test_database_errors.cpp` mark these as skipped).
- Files: `tests/test_database_errors.cpp` lines 135-168, 269-312
- Risk: A crash (segfault) rather than a `std::runtime_error` would be surfaced to callers.
- Priority: Medium — these are public API entry points.

**No tests for `describe()` output correctness:**
- What's not tested: The string output of `describe()` is never asserted against expected content — it is only called to verify it does not crash.
- Files: `src/database_describe.cpp`, `tests/test_database_lifecycle.cpp`
- Risk: Output format regressions go undetected.
- Priority: Low.

**No tests for time series schema validation (missing dimension column):**
- What's not tested: Opening a time series schema that lacks the `date_` column.
- Files: `src/schema_validator.cpp`, `tests/test_schema_validator.cpp`
- Risk: Invalid schemas fail only at first read/write, not at open time.
- Priority: Medium.

---

*Concerns audit: 2026-02-20*
