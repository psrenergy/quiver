# Roadmap: Quiver — CSV reading for the Lua runner

## Overview

One reader feature in a mature library, delivered as three vertical slices. Phase 1 wires
`vincentlaucsb/csv-parser` into the core and puts both Lua entry points on top of it, so a script
running against a file-backed database can read a CSV off disk — sandboxed exactly like every other
Lua file operation. Phase 2 takes that reader from "works on a clean file" to "works on the files
the agent actually gets": BOM, CRLF, quoted separators and newlines, junk rows above the header,
repeated and blank header names — proven against the two real Maranhão CSVs that are currently being
transcribed by hand. Phase 3 closes the loop the milestone exists for: the shipped agent reference
stops saying there is no filesystem and starts telling the model to read the file, with a worked
example over a dirty one, verified in a Release build.

## Phases

**Phase Numbering:**

- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [x] **Phase 1: A Lua script reads a CSV file** - Parser wired into the core, both Lua entry points over it, sandboxed (completed 2026-09-16)
- [x] **Phase 2: The dirty files parse correctly** - BOM, CRLF, quoting, ragged rows, junk headers, repeated names — proven on the real files (completed 2026-09-16)
- [x] **Phase 3: The agent reads instead of transcribing** - Shipped reference redirects the model to the file; Release-verified and documented (completed 2026-09-16)

## Phase Details

### Phase 1: A Lua script reads a CSV file

**Goal**: A Lua script can read a CSV file off disk — whole-file or row-by-row — with every cell
arriving as a string and every path resolved against the database directory
**Mode:** mvp
**Depends on**: Nothing (first phase)
**Requirements**: PARSE-01, PARSE-08, PARSE-09, LUA-01, LUA-02, LUA-03, LUA-04, LUA-07, LUA-08, TEST-03, DOC-01
**Success Criteria** (what must be TRUE):

  1. A script calls `db:read_csv("data.csv")` against a file-backed database and gets the file's
     rows back, addressed positionally, with the header names available separately

  2. A script calls `db:read_csv_stream("data.csv", on_row)` and the callback fires once per row,
     with the process holding a bounded window rather than the whole file — and that window does
     not change with the host's CPU count

  3. Every cell arrives in Lua as a string: `"0012"` is still `"0012"`, a date is still text, and
     nothing has been coerced to a number

  4. A path escaping the database directory, an in-memory database, a missing file, and a directory
     given as a path each raise a `Cannot read_csv: ...` error naming the problem; a path into a
     subdirectory is accepted

  5. A file using a non-`,` separator reads correctly once the separator option is given, and the
     option defaults to `,`
**Plans**: 3/3 plans executed

Plans:
**Wave 1**

- [x] 01-01-PLAN.md — Tracer: csv-parser wired in, both Lua entry points over one reader, sandboxed

**Wave 2** *(blocked on Wave 1 completion)*

- [x] 01-02-PLAN.md — The `separator` option, strictly decoded, on both entry points

**Wave 3** *(blocked on Wave 2 completion)*

- [x] 01-03-PLAN.md — Sandbox negatives + full error catalogue proven, plus house paperwork

Notes:

- DOC-01 lands here because `bindings/js/test/lua-api-sync.test.ts` parses `src/lua_runner.cpp` and
  fails the build until every newly bound `db:` name appears as a literal token in
  `bindings/js/src/lua-api.ts`. Binding `db:read_csv` without documenting it breaks the build.
  Phase 2 extends the same entries with the header options; that is doc upkeep, not a new requirement.

- LUA-03 (one parser under both forms) is a structural property of this phase, not a later
  refactor: build the core reader once, put both entry points on it.

- Sandboxing reuses the existing `resolve_sandboxed_path` helper in `src/lua_runner.cpp` — no new
  machinery, which is why TEST-03 sits here rather than in a later verification pass.

### Phase 2: The dirty files parse correctly

**Goal**: The messy, arbitrary CSVs that arrive from real sources read correctly — including the two
real Maranhão files currently transcribed into scripts by hand
**Mode:** mvp
**Depends on**: Phase 1
**Requirements**: PARSE-02, PARSE-03, PARSE-04, PARSE-05, PARSE-06, PARSE-07, LUA-05, LUA-06, TEST-01, TEST-02, TEST-04
**Success Criteria** (what must be TRUE):

  1. `"May 1, 2014",33` reads as exactly two fields, a newline inside a quoted field does not split
     the record, and a doubled quote comes back as one quote

  2. A file with a UTF-8 BOM and CRLF endings yields a first header name with no BOM and no cell
     carrying a trailing `\r`; rows with differing field counts parse without throwing or truncating

  3. A script names which row is the header (or declares there is none), so a file with two junk
     rows above the header reads with the right column names

  4. A header row where `ANO` and `Residencial` each appear twice and several names are blank leaves
     every column reachable and none shadowing another

  5. Reading the two real Maranhão CSVs yields the same values the hand-transcribed script hard-coded

