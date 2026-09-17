# Requirements: Quiver — CSV writing for the Lua runner

**Milestone:** v1.1
**Defined:** 2026-09-16
**Core Value:** Every layer — C++, C, Julia, Dart, Python, JS and Lua — sees the same data under the
same rules, because all the logic lives in the C++ core and the bindings stay thin.

REQ-IDs continue v1.0's numbering. `WRITE-01` was already defined in v1.0's deferred list and is
promoted here unchanged.

## v1 Requirements

### Writer surface

- [x] **WRITE-01**: A script can write a CSV file into the database directory
- [x] **WRITE-02**: `db:write_csv(path, opts)` returns a writer handle; `w:write_row({...})` appends
      one row; `w:close()` finishes the file. Streaming only — there is deliberately no whole-file form

- [x] **WRITE-03**: The writer accepts exactly two options — `separator` (a single character,
      default `,`) and `header` (a list of column names, written as the first row). An unknown key,
      a wrong-typed value, or a multi-character separator is a Pattern 1 error naming `write_csv`

- [x] **WRITE-04**: A path resolves against the directory containing the database file and cannot
      escape it; an in-memory (`:memory:`) database rejects `write_csv` outright. Inherited policy —
      the requirement is that the writer uses it, not that new machinery is built

- [x] **WRITE-05**: `w:write_row` after `w:close` is a Pattern 1 error; `w:close` is idempotent
- [x] **WRITE-06**: A writer still open when `LuaRunner::run` returns is flushed to disk, so the file
      is complete even if the script never called `close()`. No warning is emitted — this is the
      flush alone, not the diagnostic

- [x] **WRITE-07**: A missing parent directory is not created; the open fails and says so
- [x] **WRITE-08**: `write_csv` truncates an existing target at open — stated behaviour, not a guard

### Cell formatting and RFC 4180

- [x] **FMT-01**: A cell is quoted iff it contains the configured separator, `"`, CR or LF; an
      internal `"` is escaped by doubling it

- [x] **FMT-02**: An empty cell that is the row's only cell is written quoted (`""`). Unquoted, that
      row is a blank line, and this project's own reader deletes it — `src/csv_read.cpp` pins
      `KEEP_NON_EMPTY` and `csv_reader.cpp:107` discards a zero-field row

- [x] **FMT-03**: Every record is terminated, including the last, with LF. The stream opens in
      `std::ios::binary` so Windows text-mode translation cannot silently turn that LF into CRLF

- [x] **FMT-04**: Numbers are written with `std::to_chars` shortest round-trip, reusing the existing
      `append_number` helper (`src/lua_runner.cpp:128-135`). Lua 5.4's integer subtype is preserved,
      and an int64 never routes through `double` first

- [x] **FMT-05**: A non-finite float is a Pattern 1 error naming the row ordinal and cell index. No
      `inf` / `nan` text ever reaches a cell — MSVC and libstdc++ render those differently, so
      verbatim output would make the platform natives disagree on the same data

- [x] **FMT-06**: A boolean writes as `1`/`0`; a `nil` writes as an empty cell; a table, function or
      userdata is a Pattern 1 error naming the cell index

- [x] **FMT-07**: With a `header`, the header's length is the row width — a shorter row pads with
      empty cells, a longer one throws naming the row ordinal and both counts. With no `header`, no
      width check is performed

- [x] **FMT-08**: A row's cell count comes from a single pass over integer keys, never `#t` — which
      is undefined on a table with a trailing hole, the shape every nullable read produces

- [x] **FMT-09**: No BOM; the quote character is `"`; output is UTF-8. None of these is an option

### Lua binding

- [x] **LUA-09**: The options table is decoded with the collect-entries-then-validate shape used by
      `read_csv_options_from_lua`, never a throw inside sol2's `for_each` (D-17)

- [x] **LUA-10**: The sandboxed path resolves *before* the options table is decoded, matching
      `db:read_csv`'s established evaluation order (D-22)

- [x] **LUA-11**: The handle is a sol2 usertype owned by `std::unique_ptr`, registered with
      `sol::no_constructor` and no explicit `__gc`, mirroring `db:open_file`
      (`src/lua_runner.cpp:431-441`, `:565-579`)

### Verification

- [x] **TEST-06**: A round trip through `db:read_csv` recovers, byte-identically, a cell containing
      the separator, a bare quote, CR, LF, and all four combined in one cell

- [x] **TEST-07**: `w:write_row({9007199254740993})` reads back `9007199254740993`, not `...992` —
      the only case that distinguishes the int64 path from the double path

- [x] **TEST-08**: A field that is exactly one `"` serializes to four quote characters
- [x] **TEST-09**: A single-column file whose cells are `nil` or `""` round-trips with every row
      present — the FMT-02 regression, invisible to any multi-column fixture

- [x] **TEST-10**: A short row against a multi-column header round-trips aligned; a longer row throws
- [x] **TEST-11**: A script that returns without calling `close()` leaves a complete, re-readable
      file, asserted after `run()` returns without destroying the `LuaRunner`

- [x] **TEST-12**: A non-finite value, a table cell, a write after close, an out-of-sandbox path, and
      an in-memory database each produce their Pattern 1 error

### Agent guidance

- [x] **DOC-05**: `LUA_DB_API_REFERENCE` documents `db:write_csv`, `w:write_row` and `w:close` — the
      two options, the strict-width rule, that the target is truncated at open, and a worked example
      verified to run. Lands in the same commit as the binding

