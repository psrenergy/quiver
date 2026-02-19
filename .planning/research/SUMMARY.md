# Project Research Summary

**Project:** Multi-column time series C API interface redesign
**Domain:** C FFI interface design with multi-language bindings (Julia, Dart, Lua)
**Researched:** 2026-02-12
**Confidence:** HIGH

## Executive Summary

The Quiver library already supports multi-column time series at the C++ layer via `vector<map<string, Value>>`, but the C API bottleneck hardcodes a two-column assumption (dimension + single value). This prevents bindings from writing multi-column schemas like `(id, date_time, temperature, humidity, status)` despite full support existing in the database layer. The redesign extends the C API to pass N typed columns across the FFI boundary using columnar parallel arrays with type tags—a pattern already proven in this codebase's parameterized query implementation.

The recommended approach uses columnar typed arrays: `column_names[]`, `column_types[]`, and `column_data[]` where each column is a homogeneous typed array. This requires zero new dependencies, reuses the existing `quiver_data_type_t` enum, and follows the exact marshaling pattern from `database_query.cpp`. Both read and update must be redesigned together (a write-only API that cannot read back its own data is unusable). The bindings adopt metadata-driven marshaling, querying schema information to discover column names and types at runtime rather than hardcoding assumptions.

The critical risk is partial allocation leaks on the read side: allocating N output arrays where failure at column K leaks columns 0..K-1. Prevention requires atomic allocation (all or nothing) before writing to output parameters. Type safety is the other major concern—void* column data with mismatched type tags causes silent corruption. Validation against schema metadata before casting eliminates this. The migration must be incremental: add new C API functions alongside old ones to keep all 1,213 tests passing throughout the process.

## Key Findings

### Recommended Stack

**Use the columnar typed-arrays pattern from the existing parameterized query implementation.** This approach requires no new types, no new dependencies, and follows established FFI precedent within the codebase.

**Core technologies:**
- **Columnar parallel arrays with type tags** — Same pattern as `convert_params()` in `database_query.cpp`, extended from 1D parameters to 2D tables. Already proven in Julia and Dart bindings.
- **Existing `quiver_data_type_t` enum** — Reused directly for column type identification (INTEGER=0, FLOAT=1, STRING=2). No new enum values needed.
- **Metadata-driven marshaling** — Bindings call `get_time_series_metadata()` to discover column names and types at runtime, then marshal accordingly. Eliminates hardcoded column name assumptions.
- **Alloc/free co-location in `database_time_series.cpp`** — Read allocates N typed arrays; matching free function iterates types to deallocate correctly (string columns require per-element cleanup, numeric columns single delete[]).

**Why this approach:**
- Zero new dependencies (uses only standard C, C++20, existing quiver types)
- Columnar layout matches SQL (each column maps to one C array)
- FFI-friendly in Julia (`Ptr{Cvoid}`) and Dart (`Pointer<Void>`)
- O(columns) array allocations instead of O(rows*columns) struct constructions
- Cache-friendly for columnar data processing

**What does NOT change:**
- C++ `Database::update_time_series_group` signature — already accepts `vector<map<string, Value>>`
- Lua bindings — call C++ directly via sol2, already support N columns
- CMake build configuration — no new source files needed

### Expected Features

**Must have (table stakes):**
- **N typed value columns through C API** — The C++ layer already handles N columns; the C API must not be the bottleneck. Without this, multi-column time series schemas are unusable from bindings.
- **Matching read-side redesign** — If update supports N columns but read returns single column, data is write-only. Both must ship together.
- **Type safety for each column** — Schema-defined types (INTEGER, REAL, TEXT) must be preserved. Current API loses INTEGER precision (casts to double) and cannot handle TEXT value columns at all.
- **Column name association** — Caller specifies which array belongs to which column. Positional approaches break when schema column order changes.
- **Empty row set (clear)** — Calling with 0 rows must delete all time series data for that element, matching current behavior.
- **Free function for read results** — New read signature returns new data structures; must provide matching cleanup.

