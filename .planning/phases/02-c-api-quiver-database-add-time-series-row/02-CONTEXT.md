# Phase 2: C API quiver_database_add_time_series_row - Context

**Gathered:** 2026-05-19
**Status:** Ready for planning

<domain>
## Phase Boundary

Mirror the C++ `Database::add_time_series_row` (shipped in Phase 1) through the FFI boundary as `quiver_database_add_time_series_row`, using the established columnar typed-arrays marshaling pattern from `quiver_database_update_time_series_group`. Every Quiver language binding (Phase 3) consumes this single C symbol — it is the only deliverable of this phase.

Implementation deliverables:
1. Declaration in `include/quiver/c/database.h`
2. Implementation in `src/c/database_time_series.cpp`
3. C API test coverage in `tests/test_c_api_database_time_series.cpp`

Explicitly NOT in this phase:
- C++ core changes (locked by Phase 1)
- Binding wrappers — Julia/Dart/Python/Lua (Phase 3)
- `add_time_series_rows` plural batched variant (CORE-15, deferred to v2)
- Multi-element add across multiple `id`s (out of scope per REQUIREMENTS.md)

</domain>

<decisions>
## Implementation Decisions

### FFI Signature Shape
- **D-01:** Stick to the established columnar typed-arrays code pattern (user directive). Mirror `quiver_database_update_time_series_group`'s marshaling shape exactly, with one difference: this is a single-row API, so there is **no `row_count` parameter** and `column_data[c]` is a pointer to a single typed value (1-element typed array). This matches REQUIREMENTS.md CAPI-11 which lists `column_names[] / column_types[] / column_data[] / column_count` (no `row_count`).
- **D-02:** Final declared signature:
  ```c
  QUIVER_C_API quiver_error_t quiver_database_add_time_series_row(
      quiver_database_t* db,
      const char* collection,
      const char* group,
      int64_t id,
      const char* const* column_names,
      const int* column_types,
      const void* const* column_data,
      size_t column_count);
  ```
  `column_data[c]` points to one typed value: `int64_t*` for INTEGER, `double*` for FLOAT, `const char**` for STRING/DATE_TIME (still indirected because strings own their bytes).
- **D-03:** Declaration placement in `include/quiver/c/database.h`: immediately after `quiver_database_update_time_series_group` and before `quiver_database_read_time_series_row` — mirrors C++ ordering in `include/quiver/database.h`.
- **D-04:** Implementation placement in `src/c/database_time_series.cpp`: immediately after `quiver_database_update_time_series_group` (current ends at line ~297). Marshaling code uses the same `switch (column_types[c])` pattern; only difference is no outer `for (size_t r ...)` loop (single row → single map).

### Error Handling
- **D-05:** No new error strings in the C API layer. All Pattern-1 errors (`"Cannot add_time_series_row: ..."`) originate in C++ — the C wrapper just forwards via `catch (const std::exception& e) { quiver_set_last_error(e.what()); return QUIVER_ERROR; }`. Per CLAUDE.md "Error messages are defined in the C++/C API layer. Bindings retrieve and surface them — they never craft their own."
- **D-06:** Use `QUIVER_REQUIRE(db, collection, group)` at the top, then a conditional `if (column_count > 0) { QUIVER_REQUIRE(column_names, column_types, column_data); }` — exact same null-check shape as `quiver_database_update_time_series_group` (src/c/database_time_series.cpp:259-262).
- **D-07:** Unknown `column_types[c]` integer (not a valid `QUIVER_DATA_TYPE_*` enum value) throws from the C wrapper itself with `"Cannot add_time_series_row: unknown column type N"` — mirroring update's behavior (src/c/database_time_series.cpp:283-285). This is the one error string that the C API legitimately owns because the C++ method never sees an int — only `Value`.

### Plan Split
- **D-08:** Mirror Phase 1's two-plan split:
  - **Plan 02-01** — Declaration + implementation in header + cpp. Atomic commits per file.
  - **Plan 02-02** — Test coverage in `tests/test_c_api_database_time_series.cpp`.

  Rationale: Phase 1's identical split executed cleanly in ~10min total with atomic per-task commits, zero regressions. Same shape is the lowest-risk choice and keeps Phase 2 symmetric with Phase 1 for the audit trail.

