# Codebase Concerns

**Analysis Date:** 2026-04-18

This document catalogs technical debt, bugs, security considerations, performance bottlenecks, fragile areas, and gaps in the Quiver codebase. The project self-describes as WIP with "breaking changes acceptable, no backwards compatibility required", so the emphasis is on surfacing items rather than prescribing migration paths. Every claim below is anchored to a file path and, where applicable, line numbers.

---

## Tech Debt

**Per-row `sqlite3_prepare_v2` in write loops:**
- Issue: `Database::execute()` in `src/database.cpp:131-212` prepares, binds, steps and finalizes a statement per call. All bulk write paths invoke it in a tight `for (row = 0; row < row_count; ++row)` loop without statement caching.
- Files:
  - `src/database_csv_import.cpp:438` (scalar INSERT loop inside transaction)
  - `src/database_csv_import.cpp:466-468` (self-FK UPDATE loop)
  - `src/database_csv_import.cpp:672` (group INSERT loop)
  - `src/database_time_series.cpp:180` (`update_time_series_group` re-prepares INSERT per row)
  - `src/database_impl.h:192-206` (vector insert loop)
  - `src/database_impl.h:231-243` (set insert loop)
  - `src/database_impl.h:268-281` (time-series insert loop)
- Impact: Large imports and time-series updates pay the prepare/parse cost per row. With tens of thousands of rows this dominates runtime even inside an explicit transaction. The `quiver_benchmark.exe` in `tests/benchmark/benchmark.cpp:96-158` exists specifically to compare "individual" vs "batched" transaction variants but does not measure prepared-statement reuse.
- Fix approach: Prepare the INSERT once, bind in a loop, reset between iterations. Alternatively add a cached-statement map on `Database::Impl`.

**Dynamic SQL concatenation instead of prepared templates:**
- Issue: `src/database_impl.h:192-206`, `src/database_impl.h:231-243`, `src/database_impl.h:268-281` all build `INSERT INTO ... (col1, col2, ...) VALUES (?, ?, ...)` inside the row loop instead of hoisting the template outside. The string is rebuilt every iteration even though column sets are row-invariant.
- Files: `src/database_impl.h:170-284`
- Impact: Allocation churn on every row of every write; every column name is concatenated N*M times for N rows and M columns.
- Fix approach: Build SQL string once outside the row loop, capture column ordering, then only rebuild `params` per row.

**Duplicate `unordered_map` construction at C API boundary:**
- Issue: `src/c/binary/binary_file.cpp:69-72` and `:101-104` build `std::unordered_map<std::string, int64_t>` from parallel arrays on every `read`/`write`. The internal C++ method already treats this map as the #1 hot-path bottleneck per `CLAUDE.md`.
- Files: `src/c/binary/binary_file.cpp:69-72`, `src/c/binary/binary_file.cpp:101-104`
- Impact: FFI consumers (Julia, Dart, Python, JS) pay the 40% hash-map cost twice — once marshalling C arrays into `std::unordered_map`, and again inside `BinaryFile::read/write`.
- Fix approach: Introduce a `vector<int64_t>` overload on `BinaryFile::read/write` keyed by dimension index, and have the C API pass values through in declaration order.

**C API `write()` copies `data` a second time:**
- Issue: `src/c/binary/binary_file.cpp:106` copies the caller's `const double* data` into a fresh `std::vector<double>` before calling `BinaryFile::write()`. The C++ `write()` signature takes `const std::vector<double>&` and does not mutate the buffer.
- Files: `src/c/binary/binary_file.cpp:106-107`, `include/quiver/binary/binary_file.h:38`
- Impact: Extra allocation + copy per write call (worst case ~7.3M copies of ~24 B buffers in the CLAUDE.md scenario).
- Fix approach: Change `BinaryFile::write` to accept `std::span<const double>` (C++20) or raw pointer + length; have the C++ layer construct the vector only if needed.

**CSV import disables foreign keys, relying on `try/catch` to restore:**
- Issue: `src/database_csv_import.cpp:276-282`, `:370-476`, `:601-679` all call `execute_raw("PRAGMA foreign_keys = OFF")` and attempt to restore with `execute_raw("PRAGMA foreign_keys = ON")` in two branches (success and catch). The restore in the catch block is itself not wrapped in a try/catch — if the database is mid-error, restoring FK could throw and mask the original exception.
- Files: `src/database_csv_import.cpp:473-483`, `:676-686`
- Impact: If the restore throws, the original import error is swallowed and FK state may be left disabled for the remaining session.
- Fix approach: Wrap the PRAGMA restore in its own try/catch, and consider an RAII `ForeignKeyGuard` to guarantee restoration.

**`sqlite3_exec` return code ignored for PRAGMA calls:**
- Issue: `src/database.cpp:116` runs `sqlite3_exec(impl_->db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr)` without checking the return code and without an error-message out-param. If this fails (e.g., old SQLite build, locked DB), the caller never learns.
- Files: `src/database.cpp:116`
- Impact: Silent failure — `Database` is left without FK enforcement despite `CLAUDE.md` claims "foreign keys enabled".
- Fix approach: Check return code, throw `std::runtime_error("Failed to open database: foreign_keys PRAGMA failed: ...")` on non-`SQLITE_OK`.

**No `-Werror` on CI/local build:**
- Issue: `cmake/CompilerOptions.cmake:4-30` sets `/W4` (MSVC) and `-Wall -Wextra -Wpedantic` (GCC/Clang) but no `-Werror` / `/WX`. Warnings do not break the build.
- Files: `cmake/CompilerOptions.cmake:1-30`
- Impact: Regression risk — new warnings can accumulate unnoticed. `switch` statements with missing returns (e.g., `src/binary/time_properties.cpp:49-67`, `:70-86`) technically compile even though falling off the end after a non-exhaustive switch is UB.
- Fix approach: Add `-Werror` in CI at minimum; consider `/WX` for MSVC.

