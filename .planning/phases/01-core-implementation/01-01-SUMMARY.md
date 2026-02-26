---
phase: 01-core-implementation
plan: 01
subsystem: bindings
tags: [lua, julia, composite-read, read-element-by-id]

# Dependency graph
requires: []
provides:
  - "read_element_by_id Lua binding (flat table with scalars, vector columns, set columns)"
  - "read_element_by_id Julia binding (flat Dict{String,Any} with scalars, vector columns, set columns)"
affects: [02-binding-extensions]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Binding-level composition: compose existing C API calls to build flat element representation"
    - "Nonexistent ID detection via nil label (no validation query)"
    - "Multi-column vector/set groups: each column as separate top-level key"

key-files:
  created: []
  modified:
    - src/lua_runner.cpp
    - tests/test_lua_runner.cpp
    - bindings/julia/src/database_read.jl
    - bindings/julia/test/test_database_read.jl
    - bindings/julia/src/date_time.jl

key-decisions:
  - "id excluded from result, label included as regular scalar"
  - "Nonexistent IDs detected via nil label (NOT NULL in schema) -- no extra validation query"
  - "Multi-column vectors/sets: each column as own top-level key, not nested under group name"
  - "Lua DATE_TIME returns raw strings; Julia returns native DateTime objects"

patterns-established:
  - "read_element_by_id pattern: list metadata, typed by-id reads, flat merge -- reusable for Dart/Python bindings"

requirements-completed: [READ-01, READ-02]

# Metrics
duration: 10min
completed: 2026-02-26
---

# Phase 01 Plan 01: read_element_by_id Lua and Julia Bindings Summary

**Binding-level read_element_by_id composing metadata queries with typed by-id reads into flat table/dict across Lua and Julia**

## Performance

- **Duration:** 10 min
- **Started:** 2026-02-26T17:55:38Z
- **Completed:** 2026-02-26T18:05:46Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments
- Lua `db:read_element_by_id("Collection", id)` returns flat table with all scalar, vector column, and set column values as top-level keys
- Julia `read_element_by_id(db, "Collection", id)` returns flat Dict{String,Any} with same structure, plus native DateTime for DATE_TIME columns
- Both handle nonexistent IDs by returning empty table/dict (detected via nil label, no validation query)
- 6 new tests (3 Lua, 3 Julia) covering all categories, nonexistent IDs, and scalars-only schemas
- Fixed pre-existing `string_to_date_time` NULL handling gap in Julia

## Task Commits

Each task was committed atomically:

1. **Task 1: Implement read_element_by_id in Lua binding and add tests** - `8725165` (feat)
2. **Task 2: Implement read_element_by_id in Julia binding and add tests** - `aa93277` (feat)

**Plan metadata:** [pending] (docs: complete plan)

## Files Created/Modified
- `src/lua_runner.cpp` - Added read_element_by_id_lua composing scalars/vectors/sets into flat table, registered in bind_database()
- `tests/test_lua_runner.cpp` - 3 new tests: all categories, nonexistent ID, scalars only
- `bindings/julia/src/database_read.jl` - Added read_element_by_id composing scalars/vectors/sets into flat Dict
- `bindings/julia/test/test_database_read.jl` - 3 new tests: all categories, nonexistent ID, scalars only
- `bindings/julia/src/date_time.jl` - Added Nothing method for string_to_date_time

## Decisions Made
- id excluded from result (caller already knows it); label included as regular scalar
- Nonexistent ID detection: read scalars first, if label is nil/nothing return empty result immediately (no extra SQL query)
- Multi-column vectors/sets: each column is its own top-level key, not nested under group name
- Lua returns raw strings for DATE_TIME; Julia uses native DateTime via read_scalar_date_time_by_id

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Fixed string_to_date_time Nothing handling in Julia**
- **Found during:** Task 2 (Julia read_element_by_id implementation)
- **Issue:** `read_scalar_date_time_by_id` calls `string_to_date_time(read_scalar_string_by_id(...))` which returns `nothing` for NULL scalars, but `string_to_date_time` only accepted `String` type
- **Fix:** Added `string_to_date_time(::Nothing)::Nothing` method dispatch in `date_time.jl`
- **Files modified:** bindings/julia/src/date_time.jl
- **Verification:** Julia test "read_element_by_id scalars only" passes (basic.sql has nullable date_attribute)
- **Committed in:** aa93277 (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (1 blocking)
**Impact on plan:** Fix necessary for correctness -- pre-existing gap in NULL DateTime handling. No scope creep.

## Issues Encountered
None beyond the auto-fixed deviation.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Lua and Julia bindings for read_element_by_id are complete and tested
- Pattern established for Dart/Python bindings (Plan 02): compose list metadata + typed by-id reads into flat result
- All 533 C++ tests and all Julia tests pass with no regressions

## Self-Check: PASSED

All files exist, all commits verified.

---
*Phase: 01-core-implementation*
*Completed: 2026-02-26*
