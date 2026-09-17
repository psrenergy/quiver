---
gsd_state_version: 1.0
milestone: v1.1
milestone_name: CSV writing for the Lua runner
current_phase: 5
current_phase_name: Ragged rows and forgotten closes
status: verifying
stopped_at: Milestone v1.1 complete and archived
last_updated: "2026-09-17T14:02:14.011Z"
last_activity: 2026-09-16
last_activity_desc: v1.1 roadmap created (2 phases, 29/29 requirements mapped)
progress:
  total_phases: 2
  completed_phases: 2
  total_plans: 6
  completed_plans: 6
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-09-16)

**Core value:** Every layer — C++, C, Julia, Dart, Python, JS and Lua — sees the same data under the same rules, because all the logic lives in the C++ core and the bindings stay thin.
**Current focus:** Phase 04 — a-lua-script-writes-a-csv-file

## Current Position

Phase: 5 — Ragged rows and forgotten closes
Plan: 3 of 3
Status: Phase complete — ready for verification
Last activity: 2026-09-16 — Phase 04 complete, transitioned to Phase 5

## Performance Metrics

**Velocity:**

- Total plans completed: 12
- Average duration: —
- Total execution time: —

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 01 | 3 | - | - |
| 02 | 4 | - | - |
| 03 | 2 | - | - |
| 04 | 3 | - | - |
| 05 | — | - | - |

**Recent Trend:**

- Last 5 plans: —
- Trend: —

*Updated after each plan completion*
**Per-Plan Metrics:**

| Plan | Duration | Tasks | Files |
|------|----------|-------|-------|
| Phase 01 P01 | 20min | 2 tasks | 8 files |
| Phase 01 P02 | 25min | 2 tasks | 3 files |
| Phase 01 P03 | 45min | 2 tasks | 5 files |
| Phase 02 P01 | 45min | 3 tasks | 6 files |
| Phase 02 P02 | 25min | 3 tasks | 1 files |
| Phase 02 P03 | ~35min | 3 tasks | 4 files |
| Phase 02 P04 | 20min | 2 tasks | 1 files |
| Phase 03 P01 | 25min | 2 tasks | 1 files |
| Phase 03 P02 | 40min | 2 tasks | 3 files |
| Phase 04 P01 | 50min | 3 tasks | 11 files |
| Phase 04 P02 | 55min | 3 tasks | 4 files |
| Phase 04 P03 | 45min | 3 tasks | 2 files |
| Phase 05 P01 | 25min | 2 tasks | 3 files |
| Phase 05 P02 | 12min | 2 tasks | 2 files |
| Phase 05 P03 | 25min | 2 tasks | 4 files |

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table. Affecting current work (v1.1):

- The writer is **hand-rolled**, ~90 lines of RFC-4180 emission over `std::ofstream`. No new
  dependency. csv-parser's `DelimWriter` was read and rejected: compile-time delimiter/quote
  template parameters (`csv_writer.hpp:254`) cannot serve a runtime `separator`, and its float
  `to_string` truncates at `DECIMAL_PLACES = 5` (`:25`)

- Writing is **streaming-only** — `db:write_csv` → handle, `w:write_row`, `w:close`. No whole-file
  form; a second code path is a second thing that can diverge

- **Two options and no more**: `separator` and `header`. `LUA_DB_API_REFERENCE` is system-prompt
  payload interpolated into every `claw` session, so every knob costs tokens forever

- **Ownership is settled as Position A** (`unique_ptr`, `sol::no_constructor`, default `__gc`,
  mirroring `db:open_file`). The research left A-vs-B open only because of the unclosed-writer
  warning; that warning was declined, so no `weak_ptr` registry and no `Database::log_warning`

- **No overwrite guard** (declined; WRITE-08 documents truncate-at-open instead) and **no unclosed-
  writer warning** (declined; WRITE-06 keeps the flush, which is the part that mattered)

- **Lua only** — no public C++ header, no C API, no FFI binding work. `db:write_csv` rides inside
  the already-bound generic `LuaRunner::run` path

- Numbers are formatted by Quiver with `std::to_chars` shortest round-trip, reusing `append_number`
  (`src/lua_runner.cpp:128-135`); a non-finite float throws rather than emitting `inf`/`nan`

Carried from v1.0:

- Parser is `vincentlaucsb/csv-parser` 5.3.0 via FetchContent, speculative-parallel and SIMD off
- Two Lua entry points over one parser, so whole-file and streaming cannot diverge
- Writing parsed rows into the database stays the script's job
- Core CSV handling (`import_csv`/`export_csv`, still on rapidcsv) is not unified in this milestone
- csv-parser 5.3.0 wired in (threads/SIMD forced off); `db:read_csv` and `db:read_csv_stream` share
  one internal `csv_read::Reader` (no public header)

- D-22 evaluation order: the sandboxed path resolves *before* the options table is decoded
- `header_row` is 1-based at the Lua boundary, 0 = no header, default 1 (D-20); `make_format` sets
  header mode before `variable_columns(KEEP_NON_EMPTY)`