**Benchmark never runs in CI:**
- Issue: `tests/CMakeLists.txt:93-96` builds `quiver_benchmark` but no `add_test` / `gtest_discover_tests` registers it; it is comment-tagged "not part of test suite" and `CLAUDE.md` confirms it is run manually. `.github/workflows/ci.yml` never invokes it.
- Files: `tests/benchmark/benchmark.cpp`, `tests/CMakeLists.txt:93-96`, `.github/workflows/ci.yml`
- Impact: Performance regressions in the primary write path (create_element + update_time_series_group loop) go undetected.
- Fix approach: Add a smoke-test invocation of `quiver_benchmark --smoke` with a regression budget to a nightly workflow.

**Stale claims in CLAUDE.md `Binding-Only Convenience Methods` table:**
- Issue: `CLAUDE.md` claims "Composite read helpers" (`readScalarsById`, `readVectorsById`, `readSetsById`) exist only in Julia/Dart/Lua/JS, and that `read_vector_group_by_id` / `read_set_group_by_id` exist only in Julia and Dart. In reality Python has them too (`bindings/python/src/quiverdb/database.py:1263`, `:1283`, `:1305`, `:1337`, `:1373`).
- Files: `bindings/python/src/quiverdb/database.py:1263-1399`, `CLAUDE.md` Binding-Only Convenience Methods tables
- Impact: Documentation drift; future contributors may duplicate existing functionality.
- Fix approach: Update the CLAUDE.md tables to include the Python column.

---

## Known Bugs

**`quiver_database_close` silently deletes arbitrary pointer:**
- Symptoms: `src/c/database.cpp:32-35` calls `delete db` without `QUIVER_REQUIRE` or any sanity check. Passing a pointer not produced by `new quiver_database` is undefined behavior and the function always returns `QUIVER_OK`.
- Files: `src/c/database.cpp:32-35`
- Trigger: Any FFI caller that accidentally passes an invalidated handle (double-close or pointer from a different allocator).
- Workaround: None. Callers must trust themselves.
- Note: `quiver_binary_file_close` (`src/c/binary/binary_file.cpp:49-52`) has the same shape.

**`quiver_database_free_string` has no null-pointer guard:**
- Symptoms: `src/c/database_read.cpp:80-83` calls `delete[] str` with no `QUIVER_REQUIRE`. `delete[]` on `nullptr` is defined to be a no-op, but the inconsistency with `quiver_database_free_integer_array` (`:56`) / `_float_array` (`:63`) / `_string_array` (`:70`) — all of which DO call `QUIVER_REQUIRE(values)` — means a caller migrating from one helper to another may get surprising behavior when they pass a mistakenly-computed pointer.
- Files: `src/c/database_read.cpp:80-83`, `src/c/binary/binary_file.cpp:139-142`, `src/c/binary/binary_metadata.cpp:260-263`
- Trigger: Calling `quiver_database_free_string(p)` with an uninitialized pointer.
- Workaround: Initialize out-parameters explicitly.

**Query-float / query-integer params leave `out_value` uninitialized when `has_value == 0`:**
- Symptoms: In `src/c/database_query.cpp:76-77` and `:95-97` (plain) and `:159-160`, `:187-188` (params variants), when the query returns no row, `*out_has_value = 0` but `*out_value` is never assigned. The string variants at `:53-55` and `:128-129` correctly set `*out_value = nullptr`.
- Files: `src/c/database_query.cpp:64-104`, `:138-195`
- Trigger: Any `query_integer` / `query_float` that matches zero rows — if the caller reads `*out_value` regardless of `has_value`, they see indeterminate data.
- Workaround: Always check `has_value` before reading `out_value`.

**`switch` with no trailing return in TimeProperties:**
- Symptoms: `src/binary/time_properties.cpp:49-67` (`datetime_to_int`) and `:70-86` (`add_offset_from_int`) end with `switch` statements covering every enumerator but without a post-switch return. If the input is corrupt (e.g., a value outside `TimeFrequency`, which is possible only via memcpy/FFI), control falls off the end — UB. Compiler warnings help here, but `-Werror` is off.
- Files: `src/binary/time_properties.cpp:49-86`
- Trigger: Corrupted or uninitialized `TimeFrequency` value.
- Workaround: None. Relies on callers constructing `TimeFrequency` correctly.

**`frequency_to_string` has the same fall-off issue:**
- Files: `src/binary/time_properties.cpp:12-25`
- Same pattern: `switch` on enum, no trailing return.

**Rollback error masking during import catch block:**
- Symptoms: `src/database_csv_import.cpp:474-482` and `:677-685` do `impl_->rollback(); execute_raw("PRAGMA foreign_keys = ON"); throw`. If `execute_raw` throws during PRAGMA restore, the original import error is lost (replaced by the PRAGMA error). `Impl::rollback` (`src/database_impl.h:324-335`) is already defensive ("Don't throw - rollback is often called in error recovery"), but the PRAGMA call is not.
- Files: `src/database_csv_import.cpp:474-482`, `:677-685`
- Trigger: Any low-level SQLite failure while restoring FK constraints after an import error.
- Workaround: None.

---

## Security Considerations

