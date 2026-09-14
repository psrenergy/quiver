# Codebase Concerns

**Analysis Date:** 2026-09-14

## Version Mismatch

**CHANGELOG.md disagrees with all five version manifests:**
- Issue: `CHANGELOG.md` line 16 declares `## [0.10.4] — unreleased`, but `CMakeLists.txt`, `bindings/python/pyproject.toml`, `bindings/js/package.json`, `bindings/dart/pubspec.yaml`, and `bindings/julia/Project.toml` all declare version `0.10.6`
- Files: `CHANGELOG.md` (line 16), `CMakeLists.txt` (line 3), `bindings/python/pyproject.toml` (line 2), `bindings/js/package.json` (line 2), `bindings/dart/pubspec.yaml` (line 4), `bindings/julia/Project.toml` (line 3)
- Impact: Unclear which is the true current version; release automation might behave unexpectedly
- Fix approach: Update `CHANGELOG.md` heading to `## [0.10.6] — unreleased` and verify the compare link (currently `v0.11.0`)

## Known Open Defects

**Nine documented GitHub issues tracking NULL handling, group readers, and time-series semantics:**

### Vector & Set Readers (Issues #248, #237)
- Problem: `read_vector_*` / `read_set_*` bulk readers drop NULL cells instead of preserving them positionally, and skip elements with no rows in the group — causing misalignment with `read_element_ids`
- Files: `src/database_internal.h` (template `read_grouped_values_all`), `src/database_read.cpp`
- Impact: Multi-column group reads misalign when nulls are present; callers cannot reliably zip columns with element IDs
- Note: This is a root design decision documented in `CLAUDE.md` — the scalar readers preserve NULLs; the grouped readers drop them. Issue #249 requests alignment.

### Group Reader Misalignment (Issue #249)
- Problem: `read_vector_group_by_id` / `read_set_group_by_id` return rows (C++) but are composed differently per binding (Julia/Python compose per-column, Dart binds natively), leading to inconsistent NULL handling
- Files: `src/database_read.cpp`, `bindings/julia/src/database_read.jl`, `bindings/python/src/database.py`, `bindings/dart/lib/src/ffi/bindings.dart`
- Impact: Developers unaware of the per-binding asymmetry may get misaligned data
- Note: Deliberate architectural decision documented in `CLAUDE.md` (Dart gets native C++, Julia/Python compose). Design review is open.

### Time-Series Row Reads (Issue #250)
- Problem: `read_time_series_row` returns sentinel values (0/0.0) for integers and floats when no data exists, instead of NULL/None/nil
- Files: `src/database_time_series.cpp`, cross-layer bindings
- Impact: No way to distinguish "no data for this element" from "data is legitimately zero"
- Fix approach: Return a nullable Value/optional from the C++ core

### Partial Group Writes (Issue #252)
- Problem: `update_element` with an array attribute updates that array in *every* matching group table if the column name appears in multiple groups (fan-out). See `UpdateElementSharedColumnNameWritesEveryMatchingGroup` test
- Files: `src/database_impl.h`, `tests/schemas/valid/relations.sql` (intentional test case with shared `parent_ref` across a vector and set group)
- Impact: Silent data mutation in unintended groups; a typo'd column name rewrites related data
- Fix approach: Deep fix is to reject `matches.size() > 1` when `delete_existing` is true. Documented as blocking only `bindings/julia/test/test_helper_maps.jl` (the non-destructive `create_element!` half), so not a blocker
- Note: Documented in root `CLAUDE.md` as "Not yet fixed"

### Time-Series Write Semantics (Issue #234)
- Problem: `upsert_time_series_row` and `update_time_series_files` replace the whole row instead of writing only the passed columns, inconsistent with sparse-update expectations
- Files: `src/database_time_series.cpp`
- Impact: Cannot do sparse updates; must construct complete row on every write

### CSV Import Issues (Issue #180)
- Status: Open, no recent activity; check for blocker status

