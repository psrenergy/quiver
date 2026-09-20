# Roadmap: Quiver — UI Metadata in `describe`

**Milestone:** v0.10.8
**Granularity:** standard
**Phases:** 2
**Coverage:** 10/10 v0.10.8 requirements mapped

## Overview

One new parser `.cpp`/`.h` in `QUIVER_SOURCES`, one plain member on `Database::Impl`, one call site
in `Database::from_migrations`, and two append-before-newline edits in `src/database_describe.cpp`.
Zero C API symbols, zero binding code, zero generator runs, zero CMake dependency changes, zero
binding test churn. The milestone is split on the only seam that physically exists in the render
layer: `write_collection_section` (`src/database_describe.cpp:56`) is one injection site shared by
`describe()` and `describe_collection()`, while `summarize_collection` (`:136`, histogram at
`:156-160`) is a second, independent site and the only one that needs the enum→histogram join.
Phase 1 lands the reader and the shared site; Phase 2 lands the second site.

No phase here is a frontend phase. `ui/` is the name of a TOML sidecar directory read by the C++
core; nothing in this milestone renders pixels, so no `UI hint` annotation appears below and
`/gsd-ui-phase` is not applicable.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

- [ ] **Phase 1: Sidecar Reader and Attribute Meaning** - Parse `ui/` from `from_migrations` and render label, tooltip and enum labels in `describe` / `describe_collection`, degrading silently when the sidecar is absent or broken
- [ ] **Phase 2: Enum Labels on the Value Histogram** - Annotate `summarize_collection`'s integer value distribution with each code's enum label

## Phase Details

### Phase 1: Sidecar Reader and Attribute Meaning
**Goal**: An agent calling `describe` or `describe_collection` on a PSR study database opened with `from_migrations` sees each scalar attribute's English label, tooltip and enum code→label list; a database with no `ui/`, or a broken one, reads exactly as it does today.
**Depends on**: Nothing (first phase)
**Requirements**: READ-01, READ-02, READ-03, READ-04, READ-05, RENDER-01, RENDER-03, SAFE-01, SAFE-02
**Success Criteria** (what must be TRUE):
  1. Opening a migrations tree that has a `ui/` sibling and calling `describe_collection` shows, appended after each scalar's `name (TYPE)` and its `PRIMARY KEY` / `NOT NULL` flags, that attribute's English label and tooltip — a UI string carrying a literal `\n` or `\r` arriving on one line, and a non-ASCII UTF-8 string (`hm³`, `°C`, a Portuguese accent) arriving byte-for-byte.
  2. An enum-bound attribute additionally shows its codes and labels (`0 = User Defined Forecast, 1 = Model`), taken from `enum.toml`, joined by the attribute's `enum` value rather than its `id`, with each code read from the entry's own `id` field — so a gapped vocabulary (`[0, 2]`) and a 1-based one (`[1..7]`) both render their real codes.
  3. A scalar the sidecar does not describe renders the line it renders today, in all three of the cases that occur in the corpus: its collection has no ui file, its own `[[attribute]]` entry is absent, and a ui entry names a column the schema does not have.
  4. `from_migrations` against a tree with no `ui/`, an empty `ui/`, a zero-byte `enum.toml`, or an unparseable `.toml` still opens the database — logging a warning for the malformed case — and all three reports come back byte-identical to a run of the same tree with the sidecar deleted.
  5. A migrations path with a trailing separator, and a relative migrations path, resolve to the same `ui/` directory as the absolute separator-free form — never `<migrations>/ui`, and never a `ui/` under the process CWD.
  6. Fixtures build `migrations/` and `ui/` in a per-test temp dir; nothing is committed under `tests/schemas/ui/`, and the existing `test_database_lifecycle.cpp` describe assertions (`:396-436`, `:438-460`, `:463-485`, `:487-500`) still pass unmodified.
**Plans**: 3 plans