- [x] **DOC-06**: `src/CLAUDE.md`, root `CLAUDE.md` and `CHANGELOG.md` record the writer and its
      design decisions

## v2 Requirements

Deferred. Tracked but not in this roadmap.

### TOML

- **TOML-01**: A script can read a `.toml` file from the database directory
- **TOML-02**: A script can write a `.toml` file into the database directory

Same justification as CSV — Lua has no `io`, and a case folder holds `.toml` sidecars. `toml++` 3.4.0
is already a FetchContent dependency that `BinaryMetadata::from_toml_content` already uses, so the
cost is small. Out of scope in v1.1 only to keep this milestone to one format.

### Core unification

- **UNIFY-01**: `import_csv` reads through the same parser, deleting the global `;`→`,` replace and
  the quote-unaware trailing-comma stripper that corrupt quoted fields today

- **UNIFY-02**: `CSVOptions` carries a `separator`, replacing the pre-pass hack
- **UNIFY-03**: Import-side quoting tests exist, written against current behaviour *before* any
  parser migration

### Write throughput

- **PERF-01**: Group inserts prepare one statement and reuse it, instead of rebuilding and
  re-preparing identical SQL per row

- **PERF-02**: A bounded-memory append path exists, so a large dataset can be written without holding
  every row

## Out of Scope

| Feature | Reason |
|---------|--------|
| A destructive-overwrite guard | Explicitly declined. A script can write onto the live `.db`, a `.qvr`, or a migration `.sql` inside the sandbox and destroy it — verified silent on Windows as well as POSIX, contrary to the research note that claimed Windows would fail loudly. Accepted: `db:open_file('w')` already has the same hole, and the project's stated philosophy is to assume callers obey contracts. WRITE-08 documents the truncation instead |
| A warning when a writer is left unclosed | Dropped in exchange for the simple `unique_ptr` + default `__gc` lifetime. Emitting it requires a `weak_ptr` registry swept at `run()`'s exit plus a new `Database::log_warning`, since `LuaRunner` has no logger at all. WRITE-06 keeps the flush, which is the part that mattered |
| A whole-file `db:write_csv` form | Writing is streaming-only by decision. A second code path is a second thing that can diverge from the first |
| Atomic write-then-rename | Conflicts with WRITE-08's stated truncate-at-open behaviour, and costs more than the clause in the reference that warns about it |
| `append`, `line_ending`, quote-char, quoting-mode, dialect, encoding and BOM options | Every option is permanent `LUA_DB_API_REFERENCE` payload interpolated into every `claw` session. None earns it; the fixed constants in FMT-03 and FMT-09 are what you get by writing nothing |
| `is_open()` and a row-count getter | Same prompt-weight argument. A script that needs a row count keeps its own |
| Getting data out of the database and into the rows | The script's job, symmetric with the reader. `db:write_csv` takes whatever rows the script assembled |
| Exposing CSV writing in Julia/Dart/Python/JS or the C API | Those hosts all have native CSV libraries. Lua needs this precisely because `io` is deliberately absent. It also needs no C API work at all — `db:write_csv` rides inside the already-bound generic `LuaRunner::run` path |
| Relying on `lua-api-sync.test.ts` to gate the handle's methods | Its usertype check is receiver-agnostic (`includes(':name(')`) and `BinaryFile` already puts `f:close(` in the reference, so adding `Writer` to its hardcoded array covers `write_row` only. DOC-05 is the real guarantee; the array edit is one token, not a gate |
| Migrating `import_csv`/`export_csv` off rapidcsv | Deferred, unchanged from v1.0 — `import_csv` is destructive with zero import-side quoting tests to migrate against |
| A multi-GB end-to-end test fixture | Expensive to build and proves less than the dirty-cell cases |
| Numeric or date inference on cells | Symmetric with the reader — a silent coercion is worse than an explicit `tostring` |

## Traceability

Which phases cover which requirements. Filled during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| WRITE-01 | Phase 4 | Complete |
| WRITE-02 | Phase 4 | Complete |
| WRITE-03 | Phase 4 | Complete |
| WRITE-04 | Phase 4 | Complete |
| WRITE-05 | Phase 4 | Complete |
| WRITE-06 | Phase 5 | Complete |
| WRITE-07 | Phase 4 | Complete |
| WRITE-08 | Phase 4 | Complete |
| FMT-01 | Phase 4 | Complete |
| FMT-02 | Phase 4 | Complete |
| FMT-03 | Phase 4 | Complete |
| FMT-04 | Phase 4 | Complete |
| FMT-05 | Phase 4 | Complete |
| FMT-06 | Phase 4 | Complete |
| FMT-07 | Phase 5 | Complete |
| FMT-08 | Phase 4 | Complete |
| FMT-09 | Phase 4 | Complete |
| LUA-09 | Phase 4 | Complete |
| LUA-10 | Phase 4 | Complete |
| LUA-11 | Phase 4 | Complete |
| TEST-06 | Phase 4 | Complete |
| TEST-07 | Phase 4 | Complete |
| TEST-08 | Phase 4 | Complete |
| TEST-09 | Phase 4 | Complete |
| TEST-10 | Phase 5 | Complete |
| TEST-11 | Phase 5 | Complete |
| TEST-12 | Phase 4 | Complete |
| DOC-05 | Phase 4 | Complete |
| DOC-06 | Phase 5 | Complete |

**Coverage:**

- v1 requirements: 29 total
- Mapped to phases: 29
- Unmapped: 0

---
*Requirements defined: 2026-09-16*