**SQL built by string concatenation in the core API:**
- Risk: Table names, column names, and collection names are interpolated directly into SQL in many places without any whitelist check beyond schema existence. Example: `src/database_time_series.cpp:82` `" FROM " + ts_table + " WHERE id = ? ORDER BY " + dim_col` where both `ts_table` and `dim_col` come from schema metadata. Schema metadata itself comes from the live SQLite catalog (`src/schema.cpp`), so the surface area is bounded, but if schema-bypassing paths are ever added (e.g., passing a user-supplied `collection` without `require_collection`), SQL injection becomes possible.
- Files: `src/database_impl.h:192-281`, `src/database_time_series.cpp:82-83,131,148,283-306`, `src/database_csv_import.cpp:378-388,606-620`
- Current mitigation: Parameterized values (`?`) are used for data; structure uses concatenation. `Impl::require_collection` and `Impl::require_column` whitelist against loaded schema.
- Recommendations: Add an explicit identifier-sanitization helper (e.g., reject anything non-`[A-Za-z0-9_]`) used at every interpolation site, for defense in depth.

**Lua scripts have direct database access:**
- Risk: `src/lua_runner.cpp:27-127` binds the entire `Database` usertype into Lua. A Lua script (often user-supplied in downstream tooling) has full CRUD + raw query capabilities. There is no sandbox beyond the standard library subset (`sol::lib::base, sol::lib::string, sol::lib::table` at `src/lua_runner.cpp:19`).
- Files: `src/lua_runner.cpp:14-165`
- Current mitigation: Standard-library surface is intentionally small — no `io`, `os`, `package`.
- Recommendations: Document that Lua scripts should never be loaded from untrusted sources; consider an opt-in "read-only" bind that omits mutating verbs.

**No input-length caps on user-provided strings:**
- Risk: `quiver::string::new_c_str` (`src/utils/string.h` via reference, see `database_helpers.h:1`) allocates exactly `str.size() + 1`. A pathological SQL or label value could trigger large allocations; not a vulnerability per se but worth noting that no length guard exists at the API boundary.
- Files: `src/c/database.cpp:11-30`, `src/c/database_query.cpp:42-103`
- Current mitigation: SQLite itself enforces row/column/statement size caps.
- Recommendations: Consider documenting maximum reasonable sizes in the public header.

**`.env` file not in repo (confirmed):**
- `ls .env*` returns nothing in the project root. No inline secrets observed.

---

## Performance Bottlenecks