**Plans**: 4/4 plans executed

Plans:

**Wave 1**

- [x] 02-01-PLAN.md — Tracer: the `header_row` option end-to-end, with both csv-parser traps handled

**Wave 2** *(blocked on Wave 1 completion)*

- [x] 02-02-PLAN.md — The dirty-file matrix, repeated/blank header names, and the option negatives

**Wave 3** *(blocked on Wave 2 completion)*

- [x] 02-03-PLAN.md — The two real Maranhão files committed as fixtures, and their regression tests

**Wave 4** *(blocked on Wave 3 completion)*

- [x] 02-04-PLAN.md — Release-build run, plus the nearest-CLAUDE.md and changelog paperwork

Notes:

- PARSE-02 through PARSE-07 are acceptance properties of the chosen parser rather than code to
  write; the work here is the option surface (LUA-05, LUA-06) plus the fixture suite that proves
  the properties hold through the Lua boundary.

- TEST-02 needs the two real files copied into `tests/` as fixtures from
  `C:/Development/Claw/claw-experiments/Foresight/.claw/case-ma-2.foresight/a20a2fd08893/runs/run-007/`.

- TEST-04 asserts each bad option value *throws* — a silent fallback to a default is the failure
  mode being ruled out.

- New test files must be registered explicitly in `tests/CMakeLists.txt` (no glob); sandboxed Lua
  file tests use the `LuaSandboxTest` fixture in `tests/test_lua_runner.h`.

### Phase 3: The agent reads instead of transcribing

**Goal**: The shipped agent reference sends the model to the file instead of the clipboard, and the
feature is verified and documented well enough to release
**Mode:** mvp
**Depends on**: Phase 2
**Requirements**: DOC-02, DOC-03, DOC-04, TEST-05
**Success Criteria** (what must be TRUE):

  1. `LUA_DB_API_REFERENCE` tells the model to read data files from disk, stated where the reference
     currently tells it that it has no filesystem access

  2. The reference carries a worked example over a realistically dirty file, including the
     `tonumber`/`gsub` parenthesis trap that silently returns `nil`

  3. The full test suite passes in a Release build, not only Debug — `SOL_SAFE_GETTER` is off in
     Release and has hidden Lua marshalling bugs before

  4. The `CLAUDE.md` nearest each change describes the new surface, and `CHANGELOG.md` carries an
     entry under the current unreleased version
**Plans**: 2/2 plans executed

Plans:

**Wave 1**

- [x] 03-01-PLAN.md — The read-don't-transcribe instruction and the worked dirty-file example, run against the real fixtures before shipping

**Wave 2** *(blocked on Wave 1 completion)*

- [x] 03-02-PLAN.md — House paperwork (nearest CLAUDE.md files + changelog) and the Release gate over the finished tree

Notes:

- `LUA_DB_API_REFERENCE` is system-prompt payload interpolated into every `claw` session, so every
  documented knob costs tokens forever — the worked example earns its place, a tour of options does not.

- DOC-04 touches the root `CLAUDE.md` (Lua surface, cross-layer table) and `src/CLAUDE.md`, plus
  `tests/CLAUDE.md` for the new fixtures.

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2 → 3

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. A Lua script reads a CSV file | 3/3 | Complete    | 2026-09-16 |
| 2. The dirty files parse correctly | 4/4 | Complete    | 2026-09-16 |
| 3. The agent reads instead of transcribing | 2/2 | Complete    | 2026-09-16 |

## Requirement Coverage

| Requirement | Phase |
|-------------|-------|
| PARSE-01 | Phase 1 |
| PARSE-02 | Phase 2 |
| PARSE-03 | Phase 2 |
| PARSE-04 | Phase 2 |
| PARSE-05 | Phase 2 |
| PARSE-06 | Phase 2 |
| PARSE-07 | Phase 2 |
| PARSE-08 | Phase 1 |
| PARSE-09 | Phase 1 |
| LUA-01 | Phase 1 |
| LUA-02 | Phase 1 |
| LUA-03 | Phase 1 |
| LUA-04 | Phase 1 |
| LUA-05 | Phase 2 |
| LUA-06 | Phase 2 |
| LUA-07 | Phase 1 |
| LUA-08 | Phase 1 |
| DOC-01 | Phase 1 |
| DOC-02 | Phase 3 |
| DOC-03 | Phase 3 |
| DOC-04 | Phase 3 |
| TEST-01 | Phase 2 |
| TEST-02 | Phase 2 |
| TEST-03 | Phase 1 |
| TEST-04 | Phase 2 |
| TEST-05 | Phase 3 |

**Coverage:** 26/26 v1 requirements mapped. No orphans, no duplicates.

---
*Roadmap created: 2026-09-14*
