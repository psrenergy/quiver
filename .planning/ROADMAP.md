# Roadmap: Quiver

## Shipped Milestones

- **v1.0 — CSV reading for the Lua runner** (2026-09-16) — 3 phases, 9 plans, 26/26 requirements.
  A Lua script reads a real, dirty CSV off disk instead of having its contents transcribed into the
  script. Full record: [`milestones/v1.0-ROADMAP.md`](milestones/v1.0-ROADMAP.md) ·
  [`milestones/v1.0-REQUIREMENTS.md`](milestones/v1.0-REQUIREMENTS.md) ·
  [`v1.0-MILESTONE-AUDIT.md`](v1.0-MILESTONE-AUDIT.md)

## Current Milestone

**v1.1 — CSV writing for the Lua runner** (`db:write_csv`)

### Overview

One writer feature, delivered as two phases. The whole feature is a hand-rolled RFC-4180 emitter
over `std::ofstream` plus one sol2 usertype — no new dependency, no C API work, and no FFI binding
work in any of the four bindings, because `db:write_csv` rides inside the already-bound generic
`LuaRunner::run` path. Phase 4 delivers the writer and its Lua surface: a script creates a file in
the case folder, writes string / number / boolean / `nil` cells, and `db:read_csv` reads back
exactly what was written — with the agent reference updated in the same commit, because the
lua-api sync test is a hard build gate. Phase 5 covers the two ways the file is wrong even though
every row was accepted: a row shorter or longer than the declared `header`, and a script that
returns without calling `close()`.

Every success criterion below is a **round trip through the already-verified `db:read_csv`**, never
a string search over the output file. This project has twice named that trap — `export_csv` has 118
export-side tests that never re-import — and a writer suite built the same way would make it three.

### Phases

- [x] **Phase 4: A Lua script writes a CSV file** - The writer, its sandboxed Lua handle, and the agent reference (completed 2026-09-16)
- [ ] **Phase 5: Ragged rows and forgotten closes** - The header as width authority, and the flush at `run()`'s return

## Phase Details

### Phase 4: A Lua script writes a CSV file