Plans:
- [ ] 01-00-PLAN.md — Wave 0: temp-dir `migrations/` + `ui/` fixture harness and the SAFE-01 no-sidecar baseline test
- [ ] 01-01-PLAN.md — Wave 1: tracer slice (path resolve → shape-selected parse → `Impl` store → label/tooltip clauses in both reports), then the `enum.toml` vocabulary expansion
- [ ] 01-02-PLAN.md — Wave 2: path-resolution, shape-selection, undescribed and malformed-sidecar coverage, plus the `CHANGELOG.md` reconciliation and CLAUDE.md updates

Also in this phase (prerequisite for its changelog entry): reconcile `CHANGELOG.md`'s
`## [0.10.7] — unreleased` heading and its compare link to `0.10.8`, since `v0.10.7` is already
tagged and `CMakeLists.txt` is at `0.10.8`.

Non-negotiable implementation facts (measured, from PROJECT.md / research):
- The `ui/` read lives in `from_migrations` itself, **not** hooked onto `load_schema_metadata` —
  `migrate_up` early-returns at `src/database.cpp:398-401` and `:406-409` before reaching it, and
  Foresight's `load_study` hits the already-up-to-date return on every open.
- Path resolution is `fs::weakly_canonical(migrations_path).parent_path() / "ui"`.
- Collection files self-select by shape: a non-recursive scan of `ui/*.toml` keeping files with both
  a top-level string `id` and an `attribute` array. No `main.toml` parsing, no filename mapping.
- One accessor for all three localizable fields: a string is used as-is, a table is read at `en`.
- The whole load is one try/catch → `logger->warn` → empty map. Do **not** copy
  `src/binary/binary_metadata.cpp`'s throwing posture.
- The parser is a `.cpp` in `QUIVER_SOURCES` (toml++ is PRIVATE on `quiver`); the C++ suite drives
  it through the public API only, as `tests/test_binary_metadata.cpp` does.

### Phase 2: Enum Labels on the Value Histogram
**Goal**: `summarize_collection`'s integer value distribution reads as meanings rather than bare codes — the milestone's headline output for an agent inspecting a PSR study.
**Depends on**: Phase 1
**Requirements**: RENDER-02
**Success Criteria** (what must be TRUE):
  1. `summarize_collection` on a collection with an enum-bound INTEGER scalar renders each histogram entry with that code's label beside the code, using the map Phase 1 already built.
  2. A non-PK INTEGER column with no enum vocabulary, and an observed code the vocabulary does not cover, both keep today's bare-code entry — the annotation is per-code, not per-column.
  3. No injected label text introduces `Vectors:`, `Sets:`, `Time Series:` or a second `values {`, so the four brittle assertions in `tests/test_database_lifecycle.cpp` still pass unmodified.
**Plans**: TBD

Second injection site: `summarize_collection`'s scalar line at `src/database_describe.cpp:136` and
its histogram close at `:156-160`. It does not share `write_collection_section`, which is why this
is its own phase rather than a second edit inside Phase 1's.

## Progress

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Sidecar Reader and Attribute Meaning | 0/3 | Not started | - |
| 2. Enum Labels on the Value Histogram | 0/TBD | Not started | - |

## Requirement Coverage

| Requirement | Phase |
|-------------|-------|
| READ-01 | Phase 1 |
| READ-02 | Phase 1 |
| READ-03 | Phase 1 |
| READ-04 | Phase 1 |
| READ-05 | Phase 1 |
| RENDER-01 | Phase 1 |
| RENDER-02 | Phase 2 |
| RENDER-03 | Phase 1 |
| SAFE-01 | Phase 1 |
| SAFE-02 | Phase 1 |

10/10 mapped, each to exactly one phase. No orphans, no duplicates.

## Explicitly Not Phases

Named here so no later pass re-adds them: binding updates (all four describe suites assert only
"returns a string"), C API symbols (`describe*` already returns `std::string` through `new_c_str`),
a generator run, documentation-only work (CLAUDE.md folds docs into each change), and a standalone
testing phase (tests belong in the phase that writes the code).

---
*Roadmap created: 2026-09-20*