**Binary subsystem hot-path: `unordered_map<string, int64_t>` dims parameter (~40% per CLAUDE.md):**
- Problem: `BinaryFile::read` (`src/binary/binary_file.cpp:106`) and `BinaryFile::write` (`:133`) take `const std::unordered_map<std::string, int64_t>& dims`. `calculate_file_position` (`:144-158`) accesses this map once per dimension via `dims.at(dimensions[i].name)` — string hashing + heap allocation for the key lookup. `validate_dimension_values` (`:184-236`) does another hash lookup per dimension.
- Files: `src/binary/binary_file.cpp:106, 133, 144, 184`
- Cause: Map construction allocates (1 `std::string` + 1 bucket) per entry; `.at()` triggers hashing + string comparison.
- Improvement path: Add `vector<int64_t>` overloads where values are passed in dimension declaration order, reducing `calculate_file_position` to a dot-product with precomputed strides (doc'd in CLAUDE.md under "Binary Performance Bottlenecks"). Not yet implemented.

**Binary subsystem hot-path: `validate_dimension_values` cost (~19% per CLAUDE.md):**
- Problem: Every `read`/`write` validates dimension count, name existence, bounds, and — if `number_of_time_dimensions > 1` — performs date arithmetic via `add_offset_from_int`/`datetime_to_int` (`src/binary/binary_file.cpp:208-235`). `add_offset_from_int` calls `std::chrono::floor<std::chrono::days>`, constructs `year_month_day`, and does calendar arithmetic. Called on every read/write.
- Files: `src/binary/binary_file.cpp:184-236`, `src/binary/time_properties.cpp:49-86`
- Cause: Unconditional validation, including expensive time-dimension consistency check, on every I/O.
- Improvement path: Make validation opt-in (parameter or separate entry point `read_unchecked` / `write_unchecked`) for hot iteration patterns where callers know the indices are correct (e.g., `next_dimensions()` walker). Not implemented.

**Binary subsystem hot-path: per-read `vector<double>` allocation (~3% per CLAUDE.md):**
- Problem: `BinaryFile::read` (`src/binary/binary_file.cpp:111`) allocates a fresh `std::vector<double> data(impl_->metadata.labels.size())` per call.
- Files: `src/binary/binary_file.cpp:111-131`
- Cause: Output buffer is not caller-provided.
- Improvement path: Add `read_into(buffer, dims)` overload writing into caller-provided storage. Documented in CLAUDE.md. Not implemented.

**CSV import per-row `sqlite3_prepare_v2`:**
- Problem: Each row in `database_csv_import.cpp:438` and `:672` calls `execute(insert_sql, params)` which re-prepares the same SQL. For an N-row CSV this is N prepare+finalize cycles.
- Files: `src/database_csv_import.cpp:390-473, 622-676`
- Cause: No cached-statement layer; `Database::execute` is general-purpose.
- Improvement path: Prepared-statement cache keyed by SQL hash, or a dedicated `execute_prepared(stmt_handle, params)` entry point.

**Time series `update_time_series_group` per-row prepare:**
- Problem: `src/database_time_series.cpp:180` calls `execute(insert_sql, params)` in a row loop inside a transaction. Same prepare-per-row issue.
- Files: `src/database_time_series.cpp:159-181`
- Cause: Same — no prepared-statement caching.
- Improvement path: Same as above.

**CSV import validation + data pass do double work:**
- Problem: `database_csv_import.cpp:315-367` runs a full validation pass (per-cell type check, FK existence, enum resolution) then `:390-439` runs the same logic again to build parameters. Date parsing (`parse_datetime_import`), FK map lookups, and enum resolution are all performed twice.
- Files: `src/database_csv_import.cpp:313-439`, `:562-673`
- Cause: Validation was layered on without refactoring the data path to reuse intermediates.
- Improvement path: Validation pass could produce a resolved cache of `(row_idx, col_idx) -> Value` that the data pass consumes directly.

**`fill_file_with_nulls` writes ~8 MB at a time:**
- Problem: `src/binary/binary_file.cpp:350-377` pre-fills new writer files with NaN in 1 MB-double (8 MB) chunks. For large files this is unavoidable but on very large grids (e.g., 480×500×31 example from CLAUDE.md = ~60 MB) this is I/O time users may not expect on `open_file('w')`.
- Files: `src/binary/binary_file.cpp:350-377`
- Cause: Partial writes into a sparse file would leave uninitialized regions in non-sparse filesystems; NaN-filling guarantees well-defined read behavior for untouched cells.
- Improvement path: Consider OS-level sparse-file support on supported filesystems (not portable).

**Quadratic `validate_no_duplicate_attributes` and `validate_foreign_keys`:**
- Problem: `src/schema_validator.cpp:224-291` iterates every collection, then every table, then every column of every group table. `src/schema_validator.cpp:294-361` nests foreign-key iteration similarly.
- Files: `src/schema_validator.cpp:224-361`
- Cause: Straightforward nested loops without precomputed indexes.
- Improvement path: Build a `column_name -> set<table_name>` index once; then duplicate detection becomes O(columns) instead of O(collections * tables * columns). Low priority — runs once at open.

---

## Fragile Areas

**Binary write registry (NOT thread-safe, NOT multi-process safe):**
- Files: `src/binary/binary_file.cpp:18` — `static std::unordered_set<std::string> write_registry;` is a single file-local static. `open_file` mutates it without synchronization at `:58, :86`. `Impl::~Impl` (`:36-43`) removes the entry on destructor.
- Why fragile:
  - **Not thread-safe**: Two threads calling `BinaryFile::open_file` simultaneously race on the `write_registry.count()` + `write_registry.insert()` sequence (lines 58-86). No mutex.
  - **Not multi-process safe**: Two OS processes have independent `write_registry` statics; the second process will open an already-locked file without warning, leading to TOCTOU file corruption.
  - **No cleanup on crash**: If the process crashes before `~Impl` runs, the `.qvr`/`.toml` files exist but the registry entry never mattered because a new process starts with an empty set.
  - CLAUDE.md acknowledges this: "Not thread-safe or multi-process safe." — confirmed by reading `src/binary/binary_file.cpp:18`, not `binary.cpp` (the CLAUDE.md comment has a stale filename reference).
- Safe modification: Only use `BinaryFile` from a single thread of a single process. Document this at the public header (`include/quiver/binary/binary_file.h`) — currently the header has no threading notes.
- Test coverage: No concurrency tests found in `tests/test_binary_file.cpp` or `tests/test_c_api_binary_file.cpp`.

**Windows DLL dependency chain for Dart:**
- Files: `bindings/dart/hook/build.dart:1-56`, `bindings/dart/test/test.bat` (not read but referenced by `CLAUDE.md`)
- Why fragile: `libquiver_c.dll` depends on `libquiver.dll` — both must be in PATH on Windows. On Linux the build sets `INSTALL_RPATH "$ORIGIN"` (`CMakeLists.txt:120-125`) which fixes discovery relative to the binary, but no equivalent mechanism exists for Windows loaders.
- Safe modification: Use `test.bat` which prepends `build\bin` to PATH. Direct invocation of Dart tests is brittle.
- Known fragility: `CLAUDE.md` says "When C API struct layouts change, clear `.dart_tool/hooks_runner/` and `.dart_tool/lib/` to force a fresh DLL rebuild." Dart's hook system caches the build output; stale caches bind against new struct layouts and corrupt memory.

**C API struct layout versioning:**
- Files: `include/quiver/c/options.h:18-50` (`quiver_database_options_t`, `quiver_csv_options_t`), `include/quiver/c/common.h:23-26` (`quiver_error_t`)
- Why fragile: None of the public C structs carry a version/ABI stamp. Any field addition or reordering silently breaks pre-built bindings (Dart hooks, Python wheels, Julia `libquiver_c` calls). `CLAUDE.md` explicitly warns that struct changes require clearing `.dart_tool/hooks_runner/` and `.dart_tool/lib/`.
- Safe modification: Always append fields to the end of structs; never reorder; increment `quiver_version()` string (`src/c/common.cpp:28-30`) so callers can detect; ideally introduce a `size_t struct_size` first field on new structs.
- Test coverage: None — no FFI binary-compatibility tests.

**DateTime native wrappers only in some bindings:**
- Files: `bindings/julia/src/database_read.jl:246-372`, `bindings/dart/lib/src/database_read.dart:795-938`, `bindings/python/src/quiverdb/database.py:320-338`. JS does NOT wrap DateTime — `bindings/js/src/composites.ts:28-29, 57, 87` explicitly notes "case 2: STRING, case 3: DATE_TIME" get the same treatment.
- Why fragile: The C++ layer stores `DATE_TIME` as TEXT (per `CLAUDE.md` schema rules). Wrappers in Julia/Dart/Python parse the TEXT into native `DateTime` objects; JS passes through the raw ISO8601 string. Application code that relies on homogeneous return types will break when moved across bindings.
- Safe modification: Add a `readScalarDateTimeById` wrapper in `bindings/js/src/read.ts` or document the behavior discrepancy loudly in JS README.
- Test coverage: Julia has explicit DateTime tests (`test_database_read.jl:242-252`); JS does not.

**Per-database-instance logger file handle:**
- Files: `src/database.cpp:43-88`
- Why fragile: `create_database_logger` creates a file sink at `{db_dir}/quiver_database.log` with `truncate=true` (second parameter to `basic_file_sink_mt`, `:72`). Opening two `Database` instances pointing to the same directory will cause the second one to truncate the first's log file. Each instance has a unique logger name (atomic counter `g_logger_counter` at `:19`) but they share the filesystem log.
- Safe modification: Either include the atomic counter in the filename, or use a rotating/append sink.
- Test coverage: No multi-instance logging tests found.

**Silent rollback on logger file-sink failure:**
- Files: `src/database.cpp:80-86`
- Why fragile: If the log file sink fails to open (readonly FS, permissions), code catches `spdlog_ex` and silently degrades to console-only. A caller running a daemon without a visible console will lose all logs without warning.
- Safe modification: Surface this as a warning to the returned logger itself or provide a `get_log_path()` API.

---

## Scaling Limits

**Time series `read_time_series_group` returns `vector<map<string, Value>>`:**
- Current capacity: Unbounded — SQLite can hold billions of rows.
- Limit: Each row is a `std::map<std::string, Value>` — 1 heap allocation per column per row for the map node, plus `std::string` allocations for keys. For a 10 M-row time series with 5 columns, that's 50 M map nodes + 50 M strings.
- Files: `src/database_time_series.cpp:55-112`
- Scaling path: Switch to columnar return (`map<string, vector<Value>>` with parallel column vectors). The C API already uses a columnar pattern (`CLAUDE.md` "Multi-Column Time Series" section mentions `convert_params()` style) but the C++ core still returns row-major.

**CSV import holds entire file in memory:**
- Files: `src/database_csv_import.cpp:256-689` uses `rapidcsv`'s `Document` which reads the entire CSV into memory.
- Limit: Available RAM. For multi-GB CSVs this becomes a problem.
- Scaling path: Use a streaming CSV reader and batched INSERTs.

**Binary `fill_file_with_nulls` blocks `open_file('w')`:**
- Files: `src/binary/binary_file.cpp:350-377`
- Limit: For a 100 GB binary file, NaN-filling consumes noticeable wall-clock time at open.
- Scaling path: Sparse files on supported filesystems; lazy fill on first read of untouched cell.

---

## Dependencies

**C++ (FetchContent, pinned):**
- `sqlite3-cmake` v3.50.2 (`cmake/Dependencies.cmake:4-8`) — recent.
- `tomlplusplus` v3.4.0 (`:11-15`) — current stable.
- `spdlog` v1.17.0 (`:18-22`) — current stable.
- `lua-cmake/v5.4.8.0` (`:28-32`) — current.
- `sol2` v3.5.0 (`:36-39`) — current.
- `rapidcsv` v8.92 (`:43-46`) — current.
- `argparse` v3.2 (`:49-53`) — current.
- `googletest` v1.17.0 (`:57-63`) — current.

All pinned by git tag via `FetchContent_Declare`. Dependabot is configured (`.github/dependabot.yml`) only for `github-actions`, NOT for C++ FetchContent. Manual updates required.

**Python (`bindings/python/pyproject.toml`):**
- `cffi>=2.0.0` — loose, will pick up patches.
- `requires-python = ">=3.13"` — aggressive floor; excludes 3.12 LTS users.
- `cibuildwheel` builds only `cp313-win_amd64` + `cp313-manylinux_x86_64` — no macOS, no aarch64 Linux, no older Python.
- Dev: `pytest>=8.4.1`, `ruff>=0.12.2`, `dotenv>=0.9.9`.

**Julia (`bindings/julia/Project.toml`):**
- `CEnum = "0.5"` — current.
- `Dates = "1"` — stdlib.
- `julia = "1.11"` — aggressive floor; Julia 1.10 LTS excluded.
- `Manifest.toml` committed (`bindings/julia/Manifest.toml`) which `CLAUDE.md` notes must be deleted for version conflicts.

**Dart (`bindings/dart/pubspec.yaml`):**
- `sdk: ^3.10.0` — aggressive floor (Dart 3.10 is very recent).
- `code_assets: ^1.0.0`, `ffi: ^2.1.0`, `hooks: ^1.0.0`, `native_toolchain_cmake: ^0.2.2`.
- `ffigen: ^11.0.0` (dev).
- `pubspec.lock` committed.

**JS (`bindings/js/deno.json`):**
- Target: Deno v2.7.x (from `.github/workflows/ci.yml:270`). No explicit dep pinning in `deno.json`; uses Deno's FFI + standard test.
- `deno.lock` committed.

**At risk:**
- `FetchContent` dependencies are pinned but not auto-updated. A long-lived dormant repo would miss SQLite security patches silently.
- Python 3.13-only floor excludes LTS users; downstream consumers pinned to 3.12 cannot install the wheel.
- Dart SDK `^3.10.0` will break for any developer still on Dart 3.9.

---

## Missing Critical Features

**No FK-guard RAII (see Tech Debt):**
- Problem: CSV import disables FK manually with try/catch-based restore.
- Blocks: Safe concurrent use of `import_csv` with FK-relying writes.

**No prepared-statement caching (see Tech Debt):**
- Problem: Every write re-parses SQL.
- Blocks: Large imports at acceptable throughput.

**No `read_into` buffer overload on `BinaryFile` (documented in CLAUDE.md):**
- Problem: Each read allocates.
- Blocks: Zero-allocation hot loops over large grids.

**No `vector<int64_t>` dims overload on `BinaryFile` (documented in CLAUDE.md):**
- Problem: Hash-map hot path.
- Blocks: 40% speedup documented in CLAUDE.md.

**No Python/JS shell script equivalents for build/test:**
- Files: `scripts/*.bat` — no `*.sh` counterparts for `build-all`, `test-all`, `clean-all`, `format`, `tidy`, `generator`, `test-wheel*`.
- Problem: Linux/macOS developers cannot use the scripts directly; must replicate manually.
- Mitigation: `.github/workflows/ci.yml:30-45` replicates the CMake commands per-platform, but this duplicates intent.
- Blocks: Smooth Linux/macOS developer onboarding.

**No WAL / journal_mode / synchronous tuning on SQLite open:**
- Files: `src/database.cpp:102-117` opens with only `SQLITE_OPEN_READONLY` or `SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE` flags. Only `PRAGMA foreign_keys = ON` is applied — no `journal_mode`, `synchronous`, `cache_size`, `temp_store`, `mmap_size`, `busy_timeout`, or `locking_mode` tuning.
- Problem: Default SQLite journal mode (`DELETE`) and `synchronous = FULL` is safe but slow for bulk writes. No threading mode is specified (uses SQLite's compile-time default).
- Blocks: Users cannot opt into WAL for better concurrent-reader throughput.

**No way to disable FK enforcement at `DatabaseOptions` level:**
- Files: `include/quiver/options.h:20-23` — only `read_only` and `console_level`.
- Problem: Users importing large data may want to disable FK enforcement during their own bulk pipeline, but no public API exposes this. `execute_raw("PRAGMA foreign_keys = OFF")` is a workaround but not documented.

**Partial migration rollback not tested:**
- Files: `src/database.cpp:334-355` (`migrate_up`) wraps each migration in its own transaction and rolls back on failure. `tests/test_migrations.cpp` does not test the case where migration N succeeds but migration N+1 fails — `CLAUDE.md` prompt specifically calls this out as a concern.
- Blocks: Undetected regression risk in migration atomicity.

---

## Test Coverage Gaps

**Concurrency / thread safety:**
- What's not tested: `BinaryFile::open_file` concurrent invocations, logger file-sink contention, thread-local `g_last_error`, `g_logger_counter`.
- Files with zero concurrency tests: `tests/test_binary_file.cpp`, `tests/test_c_api_binary_file.cpp`, `tests/test_c_api_database_lifecycle.cpp`.
- Risk: Races could corrupt `write_registry` or cause log file truncation in production multi-threaded hosts.
- Priority: Medium (CLAUDE.md documents the project as single-thread-use, but the documented assumption is not validated by tests).

**Multi-process safety for binary files:**
- What's not tested: Two processes opening the same `.qvr` file, one writer + one reader or two writers.
- Risk: Silent file corruption.
- Priority: Low (the project documents this as unsupported, but there is no test to confirm the failure mode is sane).

**Partial migration failures:**
- What's not tested: Migration 2 fails after migration 1 committed — is the DB left at version 1? Tests in `tests/test_migrations.cpp` cover Migration class semantics and simple happy-path `migrate_up`, not failure semantics.
- Files: `tests/test_migrations.cpp:1-272`, `tests/test_issues.cpp` only tests issue52 (`:1-18`); issue70 has a schema (`tests/schemas/issues/issue70/1/up.sql`) but no test case.
- Risk: Migration atomicity regressions unnoticed.
- Priority: High.

**C API struct layout / ABI stability:**
- What's not tested: No test verifies the size or layout of `quiver_database_options_t`, `quiver_csv_options_t`, `quiver_scalar_metadata_t`, `quiver_group_metadata_t`, `quiver_binary_file_t`, `quiver_binary_metadata_t`, `quiver_dimension_t`, `quiver_time_properties_t`.
- Risk: Struct changes silently break pre-built bindings.
- Priority: High (CLAUDE.md acknowledges this as a known fragility).

**C API error-path coverage:**
- What's partially tested: `tests/test_c_api_database_lifecycle.cpp` (523 lines) covers happy path + some errors. Not every function's `QUIVER_ERROR` return path is exercised.
- Risk: Regressions in `QUIVER_REQUIRE` macro or `quiver_set_last_error` contract.
- Priority: Medium.

**Python / JS binding parity:**
- What's not tested: The composite wrappers in Python (`read_vector_group_by_id`, `read_set_group_by_id`) have tests in `bindings/python/tests/test_database_read_vector.py:95-124` and `test_database_read_set.py:73-81` — good. But JS has `readScalarsById/readVectorsById/readSetsById` only, with no DateTime-aware variants, no `readVectorGroupById/readSetGroupById`, and no `queryDateTime`.
- Files: `bindings/js/src/composites.ts:1-98`, `bindings/js/test/composites.test.ts`.
- Risk: JS users silently get string dates where Julia/Dart/Python give native dates.
- Priority: Medium.

**Benchmark regression tracking:**
- What's not tested: `tests/benchmark/benchmark.cpp` is never run in CI. No historical trend data.
- Risk: A prepare-statement refactor (see Tech Debt) or a regression in the hot paths could land unnoticed.
- Priority: Medium (benchmark exists but is dormant).

**Schema-validator edge cases:**
- What's tested: `tests/test_schema_validator.cpp` + `tests/schemas/invalid/*.sql` cover duplicate attributes, missing Configuration, label constraints, vector index, set unique, FK actions.
- What's not tested: Time-series tables with missing `date_` column prefix (the prefix convention is documented but `SchemaValidator::validate_time_series_table` is empty; only `validate_time_series_files_table` exists). `src/schema_validator.cpp:35-39` has no time-series-data validation branch, only files-table validation.
- Risk: A malformed time-series table could slip past validation.
- Priority: Medium.

**Binary time_properties error paths:**
- What's not tested: The "YEARLY frequency extraction not implemented" and "WEEKLY frequency extraction not implemented" throws in `src/binary/time_properties.cpp:53-60` have no direct test — they are only reachable via invalid `TimeProperties` construction.
- Risk: If a future refactor allows these paths to be hit, users see generic `invalid_argument`.
- Priority: Low.

---

## Platform Portability

**Build/test scripts are Windows-only (.bat):**
- Files: `scripts/build-all.bat`, `scripts/test-all.bat`, `scripts/clean-all.bat`, `scripts/format.bat`, `scripts/generator.bat`, `scripts/test-wheel.bat`, `scripts/test-wheel-install.bat`, `scripts/tidy.bat`. No `*.sh` equivalents.
- Partial: `bindings/dart/test/test.sh` + `bindings/julia/test/test.sh` exist; Python and JS bindings only ship `.bat` for tests.
- Current mitigation: `.github/workflows/ci.yml` duplicates CMake commands per-OS and does not call the scripts.
- Impact: Non-Windows developers cannot use `scripts/build-all.bat` directly; must read it and translate.

**Python loader Windows-specific behavior:**
- Files: `bindings/python/src/quiverdb/_loader.py:35-38`
- Windows-only branch: `os.add_dll_directory(str(_LIBS_DIR))` + kept alive via `_dll_dir_handle` global. Linux/macOS rely on RPATH `$ORIGIN` set in `CMakeLists.txt:120-125`.
- Dev-mode PATH dependency: On Windows, `libquiver.dll` must be on PATH for the dev-mode `ffi.dlopen("libquiver")` call (`:50-51`) to succeed. `bindings/python/tests/test.bat` prepends `build/bin/` to PATH; a bare `pytest` invocation fails.

**Dart FFI hook selects generator per OS:**
- Files: `bindings/dart/hook/build.dart:19-23` — `OS.windows => Generator.ninja`, `OS.macOS => Generator.xcode`, default `Ninja`. Linux users need Ninja installed.
- Fragility: The `HAVE_GNU_STRERROR_R_EXITCODE` cross-compile shim (`:35-36`) is Linux-specific glibc handling.

**CI runs all three OSes:**
- `.github/workflows/ci.yml:17` `matrix.os: [ubuntu-latest, windows-latest, macos-latest]` — good cross-platform coverage at the C++ + CMake + CTest level.
- Julia, Dart, Python, Deno coverage jobs run on `ubuntu-latest` only — binding tests on Windows/macOS are NOT run in CI. `scripts/build-all.bat` and the `.bat` test scripts are effectively unexercised outside of developers' local machines.

---

## TODO / FIXME / HACK Markers

Grep for `TODO|FIXME|HACK|XXX` across the codebase finds **zero** genuine markers. Matches returned are false positives:
- `QUIVER_LOG_DEBUG = 0` in `include/quiver/c/options.h:11` and mirrors in bindings — an enum value, not a marker.
- `// Type validation regression tests (BUG-01)` in `tests/test_database_update.cpp:922` — a historical regression marker, not an open bug.
- `options.console_level = QUIVER_LOG_DEBUG;` in `tests/test_c_api_database_lifecycle.cpp:99` — a test fixture.

Codebase is free of TODO/FIXME/HACK comments. This is unusual for a WIP project and suggests either (a) high discipline, or (b) tracked issues live in the GitHub issue tracker rather than in source (the `tests/schemas/issues/` directory points to GitHub issues #52 and #70 — issue52 has a test, issue70 does not).

---

## Dependencies at Risk

**None of the C++ dependencies are on known-vulnerable versions.** SQLite 3.50.2, spdlog 1.17.0, toml++ 3.4.0, Lua 5.4.8, sol2 3.5.0, rapidcsv 8.92, argparse 3.2, googletest 1.17.0 are all recent releases. No Dependabot coverage for C++ FetchContent means future CVEs require manual review.

**Python `cibuildwheel` matrix limited:**
- Files: `bindings/python/pyproject.toml:22`
- Risk: Wheels only built for `cp313-win_amd64` and `cp313-manylinux_x86_64`. macOS users + aarch64 Linux users + Python 3.12 users cannot install from the wheel.
- Impact: Forces source build (requires CMake + compiler).
- Migration plan: Expand matrix to `cp312-*`, `cp313-macosx_*`, `cp313-manylinux_aarch64`.

**Dart SDK floor `^3.10.0` is very recent:**
- Files: `bindings/dart/pubspec.yaml:5-6`
- Risk: Users on Dart 3.9 or older Flutter stable channels cannot use this binding.
- Impact: Limited install base for now.
- Migration plan: Only lower the floor if `code_assets` / `hooks` / `native_toolchain_cmake` support earlier SDKs.

**Julia `1.11` floor excludes LTS `1.10`:**
- Files: `bindings/julia/Project.toml:13`
- Risk: Downstream packages that pin Julia 1.10 cannot depend on Quiver.
- Impact: Narrower adoption.
- Migration plan: Relax to `julia = "1.10"` if stdlib features used are compatible.

---

## Error Handling Gaps

**Every C API function IS using QUIVER_REQUIRE for pointer validation — with documented exceptions:**
- Verified across `src/c/*.cpp`: 179 `QUIVER_REQUIRE` invocations. Every non-trivial function checks its pointers.
- Exceptions (intentional):
  - `quiver_database_close` (`src/c/database.cpp:32-35`) — `delete db` is no-op on null but does not reject non-null-garbage pointers. Returns `QUIVER_OK` unconditionally.
  - `quiver_binary_file_close` (`src/c/binary/binary_file.cpp:49-52`) — same pattern.
  - `quiver_database_free_string` (`src/c/database_read.cpp:80-83`) — `delete[] str` is no-op on null; no `QUIVER_REQUIRE`.
  - `quiver_binary_file_free_string` (`src/c/binary/binary_file.cpp:139-142`) — same.
  - `quiver_binary_file_free_float_array` (`:144-147`) — same.
  - `quiver_binary_metadata_free_string` (`src/c/binary/binary_metadata.cpp:260-263`) — same.
  - `quiver_version`, `quiver_get_last_error`, `quiver_clear_last_error`, `quiver_database_options_default`, `quiver_csv_options_default` — intentionally lack error returns (utility functions).

**All try/catch blocks map `std::exception` to `quiver_set_last_error(e.what())` + `return QUIVER_ERROR`:**
- Verified at `src/c/database.cpp`, `src/c/database_read.cpp`, `src/c/database_query.cpp`, `src/c/database_create.cpp`, `src/c/database_metadata.cpp`, `src/c/database_time_series.cpp`, `src/c/binary/*.cpp`.
- `std::bad_alloc` is caught separately in the handle-creation functions (`quiver_database_open`, `quiver_database_from_schema`, `quiver_database_from_migrations`, `quiver_element_create`, `quiver_binary_file_open_read/write`, `quiver_binary_metadata_create`) with a distinct "Memory allocation failed" message.

**Silent sqlite3_exec return code (see Tech Debt #6 above).**

**`Impl::rollback` deliberately swallows errors during recovery:**
- Files: `src/database_impl.h:324-335`
- Documented as intentional: "Don't throw — rollback is often called in error recovery". If rollback itself fails, only a log line is emitted. This can mask SQLite-level corruption.

---

## Schema / Migration Fragility

**Configuration table enforcement:**
- `src/schema_validator.cpp:45-49` — `SchemaValidator::validate_configuration_exists` throws if the `Configuration` table is absent. Triggered during `Impl::load_schema_metadata` (`src/database_impl.h:286-291`) which runs after `apply_schema` / `migrate_up`.
- Enforcement point: **after** the schema is applied / migration is committed. If a schema is missing Configuration, the migration's transaction has already committed the tables before `load_schema_metadata` validates — see `src/database.cpp:333-358`:
  ```
  impl_->begin_transaction();
  try {
      execute_raw(up_sql);
      set_version(...);
      impl_->commit();   // <-- commits THEN validates
      ...
  }
  // After the loop:
  impl_->load_schema_metadata();  // <-- validation happens here
  ```
- Consequence: A migration that creates a schema without Configuration will commit successfully, then the next step (`load_schema_metadata`) throws. The DB is left at the new version but unusable.
- Test coverage: `tests/schemas/invalid/no_configuration.sql` + `tests/test_schema_validator.cpp:26` test `from_schema` rejection. No test verifies the per-migration case.

**Migration file discovery tolerates non-numeric directory names:**
- `src/migrations.cpp:22-33` catches `std::exception` from `std::stoll(dirname, &pos)` and silently skips non-numeric directories. This means typos like `00_initial` (leading zero okay, stoll parses 0 and then pos != dirname.size) will be silently skipped rather than error.
- Files: `src/migrations.cpp:12-36`
- Impact: Silent failure mode — developers may create a migration named `v1` and wonder why it's ignored.

**Partially applied migration:**
- `Database::migrate_up` (`src/database.cpp:333-358`) wraps each migration in its own transaction. If migration N+1 fails, migration N is already committed. There is no cross-migration rollback.
- Files: `src/database.cpp:333-358`
- Consequence: Database is left at version N with schema from N applied, even if the user wanted the entire batch.
- Test coverage: None (see Test Coverage Gaps above).

---

## FFI Struct Layout Risk

**No versioning guard on any C struct:**
- `quiver_database_options_t` (`include/quiver/c/options.h:18-21`): two fields — `int read_only`, `quiver_log_level_t console_level`. No size/version stamp.
- `quiver_csv_options_t` (`:42-50`): 7 pointer/size fields. No version.
- `quiver_scalar_metadata_t` / `quiver_group_metadata_t` / `quiver_dimension_t` / `quiver_time_properties_t`: same — plain structs with no ABI header.

**Dart cache-clearing requirement:**
- `CLAUDE.md` instructs: "When C API struct layouts change, clear `.dart_tool/hooks_runner/` and `.dart_tool/lib/` to force a fresh DLL rebuild."
- Mechanism: Dart's hook-based build caches DLL outputs. If the C header changes but the cache isn't invalidated, Dart's pre-generated FFI struct definitions (in `bindings/dart/lib/src/ffi/bindings.dart`, generated by ffigen) will be out of sync with the DLL's actual layout.
- Symptom: Silent memory corruption. Fields read at wrong offsets, variable-size structs truncated.

**Python CFFI is somewhat safer:**
- `bindings/python/src/quiverdb/_c_api.py` and `_declarations.py` have hand-maintained cdef strings that must match the headers. A struct-layout mismatch throws at first use rather than corrupting memory silently — but only because CFFI re-reads the cdef at import time.
- Files: `bindings/python/src/quiverdb/_c_api.py`
- Mitigation: Regenerate via `bindings/python/generator/generator.bat` when headers change.

**Julia FFI caching:**
- Julia's FFI (`bindings/julia/src/c_api.jl`) is regenerated via `bindings/julia/generator/generator.bat` from the C headers. If stale, same risk as Dart.

---

## Summary

The codebase is clean of TODO/FIXME markers and enforces disciplined error handling patterns. The largest open concerns are:

1. **Documented-but-unimplemented performance optimizations** in the binary subsystem (`read_into`, `vector<int64_t>` overloads) — 60%+ of hot-path CPU.
2. **Per-row SQLite statement preparation** in all bulk-write paths — affects CSV import, time-series updates, group inserts.
3. **No thread or multi-process safety** for the binary write registry; documented but not enforced at the API level.
4. **No ABI versioning** on C API structs, making binding cache invalidation a manual process.
5. **Benchmark drift risk** because the regression-detection executable is never invoked automatically.
6. **Test coverage gaps** around concurrency, partial migration failure, and C API struct layout.
7. **Non-Windows developer friction** due to .bat-only scripts for Python and JS.

*Concerns audit: 2026-04-18*