**Should have (differentiators):**
- **Metadata-driven validation in C API** — Validate column names and types against `get_time_series_metadata()` at FFI boundary with clear error messages.
- **Columnar input (arrays of columns, not arrays of rows)** — Matches scientific/time-series data's natural memory layout. Simpler for FFI than row-of-maps.
- **Binding-level kwargs decomposition (Julia) / Map (Dart)** — Caller uses language-native patterns: `update_time_series_group!(db, col, grp, id; date_time=[...], temp=[...])` in Julia, `updateTimeSeriesGroup(col, grp, id, {'date_time': [...], 'temperature': [...]})` in Dart.
- **Integer and string value column support** — Current API is float-only. New API natively supports int64_t and string columns.

**Defer (v2+):**
- **Append semantics** — "Add rows without replacing" requires upsert logic and complicates SQL generation. Defer to separate function.
- **Partial column update** — "Update only temperature, leave humidity unchanged" requires per-column UPDATE instead of DELETE+INSERT. Different operation.
- **Time range filtering on update** — "Replace only rows in date range X-Y" makes DELETE conditional. Caller can read-modify-write instead.
- **Streaming/chunked update** — "Send rows in batches" adds transaction management complexity. Use `import_csv` or raw SQL for large datasets.

### Architecture Approach

**The C API is the translation layer between columnar FFI-friendly format and row-oriented C++ format.** The C++ interface already supports multi-column time series correctly. Only the C API marshaling and bindings need changes.

**Major components:**
1. **C API signature redesign (database_time_series.cpp)** — Replace 2-array `(date_times, values)` with N-array `(column_names, column_types, column_data, column_count, row_count)`. Implement columnar-to-row conversion using the exact switch/cast pattern from `convert_params()` in `database_query.cpp`.
2. **Columnar marshaling helpers (database_helpers.h)** — Add `convert_columnar_to_rows()` for update direction and `convert_rows_to_columnar()` for read direction. Reuse existing `strdup_safe()`, `copy_strings_to_c()`, and typed read templates.
3. **Metadata-driven binding logic (Julia and Dart)** — Bindings call `get_time_series_metadata()` to discover column names and types, then construct typed arrays accordingly. Julia uses kwargs -> parallel arrays. Dart uses Map -> parallel arrays with Arena allocation.
4. **Free function redesign** — `quiver_database_free_time_series_data()` iterates `column_types[]` to determine deallocation strategy per column (string columns: iterate and delete[] each element then delete[] array; numeric columns: single delete[]).

**Data flow (update path):**
```
Julia/Dart: kwargs/Map with column name -> typed array mappings
  |
  | Binding queries metadata to discover schema
  | Marshals to columnar arrays: names[], types[], data[], count, rows
  |
  v
C API: quiver_database_update_time_series_group(...)
  |
  | Converts columnar -> row-oriented: for each row, build map<string, Value>
  |
  v
C++ Database::update_time_series_group(vector<map<string, Value>>)
  |
  v
SQLite: INSERT with all columns
```

**Patterns to follow:**
- **Metadata-driven marshaling** — Use `get_time_series_metadata()` at runtime to discover schema. Never hardcode column names in bindings.
- **Co-located alloc/free** — Read and free functions both live in `database_time_series.cpp`, following existing alloc/free co-location principle.
- **Reuse existing type enum** — Use `quiver_data_type_t` for column types; do not introduce new enums.

**Anti-patterns to avoid:**
- **Hardcoding column names in bindings** — Fails for schemas with different dimension/value column names. Query metadata instead.
- **Row-oriented void* arrays through FFI** — Requires O(rows*cols) pointer allocations in bindings. Columnar: O(cols) allocations.
- **Changing the C++ interface** — C++ is already correct. Lua depends on it staying row-oriented. Only change C API marshaling.
- **Separate functions per type combination** — Combinatorial explosion. One function with type tags handles all combinations.

### Critical Pitfalls