### DateTime Issues (Issue #176)
- Problem: Setting date_time in `create_element` produces wrong results (reported 2026-08-26)
- Files: Cross-layer, likely binding-level parsing or core-level validation
- Impact: DateTime column data corruption on create
- Related: Dart DateTime reader also fixed in 0.10.4 (CHANGELOG, line 28-32) for DST gap handling

## Hand-Maintained FFI Symbol Table (No Generator)

**JavaScript binding has zero automation:**
- Problem: `bindings/js/src/loader.ts` (329 lines) hand-maintains the C API symbol definitions with no code generator
- Files: `bindings/js/src/loader.ts` (lines 24-120+), `bindings/js/CLAUDE.md` (documented explicitly)
- Impact: Each C API addition must be manually added to the symbol map; easy to forget, has caused drift in the past
- Context: Julia, Dart, Python all have generators (`bindings/*/generator/generator.bat`). JS alone does not. Root cause: Bun FFI is niche and has no existing binding generator ecosystem
- Mitigation: `bindings/js/test/lua-api-sync.test.ts` provides a sync check for Lua bindings, but does not cover C API symbol coverage
- Note: NOT covered by the root `CLAUDE.md` "Do Not Fix" list (which covers Dart/Python FFI boilerplate style, `tests/sandbox`, the Bun FFI workarounds, JS lint debt, and the location of the Lua reference). `bindings/js/CLAUDE.md` calls this "the drift-prone spot: check it whenever a new C function exists in other bindings but not here" - acknowledged as a hazard, not blessed.

## Duplicated CSV Parsing

**Two independent CSV parsers with different capabilities:**
- `src/database_csv_export.cpp` (308 lines): Uses `rapidcsv` library with full quote/escape handling
- `src/binary/csv_converter.cpp` (332 lines): Hand-rolled CSV parser with **no quote handling**, only comma-separated values
- Files: `src/database_csv_export.cpp`, `src/binary/csv_converter.cpp`
- Impact: Binary subsystem cannot round-trip CSV files with quoted fields or embedded commas
- Scaling: If CSV format needs enhancement (e.g., quote handling), both paths must be updated independently
- Fix approach: Unify on `rapidcsv` for the binary subsystem or add quote handling to the hand-rolled parser

## Statement Preparation Per Row

**Group inserts prepare and finalize SQLite statements per row:**
- Problem: `insert_rows_into_group_table` in `src/database_impl.h` calls `db.execute(sql, parameters)` inside a loop (one call per row added to the group)
- Files: `src/database_impl.h` (group insert loop), `src/database.cpp` (execute → sqlite3_prepare_v2 + sqlite3_finalize)
- Impact: Performance cost of statement preparation is multiplied by the row count in a group; micro-batching could reduce this
- Context: This is **load-bearing by design** (validated in `src/CLAUDE.md`): validation must run before the DELETE, and `TransactionGuard` is no-op inside caller-owned transactions, so early validation prevents leaving cleared groups on error
- Note: Documented design, not a bug; stated as acceptable given the precondition that `Database::execute` is currently the pattern used internally for all parameterized statements

## Lua Whole-Group Readers Not Bound

**Lua deliberately does not expose multi-column group readers:**
- Problem: `read_vector_group_by_id` / `read_set_group_by_id` are not bound in Lua; scripts must read each column independently via `db:read_vectors_by_id` / `db:read_sets_by_id`
- Files: `src/lua_runner.cpp` (no binding), `bindings/js/src/lua-api.ts` (LUA_DB_API_REFERENCE, documented)
- Impact: Row alignment across a nullable group requires manual scripting; silently misaligned reads possible if columns have sparse NULLs
- Rationale: Documented design decision — Lua null-dropping readers don't align anyway when nullable columns are present
- Mitigation: Agent-facing Lua reference (`bindings/js/src/lua-api.ts` line 708+) documents this explicitly

## Lua File Sandboxing Edge Case

