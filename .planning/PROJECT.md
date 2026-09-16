# Quiver

## What This Is

Quiver is a SQLite wrapper library with a C++ core, a C API for FFI, and five language
bindings — Julia, Dart, Python, JS/Bun, plus an embedded Lua runner. It gives modeling tools a
schema-validated, typed way to keep collections, vectors, sets and time series in SQLite without
hand-writing SQL, and it is published to npm, PyPI, pub.dev and the Julia registry.

The embedded Lua runner is what makes this milestone necessary: it is how an LLM agent (`claw`)
edits a study database, and it deliberately has no `io` library, so a script cannot read a file.

## Core Value

Every layer — C++, C, Julia, Dart, Python, JS and Lua — sees the same data under the same rules,
because all the logic lives in the C++ core and the bindings stay thin.

## Current Milestone: v1.1 CSV writing for the Lua runner

**Goal:** A Lua script can write a CSV file into the case folder — streaming, correctly quoted,
with numbers that round-trip exactly.

**Target features:**
- `db:write_csv(path, opts)` returns a writer handle; `w:write_row({...})` appends a row;
  `w:close()` finishes the file. **Streaming only** — no whole-file counterpart to `db:read_csv`.
- Sandboxed to the database directory like every other Lua file operation; in-memory databases
  reject it outright.
- Two options and no more: `separator` (default `,`) and `header` (column names, written as the
  first row). Every option is permanent `LUA_DB_API_REFERENCE` payload.
- Cell values: string, number, boolean (→ `1`/`0`, the project-wide write policy), `nil` (→ empty
  cell). A table, function, or userdata in a row is a Pattern 1 error naming the cell index.
- The header is the row-width authority: a shorter row pads with empty cells, a longer one throws
  naming the row ordinal and both counts. With no `header` given, no width check is performed.
- Numbers written with `std::to_chars` shortest round-trip; Lua 5.4's integer subtype preserved. A
  non-finite float is a Pattern 1 error — never `inf`/`nan` text in a cell.
- An empty cell that is a row's only cell is written quoted, so the row is not a blank line.
- A writer still open when `LuaRunner::run` returns is flushed to disk. No warning is emitted.
- Lua only — no public C++ header, no C API, no FFI binding. Same rationale as `db:read_csv`.
- The agent-facing reference (`bindings/js/src/lua-api.ts`) updated in the same phase as the
  binding, since `lua-api-sync.test.ts` is a hard build gate.

## Requirements

### Validated

<!-- Inferred from the existing codebase (.planning/codebase/, 2026-09-14). -->

- ✓ Schema-validated SQLite storage: collections, scalar attributes, vector/set/time-series groups — existing
- ✓ A C API covering the public C++ surface, for FFI — existing
- ✓ Five bindings with mechanically consistent naming across layers — existing
- ✓ Embedded Lua runner (sol2) with a deliberately `io`-free, sandboxed scripting surface — existing
- ✓ Whole-database CSV import/export in Quiver's own format — existing
- ✓ Binary `.qvr` file I/O and a lazy expression DAG over it (Julia and Lua only, by decision) — existing
- ✓ Migrations with up/down paths and round-trip validation — existing
- ✓ Six test suites plus a CLI smoke test, over one shared schema set — existing
- ✓ A Lua script can read a CSV file from disk, sandboxed to the database directory like every other Lua file operation — validated in Phase 1: A Lua script reads a CSV file
- ✓ Two entry points over one parser: a whole-file form for the common case, and a row-streaming form that holds bounded memory on a large file — validated in Phase 1: A Lua script reads a CSV file
- ✓ Parsing is correct on genuinely dirty input — UTF-8 BOM, CRLF, a separator inside a quoted field, a newline inside a quoted field, doubled-quote escapes, ragged rows, junk rows above the header, duplicate and empty header names, configurable separator — validated in Milestone v1.0 (Phase 2)
- ✓ The agent-facing Lua reference (`bindings/js/src/lua-api.ts`) tells the model to read data files rather than transcribe them into the script — validated in Milestone v1.0 (Phase 3)
- ✓ Small-fixture tests covering every dirty case above, including a regression over the two real Maranhão CSVs — validated in Milestone v1.0 (Phase 2)