1. **Partial allocation leak on multi-column read** — If read allocates arrays for columns 0..K-1 then fails on column K, those arrays leak because function returns `QUIVER_ERROR` and caller never receives pointers to free. **Prevention:** Allocate all output arrays to local variables first, only write to out-parameters after all succeed. Use RAII cleanup guard.

2. **Type tag / data array mismatch across C boundary** — If `column_types[i]` says INTEGER but `column_data[i]` actually points to `double[]`, static_cast silently reinterprets bytes, corrupting row_count values. **Prevention:** Validate column types against schema metadata before casting. Add explicit check: if caller's type tags don't match metadata, return error before accessing data.

3. **Julia GC collecting intermediate arrays during ccall** — Julia marshals kwargs to N typed arrays (string conversions, numeric vectors). If intermediates aren't rooted with `GC.@preserve`, GC can collect during the ccall, causing C function to read freed memory. **Prevention:** Wrap all intermediates in single `GC.@preserve` block encompassing the entire ccall.

4. **Breaking test suite incrementally** — Changing C API signature breaks 4 test suites (C++, C API, Julia, Dart) simultaneously, creating multi-day CI breakage. **Prevention:** Add new function alongside old (backward compatibility), migrate each test suite independently, remove old after all migrated. Every commit must pass `scripts/test-all.bat`.

5. **Free function mismatch for variable-column read** — Current free knows exact layout (char** + double*). New free receives N arrays of varying types. String columns need per-element cleanup; numeric columns single delete[]. Wrong choice causes leaks or heap corruption. **Prevention:** Free function receives `column_types[]` and switches on type to deallocate each column correctly.

## Implications for Roadmap

Based on research, suggested phase structure:

### Phase 1: Foundation (C API Core)
**Rationale:** C API redesign enables everything else. Must be completed first with backward compatibility to keep tests passing.
**Delivers:**
- New C API signatures in `include/quiver/c/database.h` (update, read, free)
- Columnar marshaling implementation in `src/c/database_time_series.cpp`
- Multi-column test schema (`multi_column_time_series.sql`)
- C API tests for multi-column scenarios
**Addresses:** N typed value columns (table stakes), type safety per column, free function redesign
**Avoids:** Partial allocation leak (via atomic alloc pattern), test suite breakage (via backward-compatible new functions)

### Phase 2: Julia Bindings
**Rationale:** Julia binding migration follows C API completion. Regenerate FFI layer, then update high-level wrappers.
**Delivers:**
- Regenerated `bindings/julia/src/c_api.jl` via generator
- Metadata-driven kwargs marshaling in `database_update.jl`
- Columnar unmarshaling in `database_read.jl`
- Julia tests using multi-column schema
**Addresses:** Binding-level kwargs decomposition, metadata-driven marshaling
**Avoids:** Julia GC collection pitfall (via comprehensive `GC.@preserve` block), kwargs column name mismatch (via metadata validation)

