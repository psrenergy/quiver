# Roadmap: Quiver

## Milestones

- v0.2 (Pre-GSD) -- Full CRUD, typed operations, bindings
- v0.3 Explicit Transactions -- Phases 1-4 (shipped 2026-02-22)
- v0.4 CSV Export -- Phases 5-7 (Phase 7 plan 02 complete)

## Phases

<details>
<summary>v0.3 Explicit Transactions (Phases 1-4) -- SHIPPED 2026-02-22</summary>

- [x] Phase 1: C++ Transaction Core (1/1 plans) -- completed 2026-02-20
- [x] Phase 2: C API Transaction Surface (1/1 plans) -- completed 2026-02-21
- [x] Phase 3: Language Bindings (1/1 plans) -- completed 2026-02-21
- [x] Phase 4: Performance Benchmark (1/1 plans) -- completed 2026-02-21

See: milestones/v0.3-ROADMAP.md for full details.

</details>

### v0.4 CSV Export

- [x] **Phase 5: C++ Core** - Options struct, export logic, RFC 4180 writer, enum resolution, date formatting, tests (completed 2026-02-22)
- [ ] **Phase 6: C API** - Flat options struct, export function, FFI generation, C API tests
- [x] **Phase 7: Bindings** - Julia, Dart, and Lua wrappers with idiomatic option conversion and binding tests (completed 2026-02-23)

## Phase Details

### Phase 5: C++ Core
**Goal**: Users can export any collection's scalars or groups to a correctly formatted CSV file with optional enum and date transformations
**Depends on**: Nothing (first phase of v0.4; builds on existing v0.3 codebase)
**Requirements**: CSV-01, CSV-02, CSV-03, CSV-04, OPT-01, OPT-02, OPT-03, OPT-04
**Success Criteria** (what must be TRUE):
  1. Calling `export_csv(collection, "", path, options)` produces a CSV file with one header row (label + scalar attribute names) and one data row per element, with no `id` column
  2. Calling `export_csv(collection, "group_name", path, options)` produces a CSV file with label replacing id, using actual schema column names, for any vector/set/time series group
  3. Passing an `enum_labels` map in options replaces integer values with human-readable labels in the output; unmapped integer values appear as raw integers (no crash, no empty field)
  4. Passing a `date_time_format` string in options reformats DateTime columns (identified by metadata, not value inspection) using strftime; non-DateTime columns are unaffected
  5. Exporting an empty collection writes a header-only CSV file (not an error); NULL values appear as empty fields; all fields with commas, quotes, or newlines are correctly escaped per RFC 4180
**Plans**: 2 plans

Plans:
- [ ] 05-01-PLAN.md -- CSVExportOptions struct, export_csv implementation, C API stub update
- [ ] 05-02-PLAN.md -- Comprehensive test suite and CLAUDE.md documentation update

### Phase 6: C API
**Goal**: C API consumers can export CSV through flat FFI-safe functions with the same capabilities as the C++ API
**Depends on**: Phase 5
**Requirements**: CAPI-01, CAPI-02
**Success Criteria** (what must be TRUE):
  1. `quiver_csv_export_options_default()` returns a valid default options struct and `quiver_database_export_csv()` accepts it to produce identical output as the C++ API
  2. The `quiver_csv_export_options_t` flat struct can represent any enum_labels mapping (multiple attributes, multiple values per attribute) through FFI-safe parallel arrays with no nested pointers beyond one level
  3. FFI generators (Julia Clang.jl and Dart ffigen) successfully parse the new C API headers and produce correct binding declarations
**Plans**: 2 plans

Plans:
- [ ] 06-01-PLAN.md -- C API csv.h header, flat options struct, database_csv.cpp implementation, build registration
- [ ] 06-02-PLAN.md -- C API CSV test suite (19 tests), FFI generator validation, CLAUDE.md update

### Phase 7: Bindings
**Goal**: Julia, Dart, and Lua users can export CSV using idiomatic APIs with native map types for options
**Depends on**: Phase 6
**Requirements**: BIND-01, BIND-02, BIND-03
**Success Criteria** (what must be TRUE):
  1. Julia `export_csv(db, collection, group, path; options)` accepts a Julia Dict for enum_labels and produces correct CSV output; default options work without any keyword arguments
  2. Dart `exportCSV(collection, group, path, {options})` accepts a Dart Map for enum_labels and produces correct CSV output; default options work without named parameters
  3. Lua `db:export_csv(collection, group, path, options)` accepts a Lua table for enum_labels and produces correct CSV output; omitting the options table uses defaults
  4. All three bindings produce byte-identical CSV output to each other and to the C++ API for the same input data and options
**Plans**: 2 plans

Plans:
- [ ] 07-01-PLAN.md -- Julia and Dart export_csv/exportCSV wrappers with options marshaling and tests
- [x] 07-02-PLAN.md -- Lua export_csv binding via sol2 and C++ tests (completed 2026-02-22)

## Progress

**Execution Order:**
Phases execute in numeric order: 5 -> 6 -> 7

| Phase | Milestone | Plans Complete | Status | Completed |
|-------|-----------|----------------|--------|-----------|
| 1. C++ Transaction Core | v0.3 | 1/1 | Complete | 2026-02-20 |
| 2. C API Transaction Surface | v0.3 | 1/1 | Complete | 2026-02-21 |
| 3. Language Bindings | v0.3 | 1/1 | Complete | 2026-02-21 |
| 4. Performance Benchmark | v0.3 | 1/1 | Complete | 2026-02-21 |
| 5. C++ Core | v0.4 | Complete    | 2026-02-22 | - |
| 6. C API | v0.4 | 0/2 | Planning complete | - |
| 7. Bindings | v0.4 | Complete    | 2026-02-23 | - |

---
*Roadmap created: 2026-02-22*
*Last updated: 2026-02-22*