### Active

Milestone v1.1 — CSV writing for the Lua runner. Requirements are scoped in
`.planning/REQUIREMENTS.md`; the shape is in Current Milestone above.

### Out of Scope

- **A TOML reader and writer for Lua** — deferred, not rejected. The same argument that justifies
  `db:read_csv`/`db:write_csv` applies (Lua has no `io`, and a case folder holds `.toml` sidecars),
  and toml++ 3.4.0 is already a FetchContent dependency that `BinaryMetadata::from_toml_content`
  already uses, so the cost is small. Out of scope here only to keep this milestone to one format.
- **A destructive-overwrite guard** — declined; see the Key Decisions row. `WRITE-08` documents the
  truncate-at-open behaviour instead of guarding against it.
- **A warning when a writer is left unclosed** — declined in exchange for the simple `unique_ptr` +
  default `__gc` lifetime. The *flush* is kept (`WRITE-06`); only the diagnostic is gone.
- **A whole-file `db:write_csv` form** — writing is streaming-only by decision. Reading has both
  forms because a whole-file read is the common case and the shape an LLM gets right first try; a
  write is naturally incremental, and a second code path is a second thing that can diverge.
- **Getting data out of the database and into the rows** — that is the script's job, symmetric with
  the reader. `db:write_csv` takes whatever rows the script assembled.
- **The GB-scale write path** — `Database::execute` prepares and finalizes a statement on every call, and the group-insert loop rebuilds identical SQL per row, so even the bulk writers do one prepare per row and need every row in memory. Real, pre-existing, affects every caller, and deserves its own milestone rather than riding along with the reader.
- **Migrating `import_csv`/`export_csv` off rapidcsv** — staged separately. `import_csv` is a destructive path (`DELETE FROM` before insert) with zero import-side quoting tests today, and `export_csv` gains nothing from streaming because `Result` is already a fully materialized `std::vector<Row>` before the CSV layer is reached.
- **Replacing the binary subsystem's `CSVConverter` parser** — it reads a fixed-shape numeric format it generates itself, with its own NaN-sentinel routing. No user-facing quoting bug class there.
- **A multi-GB end-to-end test fixture** — expensive to build and it proves less than the dirty-input cases, which is where correctness actually lives.

## Context

**What prompted v1.1.** v1.0 gave a Lua script a way to read a file; it still has no way to write
one. A script that assembles an input file for another tool, or dumps intermediate state for
inspection, has nowhere to put it — the only channel out of a script is `LuaRunner::run`'s
JSON-encoded return value, which goes back to the model, not onto disk. v1.0 deferred writing for
want of a concrete case; building input files in the case folder is that case.

**What the writer cannot reuse.** csv-parser is already vendored and its quoting state machine is
correct (RFC 4180, doubles internal quotes), but two things in `csv_writer.hpp` do not fit:
`DelimWriter<OutputStream, Delim, Quote>` takes the delimiter and quote character as *compile-time*
template parameters, so a runtime `separator` needs a switch over instantiations or a hand-rolled
writer; and its numeric `to_string` truncates floats at 5 decimal places. Whichever way the first
is settled, numeric formatting stays Quiver's.

**Settled after research.** `nil` → empty cell makes `#t` unreliable on a row table with holes, so a
row's cell count comes from a single pass over integer keys and is checked against the declared
`header`; with no `header`, no width check runs at all. The scope was reviewed and trimmed after
research: the overwrite guard and the unclosed-writer warning were both declined, and the writer is
a hand-rolled ~90 lines with no new dependency.

**What prompted v1.0.** `claw` drives Quiver's Lua runner to build and edit energy-modeling study
databases. Because Lua has no `io`, the only way to get an input CSV into a database has been for
the model to retype the file into the script as a string literal. In the `case-ma-2.foresight`
case that is **360 lines of transcribed data in a 500-line script**, re-emitted verbatim in every
run from run-002 through run-007 — the pattern is the steady state, not a one-off failure.

