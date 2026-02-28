# Roadmap: Quiver v0.5

## Overview

Quiver v0.5 is a code quality milestone that fixes architectural layering issues, resolves naming inconsistencies, and closes Python binding gaps. Work is sequenced by blast radius: C++-only structural fixes first (zero binding impact), then C API surface changes that trigger all binding generator re-runs, then self-contained Python additions, and finally cross-binding test validation of the completed API surface.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [x] **Phase 1: Type Ownership** - C++ owns DatabaseOptions and CSVOptions types independently of C API
- [x] **Phase 2: Free Function Naming** - Add quiver_database_free_string, remove quiver_element_free_string, re-run all generators
- [x] **Phase 3: Python DataType Constants** - Replace 20+ magic integers with DataType IntEnum
- [ ] **Phase 4: Python LuaRunner Binding** - Add LuaRunner wrapper to Python with lifetime-safe Database reference
- [ ] **Phase 5: Cross-Binding Test Coverage** - Validate is_healthy/path across bindings and Python convenience helpers

## Phase Details

### Phase 1: Type Ownership
**Goal**: C++ core defines its own option types without depending on C API headers
**Depends on**: Nothing (first phase)
**Requirements**: TYPES-01, TYPES-02
**Success Criteria** (what must be TRUE):
  1. `include/quiver/options.h` has zero includes from `include/quiver/c/` -- C++ core compiles without C API headers
  2. `DatabaseOptions` uses `enum class LogLevel` with member initializers (not a typedef of the C struct)
  3. `CSVOptions` is defined as a C++ type in `include/quiver/options.h` with boundary conversion in `src/c/`
  4. All existing C++, C API, Julia, Dart, and Python tests pass without modification (C API struct layout unchanged)
**Plans**: 2 plans

Plans:
- [x] 01-01-PLAN.md -- Define native C++ types and update production code with boundary conversion
- [x] 01-02-PLAN.md -- Mechanical test file update (217 occurrences across 17 files)

### Phase 2: Free Function Naming
**Goal**: Query/read string results are freed through the correct entity-scoped function across all layers
**Depends on**: Phase 1
**Requirements**: NAME-01, NAME-02
**Success Criteria** (what must be TRUE):
  1. `quiver_database_free_string()` exists in `include/quiver/c/database.h` and frees strings returned by database read/query operations
  2. `quiver_element_free_string()` no longer exists anywhere in the codebase (header, implementation, bindings, tests)
  3. Julia, Dart, and Python bindings call the new `quiver_database_free_string` for all database-returned strings (generators re-run, Python cdef updated)
  4. All five test suites (C++, C API, Julia, Dart, Python) pass after the rename
**Plans**: 3 plans

Plans:
- [x] 02-01-PLAN.md -- C API rename + build + generators (add quiver_database_free_string, remove quiver_element_free_string, regenerate bindings)
- [x] 02-02-PLAN.md -- Hand-written binding updates + CLAUDE.md + full test validation
- [x] 02-03-PLAN.md -- Gap closure: fix Dart ffigen.yaml to include options.h, regenerate bindings, verify Dart tests pass

### Phase 3: Python DataType Constants
**Goal**: Python binding uses named constants instead of magic integers for data types
**Depends on**: Phase 2
**Requirements**: PY-02
**Success Criteria** (what must be TRUE):
  1. `DataType` IntEnum exists in the Python package with values matching C API constants (INTEGER=0, FLOAT=1, STRING=2, DATE_TIME=3, NULL=4)
  2. No bare integer literals (0, 1, 2, 3, 4) remain in `database.py` for data type dispatch -- all replaced with `DataType` members
  3. `ScalarMetadata.data_type` returns a `DataType` enum value (not a plain int)
  4. Python test suite passes and includes a test validating DataType values match CFFI lib constants
**Plans**: 1 plan

Plans:
- [x] 03-01-PLAN.md -- Define DataType IntEnum, replace all magic integers in database.py, add validation test

### Phase 4: Python LuaRunner Binding
**Goal**: Python users can execute Lua scripts against a Quiver database, achieving feature parity with Julia/Dart/Lua bindings
**Depends on**: Phase 2
**Requirements**: PY-01
**Success Criteria** (what must be TRUE):
  1. `LuaRunner` class exists in the Python package, wrapping `quiver_lua_runner_new/run/get_error/free`
  2. Python LuaRunner holds a reference to the Database object (preventing GC from collecting the database while the runner is alive)
  3. Lua scripts executed via Python LuaRunner can create/read/update/delete elements in the database
  4. Calling `run()` with invalid Lua raises a Python exception with the error message from `quiver_lua_runner_get_error` (not the global error)
**Plans**: 1 plan

Plans:
- [ ] 04-01-PLAN.md -- Add LuaRunner class with CFFI declarations, lifecycle management, and test suite

### Phase 5: Cross-Binding Test Coverage
**Goal**: All bindings have verified test coverage for health check and path accessors, and Python convenience helpers are tested
**Depends on**: Phase 4
**Requirements**: TEST-01, PY-03
**Success Criteria** (what must be TRUE):
  1. Julia tests exercise `is_healthy()` and `path()` on an open database and verify expected return values
  2. Dart tests exercise `isHealthy()` and `path()` on an open database and verify expected return values
  3. Python tests exercise `is_healthy()` and `path()` on an open database and verify expected return values
  4. Python tests cover `read_scalars_by_id`, `read_vectors_by_id`, `read_sets_by_id`, `read_element_by_id`, `read_vector_group_by_id`, `read_set_group_by_id`, and `read_element_ids` with assertions on returned data
  5. All five test suites pass as the final milestone gate
**Plans**: TBD

Plans:
- [ ] 05-01: TBD
- [ ] 05-02: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 -> 2 -> 3 -> 4 -> 5

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Type Ownership | 2/2 | Complete | 2026-02-28 |
| 2. Free Function Naming | 3/3 | Complete | 2026-02-28 |
| 3. Python DataType Constants | 1/1 | Complete | 2026-02-28 |
| 4. Python LuaRunner Binding | 0/1 | Not started | - |
| 5. Cross-Binding Test Coverage | 0/? | Not started | - |