### Test Scope
- **D-09:** Mirror the nine C++ scenarios from Phase 1's Plan 01-02 at the FFI boundary, **plus** the FFI-specific tests that `quiver_database_update_time_series_group` already has in `tests/test_c_api_database_time_series.cpp`. Final scenario list for `AddTimeSeriesRow*` tests:
  - **From C++ mirror (9):** simple insert, single-PK upsert, multi-dim insert (id+date_time+block), multi-dim upsert, partial value columns (omitted → NULL), missing-dimension error, unknown-column error, type-mismatch error, transaction-nesting matrix (rollback / explicit-commit / standalone autocommit).
  - **FFI-boundary additions (3):** NULL pointer arguments (mirrors `TimeSeriesNullArguments` at line 329), unknown `column_types[c]` integer (mirrors `UpdateTimeSeriesGroupUnknownColumnType` at line 636), NULL column arrays with `column_count > 0` (mirrors `UpdateTimeSeriesGroupNullColumnArraysWithCount` at line 612).

  Rationale: "Stick to the pattern" extends to test conventions. The existing `UpdateTimeSeriesGroup*` test block covers both the C++ semantics mirror AND the FFI-boundary edge cases; the new `AddTimeSeriesRow*` block should match shape-for-shape.
- **D-10:** Skip an `AddTimeSeriesRowDateTimeStringInterchangeable` test. The marshaling switch on `QUIVER_DATA_TYPE_STRING` / `QUIVER_DATA_TYPE_DATE_TIME` is a copy of update's identical branch (both go through `std::string(column_data[c])`); `UpdateTimeSeriesGroupDateTimeStringInterchangeable` (line 796) already proves it. Adding a duplicate test for add would be belt-and-suspenders with no new coverage. If a binding generator in Phase 3 surfaces a date-type marshaling regression, that's the right place to land the test.

### Generator Inputs
- **D-11:** CAPI-13 requires that the new symbol be picked up mechanically by Phase 3 binding generators. The existing generators (`bindings/julia/generator/generator.bat`, `bindings/dart/generator/generator.bat`, `bindings/python/generator/generator.bat`) all parse `include/quiver/c/database.h` — declaring the new function there with the existing `QUIVER_C_API` macro and standard C parameter shape is sufficient. No generator-side changes are needed in this phase; binding-side codegen reruns are Phase 3 work.

### Claude's Discretion
- SQL column iteration order, internal helper extraction (e.g., whether to factor out a shared `convert_typed_column_value(int type, const void* data, size_t row_index)` helper between update and add) — choose what reads cleanest. Per CLAUDE.md "Simple solutions over complex abstractions": prefer a literal copy of the marshaling switch over a new helper unless the duplication is awkward at review time.
- Test ordering inside the file — group new `AddTimeSeriesRow*` tests immediately after the existing `UpdateTimeSeriesGroup*` block, mirroring the C++ test file's structure.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Phase 2 scope and requirements
- `.planning/ROADMAP.md` §"Phase 2: C API quiver_database_add_time_series_row" — phase goal, success criteria, dependency on Phase 1
- `.planning/REQUIREMENTS.md` §"C API" — CAPI-11, CAPI-12, CAPI-13 acceptance criteria
- `.planning/PROJECT.md` §"Current Milestone: v1.1 add_time_series_row" — milestone scope and constraints

### C++ source being mirrored
- `include/quiver/database.h:120-129` — `Database::add_time_series_row` declaration (the C++ surface this phase wraps)
- `src/database_time_series.cpp` — `Database::add_time_series_row` implementation (Pattern-1 error strings originate here; the C wrapper relays them verbatim)
- `tests/test_database_time_series.cpp` — nine `TEST(Database, AddTimeSeriesRow*)` cases that define the scenario list to mirror at the FFI boundary
- `tests/schemas/valid/multi_dim_time_series.sql` — composite-PK schema (id + date_time + block) reused by the C API multi-dim tests