**The input files are dirty and arbitrary.** They arrive from many sources, which is exactly why
they cannot arrive as `.qvr`. The two real examples carry a UTF-8 BOM, CRLF, eleven columns of
which six are empty padding, two side-by-side data blocks, two junk rows above the header, a
header row where `ANO` and `Residencial` each appear twice and several names are blank, values
like `1'114'144`, `DD/MM/YYYY` dates, and — in the second file — quoted fields containing commas
(`"May 1, 2014",33`), which any hand-rolled split on `,` gets silently wrong.

**They can also be very large.** Multi-GB CSVs are a real scenario, so the parser has to stream.
The incumbent, rapidcsv, cannot: it materializes the whole file into a
`vector<vector<std::string>>`, measured at 6.2x amplification.

**Claw already has CSV handling — on the other side.** `claw2/src/core/csv.ts` uses papaparse to
read model *result* CSVs back out, with row caps for LLM safety. There is no equivalent inbound
path, and the inbound transformation is model-authored per file (which columns, which date
format, how to strip separators), which is why this belongs in the Lua script rather than in a
TypeScript import tool with a configuration DSL.

**Known issues found while scoping, all pre-existing and all out of scope here:**
- `import_csv` does an unconditional `std::replace(content, ';', ',')` over the whole buffer, rewriting every semicolon inside every quoted TEXT value, and strips trailing commas with a quote-unaware pass that can delete a comma from inside a quoted multi-line field. Two live data-corruption paths.
- There are 118 CSV tests and none assert import-side quoting; the RFC-4180 tests are export-side string searches that never re-import.
- The rapidcsv pin (v8.92) predates a fix for an out-of-bounds in iterator arithmetic; v9.07 has it.
- `CHANGELOG.md`'s unreleased heading says `0.10.4` while all five version manifests say `0.10.6`.

## Constraints

