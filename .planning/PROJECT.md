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

### Active

- [ ] Parsing is correct on genuinely dirty input — UTF-8 BOM, CRLF, a separator inside a quoted field, a newline inside a quoted field, doubled-quote escapes, ragged rows, junk rows above the header, duplicate and empty header names, configurable separator
- [ ] The agent-facing Lua reference (`bindings/js/src/lua-api.ts`) tells the model to read data files rather than transcribe them into the script
- [ ] Small-fixture tests covering every dirty case above, including a regression over the two real Maranhão CSVs

### Out of Scope

- **Writing CSV from Lua (`db:write_csv`)** — no concrete case yet, and a script's return value is already JSON-encoded back to the host, so there is an existing channel for structured output. Revisit the first time a script genuinely needs to emit a file.
- **Getting parsed data into the database** — that is the script's job. The reader hands over rows; the existing group writers take them.
- **The GB-scale write path** — `Database::execute` prepares and finalizes a statement on every call, and the group-insert loop rebuilds identical SQL per row, so even the bulk writers do one prepare per row and need every row in memory. Real, pre-existing, affects every caller, and deserves its own milestone rather than riding along with the reader.
- **Migrating `import_csv`/`export_csv` off rapidcsv** — staged separately. `import_csv` is a destructive path (`DELETE FROM` before insert) with zero import-side quoting tests today, and `export_csv` gains nothing from streaming because `Result` is already a fully materialized `std::vector<Row>` before the CSV layer is reached.
- **Replacing the binary subsystem's `CSVConverter` parser** — it reads a fixed-shape numeric format it generates itself, with its own NaN-sentinel routing. No user-facing quoting bug class there.
- **A multi-GB end-to-end test fixture** — expensive to build and it proves less than the dirty-input cases, which is where correctness actually lives.

## Context

**What prompted this.** `claw` drives Quiver's Lua runner to build and edit energy-modeling study
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
*Last updated: 2026-09-16 after Phase 1 — Lua reads CSV off disk, both entry points over one parser, sandboxed*