- `R"LUA(...)LUA"` custom raw-string delimiter is required whenever an embedded Lua pattern literal
  ends in the two-char sequence `)"`

- Release build must be exercised separately: `SOL_SAFE_GETTER` is off in Release and has hidden
  Lua marshalling bugs before

- [Phase ?]: WRITE-07 (parent-dir-missing precondition) implemented in csv_write::Writer's constructor per the plan's task 1 action text, though absent from this plan's requirements frontmatter
- [Phase ?]: Task 1 (tracer) deliberately implemented only nil/string cell dispatch and zero recognized write_csv option keys, so task 2's TDD RED phase produced 9 genuinely failing tests before implementation
- [Phase ?]: FMT-05/WRITE-05/WRITE-07 catalogue complete: closed-writer and missing-parent-directory checks were already correct from 04-01; 04-02 added the isfinite guard and fixed a cell-formatted-before-closed-check ordering bug
- [Phase ?]: Executed the std::to_chars non-finite spot-check on MSVC 19.51.36256: 0.0/0.0 spells -nan(ind) (9 bytes), its negation spells nan (3 bytes), 1.0/0.0 spells inf, -1.0/0.0 spells -inf -- all errc=0
- [Phase ?]: 04-03: TEST-06/07/08 dirty-cell, quote-doubling, and int64/float numeric round trips proven, plus the DOC-05 worked example executed by extraction from the reference file; Release build (1224/1224, 557/557) confirms no SOL_SAFE_GETTER-off regression
- [Phase ?]: 05-01: header_width is a std::size_t on CsvWriter seeded once from csv_options.header.size(); the decoded header vector itself is discarded (D-42)
- [Phase ?]: 05-01: the row-too-wide reject branch sits ahead of the pad branch in one if (header_width != 0) guard; a long row is never truncated
- [Phase ?]: 05-01: the pinned FMT-07 message and its csv_write.cpp catalogue comment landed in the same commit as the throw, per that file's own no-reword-without-updating-the-test rule (D-45)
- [Phase ?]: 05-02: GcGuard declared before safe_script so reverse-declaration-order destruction runs collect_garbage() after result's Lua stack reference is released (D-46)
- [Phase ?]: 05-02: TEST-11 observed genuinely RED with 0-byte files on both fixtures before the fix, confirming the ROADMAP's claim rather than repeating it (D-49)
- [Phase ?]: 05-03: root CLAUDE.md's db:read_csv bullet extended in place (D-53) to record the writer, FMT-07, and WRITE-06; CSV file write row added to the cross-layer table beside CSV file read (D-52)
- [Phase ?]: 05-03: Release build (SOL_SAFE_GETTER off) in a dedicated build-release/ tree confirms quiver_tests 1234/1234 and quiver_c_tests 557/557, up from 04-03's 1224/1224 -- no shrinkage

### Pending Todos

None yet.

### Blockers/Concerns

- `bindings/js/test/lua-api-sync.test.ts` is a hard build gate: it parses `src/lua_runner.cpp` and
  fails until every newly bound `db:` name is a literal token in `bindings/js/src/lua-api.ts`.
  **Phase 4 must land DOC-05 alongside the binding, not after it.** Its usertype-method check runs
  over a hardcoded `["BinaryFile", "BinaryMetadata", "Expression"]` array, so `w:write_row` /
  `w:close` are *not* gated by it — DOC-05 is the real guarantee.

- The weak-test trap, for the third time: `export_csv` has 118 export-side string-search tests that
  never re-import. Every v1.1 verification criterion is a round trip through `db:read_csv` instead.

- `std::to_chars` on a non-finite double is the one MEDIUM-confidence research claim — verified
  against standards text, never executed. Spot-check `0.0/0.0` and `1.0/0.0` before relying on it
  (FMT-05 throws either way, so the guard is required regardless of what it renders).

- `lua_runner.cpp` already needs `/bigobj` on MSVC for sol2's template depth. Nothing
  template-heavy is being added, so this is a watch item, not a risk here.

- Pre-existing and out of scope, but noted: `import_csv`'s global `;`→`,` replace and
  quote-unaware trailing-comma stripper are live data-corruption paths; `CHANGELOG.md`'s unreleased
  section is `0.10.7` while all five manifests say `0.10.6`.

## Deferred Items

| Category | Item | Status | Deferred At |
|----------|------|--------|-------------|
| TOML | `TOML-01`/`TOML-02` — a Lua TOML reader and writer (toml++ already vendored) | v2 | v1.1 requirements |
| Core unification | `UNIFY-01..03` — `import_csv` onto the shared parser, `separator` in `CSVOptions`, import-side quoting tests | v2 | v1.1 requirements |
| Write throughput | `PERF-01`/`PERF-02` — one prepared statement per group insert, a bounded-memory append path | v2 | v1.1 requirements |

## Session Continuity

Last session: 2026-09-17T14:02:13.982Z
Stopped at: Milestone v1.1 complete and archived
Resume file: .planning/v1.1-MILESTONE-AUDIT.md