- **Tech stack**: C++20, CMake >= 3.26, dependencies via FetchContent — every existing dependency is pulled that way and a new one must fit the same pattern.
- **Licensing**: Quiver is MIT and ships *prebuilt binaries* to PyPI, npm, Julia artifacts and S3. A copyleft dependency would attach a relink obligation to every release, so LGPL and stronger are disqualifying. This is what eliminated libcsv.
- **Compatibility**: MSVC on Windows is the primary development platform; Linux and macOS with a 13.3 deployment floor must also build.
- **Architecture**: logic lives in the C++ core, bindings stay thin. A new dependency must be linkable `PRIVATE` — it must not surface in a public header.
- **Security**: every file-touching Lua operation resolves against the database file's directory and cannot escape it; in-memory databases reject file operations outright. A CSV reader inherits this without exception.
- **Prompt weight**: `LUA_DB_API_REFERENCE` is shipped system-prompt payload interpolated into every `claw` session. Every documented option costs tokens forever, so the API surface has to earn each knob.
- **Build cost**: `lua_runner.cpp` already needs `/bigobj` on MSVC because of sol2's template depth, so a heavily templated header-only library landing in that translation unit is a concrete risk, not a theoretical one.

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Adopt `vincentlaucsb/csv-parser` 5.3.0, with speculative parallel and SIMD disabled | The only candidate passing every hard requirement with a verified RFC-4180 state machine and real cross-chunk quote carry. Its parallel scheduler is on by default above 50 MB and materializes a `chunk_size x worker_count` window, which buys nothing for a row-at-a-time reader | — Pending |
| Expose both a whole-file and a streaming Lua entry point, over one parser | Whole-file is the common case and the shape an LLM gets right first try; streaming is the only thing that works on a GB file. One parser underneath means the two cannot diverge in behaviour | — Pending |
| Read only; defer CSV writing | No concrete case yet, and the script's JSON return value already carries structured data back to the host | — Pending |
| Writing into the database stays the script's job | Correct separation of concerns — the reader yields rows, the existing group writers consume them | — Pending |
| Do not unify the core's CSV handling in this milestone | `import_csv` is destructive and has no import-side quoting tests to migrate against; `export_csv` cannot benefit from streaming while `Result` is fully materialized | — Pending |
| Verify with small dirty fixtures rather than a multi-GB file | The dirty cases are where correctness lives; a GB fixture costs more and proves less | — Pending |
| **v1.1** — Writing is streaming-only: a handle with `w:write_row` / `w:close`, no whole-file form | A write is naturally incremental, and a second code path is a second thing that can diverge from the first | — Pending |
| **v1.1** — Numbers formatted by Quiver with `std::to_chars`, never by csv-parser | csv-parser's writer truncates floats at 5 decimal places (`DECIMAL_PLACES = 5`, hand-rolled `pow10`/`modf`) — the same bug class `database_csv_export.cpp` already hit with `%g` | — Pending |
| **v1.1** — Two options only: `separator` and `header` | `LUA_DB_API_REFERENCE` is system-prompt payload interpolated into every `claw` session; `append` and `line_ending` did not earn permanent token cost | — Pending |
| **v1.1** — The header is the row-width authority; short rows pad, long rows throw | A nullable read (`read_scalar_strings` yields `nil` for NULL) makes short rows the common case, not a mistake. Only padding produces a file `db:read_csv` reads back aligned — its pinned `KEEP_NON_EMPTY` policy never pads and never drops, so a ragged row returns silently misaligned against its own header | — Pending |
| **v1.1** — Cell values: `nil` → empty, boolean → `1`/`0`, table/function/userdata → error | The boolean mapping matches the project-wide write policy; erroring beats writing `table: 0x...` into a data file | — Pending |
| **v1.1** — A non-finite float throws rather than writing `inf`/`nan` | MSVC's `<charconv>` renders indefinite NaN as `-nan(ind)`, libstdc++/libc++ as `-nan`. Verbatim output would make the platform natives write different bytes for the same script and the same data, and silently un-number a column for every downstream reader | — Pending |
| **v1.1** — An empty cell that is a row's only cell is quoted | Unquoted it is a blank line, and this project's own reader deletes it (`src/csv_read.cpp` pins `KEEP_NON_EMPTY`; `csv_reader.cpp:107` discards a zero-field row). Probed: write 500 rows, read back 487, no error. Python's `csv` does the same thing | — Pending |
| **v1.1** — An unclosed writer is flushed at `run()`'s return, with no warning | `sol::state` is a member of `LuaRunner::Impl`, so an unclosed writer is unreachable-but-uncollected when `run()` returns and its buffer is never flushed — the file is zero bytes. One `collect_garbage()` at `run()`'s end fixes that. The *warning* was dropped deliberately: emitting it needs a `weak_ptr` registry plus a new `Database::log_warning`, since `LuaRunner` has no logger at all | — Pending |
| **v1.1** — No destructive-overwrite guard | Declined. A script can truncate the live `.db`, a `.qvr`, or a migration `.sql` inside the sandbox. Verified silent on Windows as well as POSIX — SQLite opens `FILE_SHARE_READ\|FILE_SHARE_WRITE`, MSVC's `ofstream` uses `_SH_DENYNO`, so an 8192-byte database truncates to 13 bytes and later queries fail `file is not a database`. Accepted because `db:open_file('w')` already has the same hole and the project assumes callers obey contracts | — Pending |
| **v1.1** — Lua only, exactly like `db:read_csv` | Every other host has a native CSV library; Lua needs this specifically because `io` is deliberately absent from its sandbox | — Pending |
| **v1.1** — Defer a TOML reader/writer to a later milestone | Same justification as CSV, and toml++ is already vendored — but one format per milestone | — Pending |
| Reject libcsv on licence | LGPL 2.1 against a repo that distributes prebuilt binaries to four registries | ✓ Good |
| Reject `p-ranav/csv2` and `ben-strasser/fast-cpp-csv-parser` | csv2 splits a record on a newline inside a quoted field; fast-cpp-csv-parser takes the column count as a template parameter and Quiver's column count is only known at runtime | ✓ Good |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd-transition`):
1. Requirements invalidated? → Move to Out of Scope with reason
2. Requirements validated? → Move to Validated with phase reference
3. New requirements emerged? → Add to Active
4. Decisions to log? → Add to Key Decisions
5. "What This Is" still accurate? → Update if drifted

**After each milestone** (via `/gsd-complete-milestone`):
1. Full review of all sections
2. Core Value check — still the right priority?
3. Audit Out of Scope — reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-09-16 — Milestone v1.1 started (CSV writing for the Lua runner); v1.0 shipped (CSV reading); software version 0.10.6, changes unreleased under 0.10.7*
