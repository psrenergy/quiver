# Requirements: Quiver — CSV reading for the Lua runner

**Defined:** 2026-09-14
**Core Value:** Every layer — C++, C, Julia, Dart, Python, JS and Lua — sees the same data under the same rules, because all the logic lives in the C++ core and the bindings stay thin.

"User" throughout means the author of a Lua script run by `LuaRunner` — in practice the `claw`
agent, and secondarily anyone scripting a Quiver database by hand.

## v1 Requirements

### Parser

- [ ] **PARSE-01**: The core can parse a CSV incrementally, yielding rows without holding the whole file in memory, so a multi-GB file does not have to fit in RAM
- [ ] **PARSE-02**: A field quoted with `"` may contain the separator, and is returned as one field (`"May 1, 2014",33` is two fields, not three)
- [ ] **PARSE-03**: A quoted field may contain a newline, and the record is not split at it
- [ ] **PARSE-04**: A doubled quote inside a quoted field is unescaped to a single quote
- [ ] **PARSE-05**: A leading UTF-8 BOM is stripped and never appears in the first header name or cell
- [ ] **PARSE-06**: CRLF and LF line endings both parse, and no cell retains a trailing `\r`
- [ ] **PARSE-07**: Rows with differing field counts parse without throwing or silently truncating
- [ ] **PARSE-08**: The field separator is configurable at runtime, defaulting to `,`
- [ ] **PARSE-09**: Parsing never uses more memory than a bounded window regardless of file size, and that window is not tied to the host's CPU count

### Lua surface

- [ ] **LUA-01**: A script can read a whole CSV file into memory with `db:read_csv(path)`, addressing columns positionally and reading the header names separately
- [ ] **LUA-02**: A script can stream a CSV row by row with `db:read_csv_stream(path, on_row)`, holding bounded memory regardless of file size
- [ ] **LUA-03**: Both forms run on the same parser, so they cannot diverge in how they handle any input
- [ ] **LUA-04**: Both forms resolve `path` against the database file's directory and refuse anything outside it, and both refuse to run on an in-memory database — identical to every other file-touching Lua operation
- [ ] **LUA-05**: A script can name which row is the header, or declare that there is none, so files with junk rows above the header are readable
- [ ] **LUA-06**: A file whose header names repeat or are blank is fully readable — no column is unreachable and none silently shadows another
- [ ] **LUA-07**: Every cell reaches Lua as a string, with no numeric or date inference, so nothing is silently coerced
- [ ] **LUA-08**: A missing file, an unreadable path, a bad option value, or a header row past the end of the file each raise a `Cannot read_csv: ...` error naming the problem, rather than surfacing a parser or stream error

### Agent guidance

- [ ] **DOC-01**: `bindings/js/src/lua-api.ts` documents both entry points, their options, and the string-cell rule, in the house literal-token format the sync test checks
- [ ] **DOC-02**: The reference tells the model to read data files rather than transcribe them into the script, placed where the model is currently told it has no filesystem access
- [ ] **DOC-03**: The reference carries a worked example over a realistically dirty file, including the `tonumber`/`gsub` parenthesis trap that silently returns `nil`
- [ ] **DOC-04**: The `CLAUDE.md` nearest each change is updated, and a changelog entry is added

### Verification

- [ ] **TEST-01**: Automated tests cover every parser requirement above, using small hand-written fixtures
- [ ] **TEST-02**: A regression test reads the two real Maranhão CSVs and asserts the values match what the transcribed script produced
- [ ] **TEST-03**: The sandbox negatives are covered — escaping path, in-memory database, missing file, directory-as-path, subdirectory allowed
- [ ] **TEST-04**: Option validation is covered, each case asserting the call *throws* rather than silently falling back to a default
- [ ] **TEST-05**: Tests pass in a Release build as well as Debug, since `SOL_SAFE_GETTER` is off in Release and that has hidden Lua marshalling bugs before

## v2 Requirements

Deferred. Tracked but not in this roadmap.

### CSV writing

- **WRITE-01**: A script can write a CSV file into the database directory

### Core unification

- **UNIFY-01**: `import_csv` reads through the same parser, deleting the global `;`→`,` replace and the quote-unaware trailing-comma stripper that corrupt quoted fields today
- **UNIFY-02**: `CSVOptions` carries a `separator`, replacing the pre-pass hack
- **UNIFY-03**: Import-side quoting tests exist, written against current behaviour *before* any parser migration

### Write throughput

- **PERF-01**: Group inserts prepare one statement and reuse it, instead of rebuilding and re-preparing identical SQL per row
- **PERF-02**: A bounded-memory append path exists, so a large dataset can be written without holding every row

## Out of Scope

| Feature | Reason |
|---------|--------|
| Writing CSV from Lua | No concrete case yet; a script's return value is already JSON-encoded back to the host. Deferred to v2 rather than excluded |
| Getting parsed rows into the database | That is the script's job — the reader yields rows, the existing group writers consume them |
| Fixing the per-row prepare/finalize in the write path | Real and pre-existing, affects every caller, but it is not this feature's problem. Deferred to v2 |
| Migrating `import_csv`/`export_csv` off rapidcsv | `import_csv` is destructive and has zero import-side quoting tests to migrate against; `export_csv` cannot benefit while `Result` is already fully materialized. Deferred to v2 |
| Replacing the binary subsystem's `CSVConverter` parser | It reads a fixed-shape numeric format it generates itself, with NaN-sentinel routing. No user-facing quoting bug class |
| A multi-GB end-to-end test fixture | Expensive to build and proves less than the dirty-input cases |
| Numeric or date inference on cells | A silent coercion is worse than an explicit `tonumber`; `"0012"` must not become `12` |
| Exposing CSV reading in Julia/Dart/Python/JS | Those hosts all have native CSV libraries. Lua needs this precisely because `io` is deliberately absent |

## Traceability

Which phases cover which requirements. Filled during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| PARSE-01 … PARSE-09 | TBD | Pending |
| LUA-01 … LUA-08 | TBD | Pending |
| DOC-01 … DOC-04 | TBD | Pending |
| TEST-01 … TEST-05 | TBD | Pending |

**Coverage:**
- v1 requirements: 26 total
- Mapped to phases: 0
- Unmapped: 26 ⚠️ (roadmap not yet created)

---
*Requirements defined: 2026-09-14*
*Last updated: 2026-09-14 after initialization*
