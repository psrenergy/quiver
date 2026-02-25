# Roadmap: Quiver

## Milestones

- ✅ **v0.3** -- Pre-GSD (core library, C API, Julia/Dart/Lua bindings)
- ✅ **v0.4 Python Bindings & Test Parity** -- Phases 1-7 (shipped 2026-02-24)
- 🚧 **v0.4.1 Python Catch-Up** -- Phases 8-9 (in progress)

## Phases

<details>
<summary>✅ v0.4 Python Bindings & Test Parity (Phases 1-7) -- SHIPPED 2026-02-24</summary>

- [x] Phase 1: Foundation (2/2 plans) -- completed 2026-02-23
- [x] Phase 2: Reads and Metadata (2/2 plans) -- completed 2026-02-23
- [x] Phase 3: Writes and Transactions (2/2 plans) -- completed 2026-02-23
- [x] Phase 4: Queries and Relations (1/1 plan) -- completed 2026-02-23
- [x] Phase 5: Time Series (2/2 plans) -- completed 2026-02-23
- [x] Phase 6: CSV and Convenience Helpers (2/2 plans) -- completed 2026-02-24
- [x] Phase 7: Test Parity (4/4 plans) -- completed 2026-02-24

Full details: [milestones/v0.4-ROADMAP.md](milestones/v0.4-ROADMAP.md)

</details>

### v0.4.1 Python Catch-Up (In Progress)

**Milestone Goal:** Align Python bindings with upstream C++ changes -- remove deprecated relation convenience methods, implement import_csv, and restructure CSVOptions to match C++ layout.

- [x] **Phase 8: Relations Cleanup** - Remove pure-Python relation convenience methods and rewrite tests to use integrated CRUD pattern (completed 2026-02-25)
- [ ] **Phase 9: CSV Import and Options** - Implement import_csv, restructure CSVOptions with locale support, add import tests, update documentation

## Phase Details

### Phase 8: Relations Cleanup
**Goal**: Python relation operations use the same create/update/read pattern as C++ (no convenience wrappers)
**Depends on**: Phase 7 (v0.4 complete)
**Requirements**: REL-01, REL-02
**Success Criteria** (what must be TRUE):
  1. Database class has no read_scalar_relation or update_scalar_relation methods
  2. Relation tests create elements with relation attributes via Element.set and read them back via read_scalar_integer_by_id
  3. All existing Python tests pass after the removal (no regressions)
**Plans:** 2/2 plans complete

Plans:
- [x] 08-01-PLAN.md -- Remove deprecated relation methods, fix generator, regenerate declarations
- [ ] 08-02-PLAN.md -- Port FK resolution test suite from Julia/Dart (create + update + metadata)

### Phase 9: CSV Import and Options
**Goal**: Python binding supports full CSV round-trip (export and import) with correctly structured options matching the C++ CSVOptions layout
**Depends on**: Phase 8
**Requirements**: CSV-01, CSV-02, CSV-03, CSV-04
**Success Criteria** (what must be TRUE):
  1. User can call import_csv on a Python Database instance to load CSV data into a collection (scalar and group imports both work)
  2. CSVOptions class (not CSVExportOptions) exists with enum_labels structured as attribute-locale-label-value matching C++ and Julia/Dart
  3. Import and export round-trip tests pass -- data exported to CSV can be imported back and read correctly
  4. CLAUDE.md references CSVOptions (not CSVExportOptions) throughout
**Plans**: TBD

Plans:
- [ ] 09-01: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 8 -> 9

| Phase | Milestone | Plans Complete | Status | Completed |
|-------|-----------|----------------|--------|-----------|
| 1. Foundation | v0.4 | 2/2 | Complete | 2026-02-23 |
| 2. Reads and Metadata | v0.4 | 2/2 | Complete | 2026-02-23 |
| 3. Writes and Transactions | v0.4 | 2/2 | Complete | 2026-02-23 |
| 4. Queries and Relations | v0.4 | 1/1 | Complete | 2026-02-23 |
| 5. Time Series | v0.4 | 2/2 | Complete | 2026-02-23 |
| 6. CSV and Convenience Helpers | v0.4 | 2/2 | Complete | 2026-02-24 |
| 7. Test Parity | v0.4 | 4/4 | Complete | 2026-02-24 |
| 8. Relations Cleanup | 2/2 | Complete   | 2026-02-25 | - |
| 9. CSV Import and Options | v0.4.1 | 0/? | Not started | - |
