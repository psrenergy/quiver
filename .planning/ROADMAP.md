# Roadmap: Quiver

## Milestones

- v0.2 (Pre-GSD) -- Full CRUD, typed operations, bindings
- v0.3 Explicit Transactions -- Phases 1-4 (shipped 2026-02-22)
- v0.4 CSV Export -- Phases 5-7 (shipped 2026-02-23)
- v0.5 CSV Refactor -- Phases 8-9 (in progress)

## Phases

<details>
<summary>v0.3 Explicit Transactions (Phases 1-4) -- SHIPPED 2026-02-22</summary>

- [x] Phase 1: C++ Transaction Core (1/1 plans) -- completed 2026-02-20
- [x] Phase 2: C API Transaction Surface (1/1 plans) -- completed 2026-02-21
- [x] Phase 3: Language Bindings (1/1 plans) -- completed 2026-02-21
- [x] Phase 4: Performance Benchmark (1/1 plans) -- completed 2026-02-21

See: milestones/v0.3-ROADMAP.md for full details.

</details>

<details>
<summary>v0.4 CSV Export (Phases 5-7) -- SHIPPED 2026-02-23</summary>

- [x] Phase 5: C++ Core (2/2 plans) -- completed 2026-02-22
- [x] Phase 6: C API (2/2 plans) -- completed 2026-02-22
- [x] Phase 7: Bindings (2/2 plans) -- completed 2026-02-23

</details>

### v0.5 CSV Refactor

- [ ] **Phase 8: Library Integration** - Replace hand-rolled CSV writer with rapidcsv via FetchContent
- [ ] **Phase 9: Header Consolidation** - Merge C API csv.h into options.h, delete csv.h, regenerate FFI bindings

## Phase Details

### Phase 8: Library Integration
**Goal**: CSV export uses a proper third-party library instead of hand-rolled helpers, enabling future CSV import without adding a second dependency
**Depends on**: Nothing (first phase of v0.5; builds on existing v0.4 codebase)
**Requirements**: LIB-01, LIB-02, LIB-03
**Success Criteria** (what must be TRUE):
  1. `csv_escape()` and `write_csv_row()` no longer exist in `src/database_csv.cpp` -- all CSV writing goes through rapidcsv
  2. rapidcsv is fetched via CMake FetchContent and linked as a PRIVATE dependency -- no new DLLs appear in the build output on Windows
  3. All 22 C++ CSV export tests pass without any test modifications (same assertions, same expected output)
  4. All 19 C API CSV export tests pass without any test modifications
**Plans**: 1 plan
Plans:
- [ ] 08-01-PLAN.md -- Replace csv_escape/write_csv_row with rapidcsv Document/Save via FetchContent

### Phase 9: Header Consolidation
**Goal**: All C API option types live in a single header, and FFI-generated bindings reflect the consolidated layout
**Depends on**: Phase 8 (stable codebase after library swap)
**Requirements**: HDR-01, HDR-02, HDR-03
**Success Criteria** (what must be TRUE):
  1. `include/quiver/c/csv.h` does not exist -- no compatibility stub, no redirect
  2. `quiver_csv_export_options_t` struct and `quiver_csv_export_options_default()` declaration are present in `include/quiver/c/options.h`
  3. Regenerated Julia `c_api.jl` and Dart `bindings.dart` contain exactly one definition of `quiver_csv_export_options_t` with all 6 fields intact
  4. All Julia CSV tests (19) and Dart CSV tests (5) pass after FFI regeneration
  5. All C API tests (282) pass with the updated include paths
**Plans**: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 8 -> 9

| Phase | Milestone | Plans Complete | Status | Completed |
|-------|-----------|----------------|--------|-----------|
| 1. C++ Transaction Core | v0.3 | 1/1 | Complete | 2026-02-20 |
| 2. C API Transaction Surface | v0.3 | 1/1 | Complete | 2026-02-21 |
| 3. Language Bindings | v0.3 | 1/1 | Complete | 2026-02-21 |
| 4. Performance Benchmark | v0.3 | 1/1 | Complete | 2026-02-21 |
| 5. C++ Core | v0.4 | 2/2 | Complete | 2026-02-22 |
| 6. C API | v0.4 | 2/2 | Complete | 2026-02-22 |
| 7. Bindings | v0.4 | 2/2 | Complete | 2026-02-23 |
| 8. Library Integration | v0.5 | 0/1 | Not started | - |
| 9. Header Consolidation | v0.5 | 0/? | Not started | - |

---
*Roadmap created: 2026-02-22*
*Last updated: 2026-02-22*