### Phase 3: Dart Bindings
**Rationale:** Dart binding migration follows same pattern as Julia. FFI regeneration then high-level wrapper updates.
**Delivers:**
- Regenerated `bindings/dart/lib/src/ffi/bindings.dart` via generator
- Map-based marshaling in `database_update.dart` (not named parameters—Dart can't do dynamic named params)
- Columnar unmarshaling in `database_read.dart` with Arena allocation
- Dart tests using multi-column schema
**Addresses:** Map interface (Dart's answer to kwargs), Arena-based FFI memory management
**Avoids:** Named parameters impossibility (use Map instead), Arena freed too early (single Arena for entire operation)

### Phase 4: Verification and Cleanup
**Rationale:** Remove old C API functions after all bindings migrated. Full-stack testing with multi-column schemas.
**Delivers:**
- Removal of deprecated single-column C API functions
- Full `scripts/build-all.bat` verification (all 1,213+ tests passing)
- Multi-column integration tests across all bindings
- Documentation updates (CLAUDE.md cross-layer naming table, binding docs)
**Addresses:** Backward compatibility removal, comprehensive verification
**Avoids:** Lua binding staleness (Lua already works, just needs test coverage), FFI generator not regenerated (enforced in checklist)

### Phase Ordering Rationale

- **C API first** — Enables all bindings. Cannot migrate bindings without new C API. Backward compatibility keeps tests green.
- **Julia then Dart (parallel possible)** — Bindings are independent. Can be done in parallel if resources allow, but sequential is safer for learning from Julia's migration issues.
- **Lua needs minimal work** — Lua binds directly to C++, which is unchanged. Only need test coverage for multi-column. No migration needed.
- **Cleanup only after full migration** — Removing old C API functions before bindings migrate breaks CI. Cleanup is final phase.

### Research Flags

**Phases with established patterns (skip research-phase):**
- **Phase 1 (C API)** — Pattern is directly modeled on existing `convert_params()` from `database_query.cpp`. Internal precedent is clear.
- **Phase 2 (Julia)** — Follows existing Julia kwargs pattern from `create_element!` and existing `GC.@preserve` pattern from `update_vector_strings!`.
- **Phase 3 (Dart)** — Follows existing Dart Map pattern from `createElement` and existing Arena pattern from all Dart FFI functions.
- **Phase 4 (Verification)** — Standard testing and cleanup. No new research needed.

**No phases require `/gsd:research-phase`** — This milestone is internally focused (redesign existing API with proven patterns). All necessary patterns exist in codebase.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | Direct analysis of codebase shows exact pattern exists in `database_query.cpp`. Columnar approach is proven. |
| Features | HIGH | Requirements derived from schema capabilities (multi_time_series.sql proves multi-column works at C++ layer) and binding consistency needs. |
| Architecture | HIGH | Direct inspection of all 4 layers (C++, C API, Julia, Dart, Lua). Component boundaries and data flow are established. |
| Pitfalls | HIGH | 5 critical pitfalls identified from codebase analysis (partial alloc leak, type mismatch, GC collection, test breakage, free mismatch). All have concrete prevention strategies from existing code patterns. |

**Overall confidence:** HIGH

### Gaps to Address

No major gaps. All recommendations are based on proven internal patterns. Minor points to validate during implementation:

- **Empty time series edge cases** — Test that 0-row updates work correctly with new multi-column API. Current API handles this; verify new API preserves behavior.
- **Row count validation** — Document contract clearly: all column arrays must have exactly `row_count` elements. Bindings validate before calling C API.
- **Dimension column naming** — Test with schema where dimension column is not named `date_time` (e.g., `date_recorded`). Verify metadata-driven approach works.

## Sources

### Primary (HIGH confidence)
- Quiver codebase direct analysis (all files in STACK.md, FEATURES.md, ARCHITECTURE.md, PITFALLS.md sources sections)
- `src/c/database_query.cpp` lines 10-36 — existing `convert_params()` pattern for typed parallel arrays
- `src/c/database_time_series.cpp` — current 2-column implementation and alloc/free patterns
- `src/c/database_helpers.h` — existing marshaling templates and string helpers
- `bindings/julia/src/database_update.jl` lines 138-167 — current kwargs + `GC.@preserve` pattern
- `bindings/dart/lib/src/database_update.dart` — current Map + Arena FFI pattern
- `src/lua_runner.cpp` lines 1058-1069 — Lua sol2 direct C++ binding
- `tests/schemas/valid/multi_time_series.sql` — proves multi-column support at schema/C++ level
- `include/quiver/c/database.h` — C API surface and `quiver_data_type_t` enum
- Project CLAUDE.md — architecture principles, error handling patterns, cross-layer naming conventions

### Secondary (MEDIUM confidence)
- SQLite C API `sqlite3_bind_*` family — industry precedent for type-tagged value passing
- Apache Arrow C Data Interface — industry precedent for columnar typed data through C FFI (columnar layout with type metadata)
- libpq (PostgreSQL C client) — uses `const char* const*` parallel arrays with type OID arrays for parameterized queries

### Tertiary (LOW confidence)
- None — all recommendations based on internal codebase patterns, no external speculation

---
*Research completed: 2026-02-12*
*Ready for roadmap: yes*