### C API pattern being followed
- `include/quiver/c/database.h:290-300` — `quiver_database_update_time_series_group` declaration (signature shape to mirror, minus `row_count`)
- `src/c/database_time_series.cpp:250-297` — `quiver_database_update_time_series_group` implementation (marshaling switch + error pattern to mirror)
- `src/c/internal.h` — `QUIVER_REQUIRE` macro + `quiver_database_t` struct definition
- `src/c/database_options.h` — Not needed; this phase adds no new options. (Listed only so planner does not waste time looking.)
- `tests/test_c_api_database_time_series.cpp` — existing `UpdateTimeSeriesGroup*` tests (lines 198, 271, 329, 492, 532, 612, 636, 672, 713, 754, 796) are the template for the new `AddTimeSeriesRow*` tests

### Project conventions
- `CLAUDE.md` §"C API Patterns" — return codes, error handling, factory functions, memory management
- `CLAUDE.md` §"Multi-Column Time Series" — columnar typed-arrays pattern documentation (note: documents `update`/`read` with `row_count`; this phase extends it with the single-row `add` variant **without** `row_count`)
- `CLAUDE.md` §"Cross-Layer Naming Conventions" — naming rule `C++ method → quiver_database_*`
- `CLAUDE.md` §"Error Handling" (C++ patterns) — three canonical message patterns

### Phase 1 artifacts (informational)
- `.planning/phases/01-c-core-add-time-series-row/01-01-SUMMARY.md` — Phase 1 Plan 01: C++ impl record
- `.planning/phases/01-c-core-add-time-series-row/01-02-SUMMARY.md` — Phase 1 Plan 02: C++ tests record (defines the 9 scenarios mirrored here)

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `quiver_database_update_time_series_group` (src/c/database_time_series.cpp:250) — the marshaling switch on `column_types[c]` is a near-direct copy target. Only structural difference for `add_time_series_row`: no outer row loop, no `row_count` parameter, populate one `std::map<std::string, quiver::Value>` instead of `std::vector<std::map<...>>`.
- `QUIVER_REQUIRE` macro (src/c/internal.h) — already handles the `db / collection / group` triad with null-check + canonical error. No new helper needed.
- `quiver_set_last_error` — already wired up; just call it inside the catch block.

### Established Patterns
- **Columnar typed-arrays marshaling** — locked. The four-array shape (`column_names`, `column_types`, `column_data`, `column_count`) is the single supported FFI marshaling shape for time series data. Indexed `vector<int64_t>` dims overloads were rejected in `b9c9ad0`; do not reintroduce.
- **Three-message error convention** — locked. C++ throws Pattern-1 (`"Cannot {op}: ..."`); the C wrapper never crafts its own messages except for the one case where the wrapper itself detects an out-of-band condition (unknown `column_types[c]` integer — see `quiver_database_update_time_series_group` line 283-285).
- **Alloc/free co-location** — N/A here. `add_time_series_row` is a write-only operation; nothing is allocated and returned to the caller, so no matching free function is needed.

### Integration Points
- `include/quiver/c/database.h` — public C header consumed by every binding generator. Adding the declaration here is the one cross-layer touch point.
- `src/c/database_time_series.cpp` — translation unit for all time-series C API entry points. New implementation lands here.
- `tests/test_c_api_database_time_series.cpp` — single test file for all time-series C API tests. New scenarios append here.
- Phase 3 generators (`bindings/{julia,dart,python}/generator/generator.bat`) — read `include/quiver/c/database.h` and auto-discover the new symbol. No coordination needed within this phase.

</code_context>

<specifics>
## Specific Ideas

- User directive: "stick to the other code pattern" — interpreted as: mirror `quiver_database_update_time_series_group`'s marshaling code shape (typed-array switch, `QUIVER_REQUIRE` null checks, try/catch+set_last_error error relay), but adapted for single-row (no `row_count`, one `std::map` not a vector of maps).

</specifics>

<deferred>
## Deferred Ideas

- **`quiver_database_add_time_series_rows` (plural batched variant)** — CORE-15, already deferred to v2 per REQUIREMENTS.md. Not in this phase.
- **Multi-element add (rows for multiple `id`s in one call)** — Out of scope per REQUIREMENTS.md, mirrors update's per-id shape.
- **Shared `convert_typed_column_value` helper between update and add** — Possible refactor if review surfaces the marshaling switch duplication as awkward. Not planned for this phase per the "simple over abstract" project principle; can land in a later cleanup phase if needed.

</deferred>

---

*Phase: 02-c-api-quiver-database-add-time-series-row*
*Context gathered: 2026-05-19*
