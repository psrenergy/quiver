# Requirements: Quiver — Milestone v1.1 add_time_series_row

**Defined:** 2026-05-19
**Core Value:** Give power-users one consistent, intuitive API surface for structured + time series + binary array data across Julia, Dart, Python, and Lua — with all intelligence in C++ and bindings kept thin.

## v1.1 Requirements

Vertical slice for `add_time_series_row`: a single-row upsert that complements the existing `read_time_series_row` and `update_time_series_group` operations, propagated symmetrically across every layer.

### Core (C++)

- [ ] **CORE-11**: `Database::add_time_series_row(collection, group, id, row)` inserts a single time series row (PK columns from schema + value columns from `row`), upserting on the time-series primary key (`id` + dimension columns) so calling it twice with the same key replaces the value columns.
- [ ] **CORE-12**: Row validation matches `update_time_series_group` semantics — every dimension column in the schema must be present in `row`; unknown columns and type mismatches throw with the canonical `"Cannot add_time_series_row: ..."` error message.
- [ ] **CORE-13**: The operation participates in the existing nest-aware `TransactionGuard` — works standalone (own transaction) and inside an explicit `begin_transaction` block without double-beginning.
- [ ] **CORE-14**: C++ tests in `tests/test_database_time_series.cpp` cover: simple insert, upsert (same key overwrites), multi-dimension schemas (e.g. `date_time + block`), partial value columns (omitted columns become NULL), missing-dimension error path, unknown-column error path, type-mismatch error path, transaction nesting.

### C API

- [ ] **CAPI-11**: `quiver_database_add_time_series_row` declared in `include/quiver/c/database.h` and implemented in `src/c/database_time_series.cpp`, using the columnar typed-arrays pattern (`column_names[]`, `column_types[]`, `column_data[]`, `column_count`) already established by `quiver_database_update_time_series_group`.
- [ ] **CAPI-12**: The wrapper returns `quiver_error_t` with errors surfaced through `quiver_get_last_error`, following the existing `QUIVER_REQUIRE` + try/catch + `quiver_set_last_error` pattern.
- [ ] **CAPI-13**: C API tests in `tests/test_c_api_database_time_series.cpp` mirror the C++ test coverage at the FFI boundary (happy path, upsert, multi-dimension, error paths).

### Bindings

- [ ] **JULIA-11**: Julia `add_time_series_row!(db, collection, group, id; <dimension>=…, <value_columns>=…)` regenerated via `bindings/julia/generator/generator.bat`, with Julia tests in `bindings/julia/test/test_time_series.jl`.
- [ ] **DART-11**: Dart `addTimeSeriesRow(collection, group, id, row)` regenerated via `bindings/dart/generator/generator.bat`, with Dart tests in `bindings/dart/test/`.
- [ ] **PY-11**: Python `add_time_series_row(collection, group, id, **kwargs)` (mirroring `create_element`'s kwargs convention) regenerated via `bindings/python/generator/generator.bat`, with Python tests in `bindings/python/test/`.
- [ ] **LUA-11**: Lua `db:add_time_series_row(collection, group, id, row_table)` exposed from `LuaRunner`, with Lua tests in `tests/lua/` (or equivalent existing Lua test harness).

### Documentation

- [ ] **DOC-11**: `docs/time_series.md` updated so the Julia `add_time_series_row!` example reflects the shipped signature (it currently references a function that doesn't exist yet); CLAUDE.md "Core API" + cross-layer naming tables include the new method.

## v2 Requirements

Deferred to future milestones. Not in v1.1 scope.

### Bulk row append
- **CORE-15**: A `add_time_series_rows` (plural) batched-append variant. Considered v2 because v1.1 ships the single-row primitive; a batched variant can be added later if profiling shows the per-call overhead is the bottleneck.

### Async / streaming
- **CORE-16**: Streaming row append for very large datasets. Deferred until there's a concrete user with the size problem.

## Out of Scope

| Feature | Reason |
|---------|--------|
| Deprecating or renaming `update_time_series_group` | Replace-all semantics is still the right tool when wiping an element's series; the new method is additive, not a replacement |
| Schema migration to allow time series without `date_time` PK | Time series schema convention is `_time_series_*` tables with a `date_*` dimension column — outside this milestone |
| Multi-element add (writing rows for multiple ids in one call) | Symmetric with existing per-id `update_time_series_group`; multi-id batching is a separate ergonomic question |
| Indexed `vector<int64_t>` dims overload on `BinaryFile` | Unrelated to time series; already rejected per `b9c9ad0` |

## Traceability

Empty initially. Populated by the roadmapper in the next step.

| Requirement | Phase | Status |
|-------------|-------|--------|
| CORE-11 | TBD | Pending |
| CORE-12 | TBD | Pending |
| CORE-13 | TBD | Pending |
| CORE-14 | TBD | Pending |
| CAPI-11 | TBD | Pending |
| CAPI-12 | TBD | Pending |
| CAPI-13 | TBD | Pending |
| JULIA-11 | TBD | Pending |
| DART-11 | TBD | Pending |
| PY-11 | TBD | Pending |
| LUA-11 | TBD | Pending |
| DOC-11 | TBD | Pending |

**Coverage:**
- v1.1 requirements: 12 total
- Mapped to phases: 0
- Unmapped: 12 ⚠️ (roadmap step will close this)

---
*Requirements defined: 2026-05-19*
*Last updated: 2026-05-19 after milestone v1.1 start*
