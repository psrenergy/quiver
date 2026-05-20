# Roadmap: Quiver

## Overview

Quiver's v1.1 milestone adds a single API — `add_time_series_row` — as a vertical slice across every layer of the stack. The work starts in the C++ core (where all intelligence lives), propagates through the C API (the FFI boundary that every binding marshals against), and finishes in the language bindings (Julia, Dart, Python, Lua) and documentation. The phase boundaries follow the codebase's existing layering: each phase delivers a verifiable surface that the next phase consumes.

## Milestones

- [x] **v1.0 baseline** - Pre-GSD shipped capabilities (see PROJECT.md Validated section)
- [ ] **v1.1 add_time_series_row** - Phases 1-3 (active)

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

### v1.1 add_time_series_row

- [x] **Phase 1: C++ Core add_time_series_row** - Implement `Database::add_time_series_row` with upsert semantics, transaction-nest awareness, and full C++ test coverage (completed 2026-05-19)
- [x] **Phase 2: C API quiver_database_add_time_series_row** - Mirror the C++ method through the FFI boundary using the columnar typed-arrays pattern, with C API tests (completed 2026-05-19)
- [x] **Phase 3: Language bindings + documentation** - Regenerate Julia / Dart / Python / Lua wrappers, add per-binding tests, and sync `docs/time_series.md` + CLAUDE.md with the shipped API (completed 2026-05-20)

## Phase Details

### Phase 1: C++ Core add_time_series_row
**Goal**: Power-users can append or upsert a single time series row through `Database::add_time_series_row` with the same validation, error patterns, and transaction semantics as the existing time series API.
**Depends on**: Nothing (first phase)
**Requirements**: CORE-11, CORE-12, CORE-13, CORE-14
**Success Criteria** (what must be TRUE):
  1. Calling `db.add_time_series_row(collection, group, id, row)` with a fresh PK inserts a new row visible to `read_time_series_group` / `read_time_series_row`.
  2. Calling `add_time_series_row` twice with the same PK (id + every dimension column) overwrites the value columns rather than failing — upsert semantics on the time series primary key.
  3. Calling `add_time_series_row` from outside an explicit transaction commits standalone; calling it inside `begin_transaction` / `commit` participates in the outer transaction without double-beginning.
  4. Missing dimension columns, unknown columns, and type mismatches throw with the canonical `"Cannot add_time_series_row: ..."` message — matching the three-pattern error convention.
  5. `quiver_tests.exe` covers happy path, upsert, multi-dimension schema (`date_time + block`), partial value columns (omitted → NULL), and every error branch.
**Plans**: TBD

### Phase 2: C API quiver_database_add_time_series_row
**Goal**: Every Quiver language binding can call `add_time_series_row` through a stable C ABI that follows the existing columnar typed-arrays pattern and surfaces C++ errors through `quiver_get_last_error`.
**Depends on**: Phase 1
**Requirements**: CAPI-11, CAPI-12, CAPI-13
**Success Criteria** (what must be TRUE):
  1. `quiver_database_add_time_series_row` is declared in `include/quiver/c/database.h` and implemented in `src/c/database_time_series.cpp`, accepting the same `column_names[] / column_types[] / column_data[] / column_count` parallel-array shape as `quiver_database_update_time_series_group`.
  2. The wrapper returns `quiver_error_t` (`QUIVER_OK` / `QUIVER_ERROR`) and propagates every C++ exception through `quiver_set_last_error` so callers can retrieve the canonical message via `quiver_get_last_error`.
  3. `quiver_c_tests.exe` exercises the new entry point at the FFI boundary: happy path, upsert, multi-dimension schema, NULL value columns, missing-dimension error, unknown-column error, type-mismatch error.
  4. The C API header changes pass through the FFI generator inputs (function signature, return code, parameter order) so downstream binding generators can pick the symbol up mechanically in Phase 3.
**Plans**: 02-01 (declaration + impl), 02-02 (FFI tests)

### Phase 3: Language bindings + documentation
**Goal**: Julia, Dart, Python, and Lua users can each call `add_time_series_row` in the idiomatic shape of their language, with tests proving the round-trip, and the existing time series documentation reflects the shipped API surface.
**Depends on**: Phase 2
**Requirements**: JULIA-11, DART-11, PY-11, LUA-11, DOC-11
**Success Criteria** (what must be TRUE):
  1. Julia: `Quiver.add_time_series_row!(db, collection, group, id; <dim>=…, <value>=…)` exists after running `bindings/julia/generator/generator.bat`; `bindings/julia/test/test_time_series.jl` covers insert + upsert + at least one error path.
  2. Dart: `Database.addTimeSeriesRow(collection, group, id, row)` exists after running `bindings/dart/generator/generator.bat`; `bindings/dart/test/` covers insert + upsert; `libquiver_c.dll` PATH wiring through `test.bat` still works.
  3. Python: `db.add_time_series_row(collection, group, id, **kwargs)` exists after running `bindings/python/generator/generator.bat` with the matching `_c_api.py` cdef update; `bindings/python/test/` covers insert + upsert + dict unpacking.
  4. Lua: `db:add_time_series_row(collection, group, id, row_table)` is exposed through `LuaRunner`; the Lua test harness covers insert + upsert.
  5. `docs/time_series.md` is updated so its `add_time_series_row!` example matches the actually-shipped Julia signature, and `CLAUDE.md` lists the method in the Core API section and the cross-layer naming table.
**Plans**: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2 → 3

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. C++ Core add_time_series_row | 2/2 | Complete    | 2026-05-19 |
| 2. C API quiver_database_add_time_series_row | 2/2 | Complete   | 2026-05-19 |
| 3. Language bindings + documentation | 5/5 | Complete   | 2026-05-20 |