**Goal**: A Lua script can create a CSV file in the database directory and write rows to it —
strings, numbers, booleans and `nil` cells — and `db:read_csv` over the same path returns exactly
what the script wrote
**Depends on**: Phase 3 (v1.0's `db:read_csv`, which every verification here round-trips through)
**Requirements**: WRITE-01, WRITE-02, WRITE-03, WRITE-04, WRITE-05, WRITE-07, WRITE-08, FMT-01, FMT-02, FMT-03, FMT-04, FMT-05, FMT-06, FMT-08, FMT-09, LUA-09, LUA-10, LUA-11, TEST-06, TEST-07, TEST-08, TEST-09, TEST-12, DOC-05
**Success Criteria** (what must be TRUE):

  1. A script calls `db:write_csv("out.csv", {separator = ..., header = {...}})`, appends rows with
     `w:write_row{...}`, calls `w:close()`, and `db:read_csv("out.csv")` returns exactly the cells
     the script passed — including one cell holding the separator, a bare `"`, a CR and an LF all
     at once, and one cell that is a single `"` on its own

  2. `w:write_row({9007199254740993})` reads back as `9007199254740993`, not `...992` — the digit
     that disappears the moment an int64 is routed through a `double` — and a float written, read
     back and re-written produces the identical text, so no value is cut to 5 decimal places or 6
     significant digits

  3. A non-finite number, a table cell, a `write_row` after `close`, a path escaping the database
     directory, and an `:memory:` database each raise a Pattern 1 error naming the problem, so no
     `inf`, `nan` or `-nan(ind)` token can reach a cell on any platform; `w:close()` called twice
     is not an error

  4. A single-column file whose cells are `nil` or `""` round-trips with every row present — none
     of them silently deleted as a blank line by the reader

  5. `LUA_DB_API_REFERENCE` documents `db:write_csv`, `w:write_row` and `w:close` — both options,
     the truncate-at-open behaviour, and a worked example that runs verbatim — and the lua-api sync
     gate passes with the binding

**Plans**: 3/3 plans executed

Plans:

- [x] 04-01-PLAN.md — Tracer: the writer, its sandboxed `db:write_csv` handle, the cell formatting
      rules, and the DOC-05 reference entry that must ship in the same commit (wave 1)

- [x] 04-02-PLAN.md — The error catalogue: the finiteness guard, the closed-writer guard, the
      missing-parent-directory failure, and the TEST-12 assertions (wave 2)

- [x] 04-03-PLAN.md — Round trips through `db:read_csv`: dirty cells, the int64 path, the worked
      example executed from the reference file, and the Release build (wave 3)

Notes:

- **DOC-05 must land with the binding, not after it.** `bindings/js/test/lua-api-sync.test.ts`
  parses `src/lua_runner.cpp` and fails the build until every newly bound `db:` name is a literal
  token in `bindings/js/src/lua-api.ts`. Splitting them makes this phase unbuildable. Note the
  verified blind spot: the test's usertype-method check runs over a hardcoded
  `["BinaryFile", "BinaryMetadata", "Expression"]` array, so the handle's own `write_row`/`close`
  are *not* gated — DOC-05 is the real guarantee, the array entry is a one-token courtesy.

- **FMT-02 and TEST-09 are inseparable.** TEST-09's single-column `nil`/`""` fixture is the only
  test that can see FMT-02 fail; every other round-trip fixture is multi-column, where an empty
  cell is never a blank line.

- **The ownership question the research left open is already closed by the scope trim.** Position B
  (a `shared_ptr` + `weak_ptr` registry plus a new `Database::log_warning`) existed only to emit
  the unclosed-writer warning, and that warning was declined. LUA-11 pins Position A: `unique_ptr`,
  `sol::no_constructor`, no explicit `__gc`, mirroring `db:open_file` at `src/lua_runner.cpp:431-441`.

- **No C API, no FFI, no new dependency.** csv-parser's `DelimWriter` was evaluated and rejected —
  compile-time delimiter template parameter, and a hardcoded 5-decimal float truncation. FMT-04
  reuses the existing `append_number` at `src/lua_runner.cpp:128-135` rather than re-deriving it.

- LUA-10 fixes the evaluation order up front (path resolves *before* the options table decodes),
  matching `db:read_csv`; LUA-09 uses the collect-then-validate shape, never a throw inside sol2's
  `for_each`. Both are established catalogue entries, not new decisions.

### Phase 5: Ragged rows and forgotten closes

**Goal**: A file written by an imperfect script — a row that does not match the header, a script
that never calls `close()` — still reads back complete and aligned
**Depends on**: Phase 4
**Requirements**: FMT-07, WRITE-06, TEST-10, TEST-11, DOC-06
**Success Criteria** (what must be TRUE):

  1. With a `header` of N names, a script writing a row of fewer than N cells produces a file whose
     every row `db:read_csv` returns with N fields, each value under the column the script meant —
     the common case, since a nullable read yields `nil` for a NULL

  2. A row longer than the header raises a Pattern 1 error naming the row ordinal and both counts,
     and the rows already written remain on disk

  3. With no `header` given, rows of differing widths are written as-is and no width error is raised

  4. A script that returns without calling `w:close()` leaves a complete, re-readable file —
     asserted after `LuaRunner::run` returns, with the `LuaRunner` still alive and undestroyed

  5. `src/CLAUDE.md`, the root `CLAUDE.md` and `CHANGELOG.md` record the writer and its design
     decisions

**Plans**: 3 plans

Plans:

- [ ] 05-01-PLAN.md — FMT-07: the header as row-width authority (pad short, throw long), the
      pinned Pattern 1 message and its catalogue line, and the TEST-10 boundary round trips
      (wave 1, leads with the tracer slice)

- [ ] 05-02-PLAN.md — WRITE-06: TEST-11 written and observed RED first with the actual byte count
      recorded, then the one-`collect_garbage()` scope guard in `LuaRunner::run` covering both
      returns and the throw path (wave 2)

- [ ] 05-03-PLAN.md — DOC-06: the four-file documentation record (`src/CLAUDE.md`, root
      `CLAUDE.md`, `CHANGELOG.md`, `bindings/js/src/lua-api.ts`), and the Release build where
      `SOL_SAFE_GETTER` is off (wave 3)

Notes:

- **WRITE-06 and TEST-11 are inseparable, and TEST-11 is a guaranteed red without it.** `sol::state`
  is a member of `LuaRunner::Impl`, so a writer left open at `run()`'s return is
  unreachable-but-uncollected and its buffer never flushes — the file is zero bytes. One
  `collect_garbage()` at `run()`'s end is the whole fix. Criterion 4 must assert *without*
  destroying the `LuaRunner`; destroying it would pass on the destructor alone and prove nothing.
  The *warning* is deliberately out of scope — flush only.

- **FMT-07 and TEST-10 are inseparable** for the same reason as FMT-02/TEST-09: the padded short
  row is only visible as a misalignment on read-back, and `db:read_csv`'s pinned `KEEP_NON_EMPTY`
  policy never pads, so an unpadded ragged row comes back silently misaligned against its own header.

- Width enforcement is separated from Phase 4 on purpose: Phase 4 writes what the script gives it,
  this phase makes the file right when the script is sloppy. FMT-08 (cell count from a single pass
  over integer keys, never `#t`) lands in Phase 4 because every row needs it; this phase is what
  compares that count against the header.

- DOC-06 is the milestone record, distinct from DOC-05's shipped agent payload. Note the standing
  `CHANGELOG.md` state: the unreleased section is `0.10.7` against manifests at `0.10.6`.

## Progress

**Execution Order:**
Phases execute in numeric order: 4 → 5

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 4. A Lua script writes a CSV file | 3/3 | Complete    | 2026-09-16 |
| 5. Ragged rows and forgotten closes | 0/3 | Planned      | - |

## Requirement Coverage

| Requirement | Phase |
|-------------|-------|
| WRITE-01 | Phase 4 |
| WRITE-02 | Phase 4 |
| WRITE-03 | Phase 4 |
| WRITE-04 | Phase 4 |
| WRITE-05 | Phase 4 |
| WRITE-06 | Phase 5 |
| WRITE-07 | Phase 4 |
| WRITE-08 | Phase 4 |
| FMT-01 | Phase 4 |
| FMT-02 | Phase 4 |
| FMT-03 | Phase 4 |
| FMT-04 | Phase 4 |
| FMT-05 | Phase 4 |
| FMT-06 | Phase 4 |
| FMT-07 | Phase 5 |
| FMT-08 | Phase 4 |
| FMT-09 | Phase 4 |
| LUA-09 | Phase 4 |
| LUA-10 | Phase 4 |
| LUA-11 | Phase 4 |
| TEST-06 | Phase 4 |
| TEST-07 | Phase 4 |
| TEST-08 | Phase 4 |
| TEST-09 | Phase 4 |
| TEST-10 | Phase 5 |
| TEST-11 | Phase 5 |
| TEST-12 | Phase 4 |
| DOC-05 | Phase 4 |
| DOC-06 | Phase 5 |

**Coverage:** 29/29 v1 requirements mapped. No orphans, no duplicates.

## Phase Numbering

- Integer phases (1, 2, 3): planned milestone work.
- Decimal phases (2.1, 2.2): inserted after the roadmap is written, when work is discovered
  mid-milestone.

- Numbering continues across milestones: v1.0 ended at Phase 3, so v1.1 starts at Phase 4.

---
*v1.1 roadmap created: 2026-09-16*