**`load` function stays enabled while `dofile`/`loadfile` are disabled:**
- Problem: Lua scripts can use string-form `load(code_string)` to execute arbitrary Lua code, though the safer file I/O functions are sandboxed
- Files: `src/lua_runner.cpp` (lines 319-323), where only `dofile` and `loadfile` are nil'd
- Impact: A string-containing code injection could execute arbitrary logic, though the attacker must provide the string inline (not load it from the filesystem)
- Rationale: String-form `load` is intentionally kept; scripts legitimately need it for dynamic logic
- Mitigation: Documented design; no user code should trust script input directly

## CI Coverage Gaps

**Test coverage not yet complete across all bindings:**
- Context: `.github/CLAUDE.md` documents that four coverage flags upload to Codecov (`cpp`, `julia`, `dart`, `python`); JavaScript coverage not yet automated
- Files: `.github/workflows/ci.yml`, `.github/CLAUDE.md` (lines 10, 26-50 address glibc/platform concerns, not coverage)
- Impact: JS binding test coverage is not tracked; regressions in JS code paths may go unnoticed
- Scaling: As Bun/FFI test surface grows, lack of automated coverage metrics increases risk

## Dart Deployment Target Floor

**macOS builds now require deployment target ≥ 13.3:**
- Problem: `std::to_chars` (used in `database_csv_export.cpp` and `lua_runner.cpp`) is unavailable in libc++ below 13.3
- Files: `cmake/Platform.cmake`, `database_csv_export.cpp` (floating-point CSV export), `lua_runner.cpp` (JSON encoding)
- Impact: macOS builds before 13.3 will fail; published natives may silently require newer OS versions if builder environment is newer
- Mitigation: `cmake/Platform.cmake` now enforces and documents this floor; native S3 build uses fixed-digest CentOS 7 image for Linux glibc 2.17 floor
- Status: Addressed in 0.10.4; documented as intentional

## Lua Renderer JSON Output Limits

**LuaRunner JSON output has hard caps:**
- Problem: `LuaRunner::run` JSON output is capped at 64 MiB size and 32 levels of nesting
- Files: `src/lua_runner.cpp` (anonymous `append_json*` functions, limits in comments)
- Impact: Scripts returning large structured data (bulk reads transposed to nested JSON) may silently truncate
- Mitigation: Documented in comments; users should be aware when returning large result sets
- Note: Designed to prevent runaway memory; 64 MiB is a reasonable cap for single-call results

## Potential Type Coercion Issues

**Boolean-to-integer coercion in Lua table parsing varies by path:**
- Problem: Lua boolean cells in mixed-type arrays may behave differently in release vs debug builds
- Files: `src/lua_runner.cpp` (lines 360-373, `lua_table_to_vector<T>`)
- Impact: Debug builds validate every cell; release builds depend on sol2's `SOL_SAFE_GETTER` (off by default in release) and could silently coerce `true` → 0 in release only
- Mitigation: `sol2` is configured with `SOL_SAFE_NUMERICS=1` and `SOL_SAFE_FUNCTION=1` (load-bearing); `SOL_SAFE_GETTER` is intentionally left at default. Code comments explain this explicitly
- Note: Pre-existing issue fixed by adding `lua_table_to_vector` validation; load-bearing by design

## Intentionally Documented Non-Issues (NOT Concerns)

**The following are deliberate design decisions, not bugs:**

- **Bun FFI workarounds** (`bindings/js/CLAUDE.md`): TypedArray pinning, `"pointer"` for buffer args, precomputed `ptr()` number timing — all load-bearing and documented as such
- **Binary hot-path indexed overloads** (`src/CLAUDE.md`): Map-based `dims` parameter only; indexed forms dropped for perf — profiling numbers justify this
- **`LUA_DB_API_REFERENCE` location** (`bindings/js/src/lua-api.ts`): Intentional constant, not an imported `.md` file — no FFI, inlines into consumer binary, optimizes for build-time inclusion
- **Lua whole-group readers omission**: Intentional; documentation acknowledges the NULL-dropping semantics make row alignment impossible anyway
- **Array fan-out on shared column names**: Documented and pinned by test; deliberate architectural limitation affecting only Julia's `helper_maps.jl`

---

*Concerns audit: 2026-09-14*
