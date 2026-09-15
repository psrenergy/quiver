# Phase 1: A Lua script reads a CSV file - Research

**Researched:** 2026-09-15
**Domain:** C++ CSV parsing (vincentlaucsb/csv-parser) wired into a sol2-based Lua sandbox
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

`vincentlaucsb/csv-parser` wired into the C++ core, with two Lua entry points on top of it —
`db:read_csv(path, opts)` (whole file) and `db:read_csv_stream(path, on_row, opts)` (row-by-row,
bounded memory) — every cell arriving as a string, every path resolved against the database
directory, plus the `lua-api.ts` entries that keep the build green.

Not this phase: dirty-input handling (BOM, CRLF, quoted separators/newlines, junk header rows,
duplicate/blank header names) is Phase 2. The shipped agent-reference rewrite is Phase 3.

All 22 lettered decisions (D-01 through D-22) from CONTEXT.md are locked and verbatim-binding on
the planner — see the full decision text in
`.planning/phases/01-a-lua-script-reads-a-csv-file/01-CONTEXT.md`. Summary of the areas locked:

- **D-01 to D-04** — `db:read_csv` returns one table `{header = {...}, rows = {{...}, ...}}`;
  `header` is absent (nil), never `{}`, when the file has none; rows are never padded to header
  width; an empty file throws `Cannot read_csv: file 'x.csv' is empty`; every cell is a string.
- **D-05 to D-08** — `db:read_csv_stream`'s callback is `on_row(row, index, header)` (`db` not
  passed first); `index` is 1-based over data rows only; returning `false` stops the read (any
  other return, including nothing/nil, continues) via `sol::optional<bool>` compared strictly to
  `false` (never `get<bool>`); the row count returned is `int64_t` rows *read*, not rows *kept*; a
  callback error propagates verbatim via `sol::protected_function` + the `db:transaction` idiom,
  never `sol::function`.
- **D-09 to D-11** — the reader is internal (`src/csv_read.h`/`.cpp`, no public header, no C API,
  not bound in Julia/Dart/Python/JS); csv-parser headers are included only in `csv_read.cpp`, never
  in `lua_runner.cpp`; CMake FetchContent forces `CSV_ENABLE_THREADS=OFF`, `CSV_NO_SIMD=ON`,
  `CSV_BUILD_PROGRAMS=OFF`, `CSV_BUILD_TESTS=OFF`, links `csv` PRIVATE, and excludes the
  `csv_no_simd` duplicate target from `all`.
- **D-12 to D-13** — the shared `CSVFormat` is built in exactly one place, pinning
  `delimiter(<sep>)`, `variable_columns(KEEP_NON_EMPTY)` (library default `IGNORE_ROW` silently
  discards ragged rows), and `header_row(0)` (library otherwise guesses and can eat a preamble
  row); never call `guess_csv()`; a blank line is not a row under `KEEP_NON_EMPTY`.
- **D-14 to D-18** — one trailing optional options table on both entry points, one shared decoder;
  exactly one key in Phase 1 (`separator`, single-character string, default `,`); the options
  parameter is `sol::object` (not `sol::optional<sol::table>`) with hand type-checking (the house
  precedent at `relation_target_from_lua`); an unknown option key throws; validate strictly by hand
  (not `lua_cell_as`), collecting entries before validating; the memory window is a fixed internal
  constant with no caller-visible knob (no `format.chunk_size(...)` call at all).
- **D-19 to D-22** — each entry point names itself in its own errors (`Cannot read_csv_stream: ...`
  vs `Cannot read_csv: ...`); the operation name threads through the shared helper and no
  csv-parser error ever reaches Lua unwrapped; error messages quote the script's own spelling of
  the path, not the resolved absolute path; the Phase-1 error catalogue (in evaluation order) is:
  in-memory db → path escape → options must be a table → unknown option key → separator must be a
  string → separator must be a single character → file not found → path is a directory → file is
  empty → csv-parser wrapper failure.

### Claude's Discretion

- Exact internal signature of the shared helper (free function vs `LuaRunner::Impl` static,
  template vs `std::function`), provided both entry points route through **one** loop and **one**
  `CSVFormat` construction (LUA-03).
- Whether the early-stop `bool` rides the internal callback's return type or a separate predicate.
- Test file naming and split, within the constraints in CONTEXT.md's Integration Points section.

### Deferred Ideas (OUT OF SCOPE)

- **`SOL_SAFE_FUNCTION=1` at `src/CMakeLists.txt:60-61` is a dead define** — sol2 3.5.0 reads
  `SOL_SAFE_FUNCTION_CALLS` / `SOL_SAFE_FUNCTION_OBJECTS`. Every existing `sol::optional<sol::table>`
  options parameter in the Lua surface therefore silently accepts a non-table in both Debug and
  Release: verified live on `db:export_csv` and `db:query_string`. Phase 1 works around it locally
  (D-15) rather than fixing it globally, because the fix changes behaviour on five existing sites
  and belongs in its own change with its own tests. **Raise as a separate issue.**
- **Blank-line and header-guess audit of `import_csv`** — the same class of parser-default trap
  (D-12) may have rapidcsv analogues on the existing import path. Out of scope here by explicit
  decision (UNIFY-01/02/03 are v2).
- **CSV writing from Lua** (`db:write_csv`) — v2, WRITE-01.
- **Direct gtest coverage of the reader** without the Lua boundary — only if Phase 2's
  PARSE-02..07 matrix proves awkward through Lua; the two-line escape hatch is recorded in
  CONTEXT.md's Integration Points.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-------------------|
| PARSE-01 | Core can parse a CSV incrementally, yielding rows without holding the whole file in memory | csv-parser's chunked/mmap reader with `CSV_ENABLE_THREADS=OFF` fixes the window at `CSV_CHUNK_SIZE_DEFAULT` (10 MB), unmultiplied by worker count — see Standard Stack, Pitfall 2, Security Domain |
| PARSE-08 | Field separator is configurable at runtime, defaulting to `,` | Options table `separator` key (D-14/D-15/D-17), decoded via the strict `sol::object` pattern copied from `relation_target_from_lua` — see Architecture Patterns / Pattern 1 |
| PARSE-09 | Parsing never uses more memory than a bounded window, and that window is not tied to host CPU count | `CSV_ENABLE_THREADS=OFF` makes `read_window_size` return `chunk_size` unmultiplied by worker count (D-18); no caller-visible `chunk_size` knob — see Standard Stack, Common Pitfalls 2 |
| LUA-01 | `db:read_csv(path)` reads a whole file, addressing columns positionally with header available separately | `{header, rows}` return shape (D-01), backed by the existing `to_lua_table` marshaler and `LuaRunner::run`'s single-return-value JSON encoder — see Architecture Patterns diagram, Anti-Patterns |
| LUA-02 | `db:read_csv_stream(path, on_row)` streams row by row with bounded memory | `on_row(row, index, header)` callback contract (D-05), `sol::protected_function` + early-stop via `sol::optional<bool>` (D-06/D-08) — see Architecture Patterns Pattern 2, Common Pitfalls 5 |
| LUA-03 | Both forms run on the same parser, cannot diverge in input handling | One shared `CSVFormat`-construction + read-loop helper (D-01 rationale, D-12) — see Architecture Patterns Pattern 1, Recommended Project Structure |
| LUA-04 | Both forms resolve `path` against the db directory, refuse escapes, refuse in-memory dbs | Reuse `resolve_sandboxed_path` verbatim (existing helper) — see Don't Hand-Roll, Code Examples |
| LUA-07 | Every cell reaches Lua as a string, no numeric/date inference | Copy each cell via `field.get<std::string_view>()` into `std::string` inside the loop, never retain `CSVRow` (D-04) — see Common Pitfalls 4 |
| LUA-08 | Bad path/option/missing file/etc. raise a `Cannot read_csv: ...` error naming the problem, never a raw parser/stream error | D-19 to D-22 error catalogue, wrap `CSVReader` construction in try/catch — see User Constraints, Security Domain |
| TEST-03 | Sandbox negatives covered: escaping path, in-memory db, missing file, directory-as-path, subdirectory allowed | `LuaSandboxTest` fixture + `expect_lua_error` helper (existing, verified this session) — see Code Examples |
| DOC-01 | `bindings/js/src/lua-api.ts` documents both entry points, options, and string-cell rule in the literal-token format the sync test checks | `lua-api-sync.test.ts` regex mechanics verified this session; both names must appear as literal `db:read_csv`/`db:read_csv_stream` tokens — see Code Examples, Architecture Patterns |
</phase_requirements>

## Summary

This phase is unusual: `/gsd-discuss-phase` already ran an adversarial research process (27
subagents, two workflows) before this research pass, and produced a CONTEXT.md whose 22 lettered
decisions (D-01..D-22) are already pinned with file:line citations against the real tree. This
research agent's job was therefore verification, not discovery: every citation that could be
checked against the current checkout was opened and read this session, and every claim below is
tagged accordingly. Nothing in CONTEXT.md's decisions was found to be stale or wrong against the
current source; two things drifted cosmetically (the `run()` JSON-encode call is now at
`src/lua_runner.cpp:1809`, not `:1806`; `append_json_table` is confirmed at exactly `:145`) and one
external fact (csv-parser's CMake option defaults) was independently confirmed against the
library's own upstream `CMakeLists.txt`.

**Primary recommendation:** Follow CONTEXT.md's decisions verbatim — they are pre-verified, not
exploratory options. Add `src/csv_read.h` / `src/csv_read.cpp` as a new internal (no public header)
translation unit wrapping `vincentlaucsb/csv-parser` 5.3.0, expose `db:read_csv` /
`db:read_csv_stream` from `src/lua_runner.cpp`'s existing sandboxed file-op block, and update
`bindings/js/src/lua-api.ts` in the same commit (the sync test is a hard, unconditional build gate).

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| CSV parsing (RFC-4180-ish state machine, chunked read) | C++ core (`src/csv_read.cpp`, internal) | — | New logic lives in the core per house rule; no public header since no FFI consumer wants it (REQUIREMENTS.md explicitly scopes Julia/Dart/Python/JS out) |
| Path sandboxing / containment | C++ core, LuaRunner layer (`resolve_sandboxed_path`) | — | Existing helper; every file-touching `db:` op already routes through it — reused, not reimplemented |
| Lua entry points (`db:read_csv`, `db:read_csv_stream`) | LuaRunner binding (`src/lua_runner.cpp`) | — | sol2 `bind.set_function` registrations in the existing sandboxed-op block; thin wrapper over `csv_read.*` |
| Row/table marshaling to Lua | LuaRunner binding | — | Reuse `to_lua_table` — the house's only vector→table marshaler |
| JSON encoding of script return value | LuaRunner (`append_json*`, `LuaRunner::run`) | — | Existing encoder; no changes needed since `{header=..., rows=...}` is a plain table |
| Build wiring (FetchContent, target options) | CMake (`cmake/Dependencies.cmake`, `src/CMakeLists.txt`) | — | New dependency addition; must stay PRIVATE-linked, no public leakage |
| Agent-facing documentation | `bindings/js/src/lua-api.ts` | — | Build-gated by `lua-api-sync.test.ts`; must land in the same commit as the binding |

## Package Legitimacy Audit

This phase adds exactly one new external dependency: `vincentlaucsb/csv-parser`. It is pulled via
CMake `FetchContent` from GitHub (a git tag), not from a language package registry (npm/PyPI/crates),
so the `gsd_run query package-legitimacy check` seam (which is registry-shaped) does not apply
directly. Verification was done manually against the authoritative sources instead.

| Package | Registry | Age | Popularity | Source Repo | Verdict | Disposition |
|---------|----------|-----|------------|--------------|---------|-------------|
| `vincentlaucsb/csv-parser` @ `5.3.0` | GitHub (git tag via FetchContent) | Multi-year project (pre-dates 2020; 5.x line active through 2026) [CITED: github.com/vincentlaucsb/csv-parser] | 1.1k stars, 205 forks, 23 watchers [CITED: github.com/vincentlaucsb/csv-parser] | github.com/vincentlaucsb/csv-parser | OK | Approved |

**Packages removed due to [SLOP] verdict:** none.
**Packages flagged as suspicious [SUS]:** none.

License is MIT [CITED: github.com/vincentlaucsb/csv-parser] — compatible with PROJECT.md's
constraint that a copyleft dependency is disqualifying (this is what eliminated `libcsv`, per
PROJECT.md Key Decisions). This was PROJECT.md's own prior evaluation (`Reject libcsv on licence`),
re-confirmed here.

Both other candidates PROJECT.md's Key Decisions table records as rejected (`p-ranav/csv2` for
splitting a record on a newline inside a quoted field, `ben-strasser/fast-cpp-csv-parser` for a
compile-time column count) were **not** re-investigated in this pass — PROJECT.md already recorded
the rejection rationale as a settled decision, and CONTEXT.md's scope explicitly locks the choice
of csv-parser (D-11, "reversible: one-way" is not stated but the whole decision tree assumes it).

## Standard Stack

### Core

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|---------------|
| `vincentlaucsb/csv-parser` | 5.3.0 | RFC-4180-ish incremental CSV reader with a real cross-chunk quote-carry state machine | Only candidate in PROJECT.md's evaluation that is MIT-licensed, header-available-but-avoided-here, streams in bounded memory, and has a genuine quoted-newline/quoted-separator implementation [CITED: github.com/vincentlaucsb/csv-parser] |

**Version verification:** `5.3.0` was located in the project's own `Vcpkg` port listing as of
2026-05-30 [CITED: web search, vcpkg.link] — current relative to a 2026-09-15 research date, no
newer tag surfaced. CONTEXT.md's own citations (`csv_format.hpp:279`, `csv_reader.cpp:104-122`,
`common.hpp:410/:424`, `orchestrator.hpp:78-90`, `csv_reader.hpp:213-215`) were not independently
re-opened against a local checkout of csv-parser in this session (the library is not yet vendored
into this repo — it lands with this phase), so those internal-file claims remain at the confidence
level CONTEXT.md already assigned them (verified by the prior refutation-lens passes, which the
planner should treat as already-adjudicated, not open questions). What **was** independently
re-verified this session, against csv-parser's own upstream `CMakeLists.txt` fetched directly from
GitHub: all four CMake option names in D-11 exist with exactly the defaults CONTEXT.md states —
`CSV_ENABLE_THREADS` (default `ON`), `CSV_NO_SIMD` (default `OFF`), `CSV_BUILD_PROGRAMS` (default
`ON`), `CSV_BUILD_TESTS` (default `ON`) — and the AVX2 compiler flag (`/arch:AVX2` on MSVC,
`-mavx2` on GCC/Clang) is indeed tied to the SIMD path and disabled when `CSV_NO_SIMD=ON`
[VERIFIED: raw.githubusercontent.com/vincentlaucsb/csv-parser/master/CMakeLists.txt].

**Installation (CMake, not a package manager):**
```cmake
# cmake/Dependencies.cmake, after the rapidcsv block
FetchContent_Declare(csv_parser
    GIT_REPOSITORY https://github.com/vincentlaucsb/csv-parser.git
    GIT_TAG 5.3.0
)
set(CSV_ENABLE_THREADS OFF CACHE BOOL "" FORCE)
set(CSV_NO_SIMD ON CACHE BOOL "" FORCE)
set(CSV_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(CSV_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(csv_parser)
set_target_properties(csv_no_simd PROPERTIES EXCLUDE_FROM_ALL YES)
```
This mirrors D-11 exactly; the four `FORCE`d cache variables must precede
`FetchContent_MakeAvailable`, matching the existing `LUA_TESTS`/`LUA_LINE_EDITOR` pattern already
in the file for the `lua` dependency [VERIFIED: cmake/Dependencies.cmake:37,40 — read this
session]. Do **not** copy the adjacent `EXCLUDE_FROM_ALL`-is-unsafe comment by analogy: that
applies to `lua-cmake`, which declares `install()` rules; csv-parser's target list (`csv`,
`fastpycsv`, `generate_single_header`, `doxygen`) shows no install/export step for `csv_no_simd`
[CITED: github.com/vincentlaucsb/csv-parser/blob/master/CMakeLists.txt].

### Supporting

No new supporting libraries. `rapidcsv` (already a dependency, v8.92) is untouched by this phase —
it stays behind `import_csv`/`export_csv` (v2 concern, out of scope, per REQUIREMENTS.md
`UNIFY-01..03` and PROJECT.md's Out-of-Scope list).

### Alternatives Considered

| Instead of | Could use | Tradeoff |
|------------|-----------|----------|
| `vincentlaucsb/csv-parser` | `p-ranav/csv2` | Splits a record on a newline inside a quoted field — fails PARSE-03's forward requirement even though PARSE-03 itself is Phase 2 [per PROJECT.md Key Decisions, re-affirmed not re-litigated] |
| `vincentlaucsb/csv-parser` | `ben-strasser/fast-cpp-csv-parser` | Column count is a compile-time template parameter; Quiver's column count is only known at runtime [per PROJECT.md Key Decisions] |
| `vincentlaucsb/csv-parser` | `libcsv` | LGPL 2.1 — disqualifying given Quiver ships prebuilt binaries to four registries [per PROJECT.md Key Decisions] |

## Architecture Patterns

### System Architecture Diagram

```
Lua script (untrusted input to the host)
    │
    │  db:read_csv(path, opts)          db:read_csv_stream(path, on_row, opts)
    ▼                                        ▼
sol2 binding layer (src/lua_runner.cpp, sandboxed file-op block)
    │  1. resolve_sandboxed_path(db, "read_csv"|"read_csv_stream", path)
    │     — rejects :memory: db, escapes, missing/dir path (existing helper, reused verbatim)
    │  2. shared options decoder (new, modeled on relation_target_from_lua)
    │     — sol::object + hand get_type() check; unknown key throws; separator must be 1-char string
    ▼
shared internal reader (src/csv_read.cpp — new, no public header)
    │  3. build ONE csv::CSVFormat: delimiter(sep), variable_columns(KEEP_NON_EMPTY),
    │     header_row(0) — never guess_csv(), never a chunk_size call
    │  4. construct csv::CSVReader inside try/catch — wrap any csv-parser exception into
    │     Pattern 1 "Cannot <op>: cannot read file '<p>': <reason>"
    │  5. empty-file check (throws before the caller sees "no header" ambiguity)
    ▼                                        ▼
db:read_csv path:                       db:read_csv_stream path:
  loop all rows into a                    loop rows one at a time, copying each cell to
  vector<vector<string>>,                 std::string via field.get<string_view>() before
  copy each cell as a string              the CSVRow dies (no retained 10MB mmap pin);
  (never retain CSVRow)                   invoke sol::protected_function per row with
    │                                     (row, index, header); check sol::optional<bool>
    ▼                                     for early stop; propagate a callback error verbatim
to_lua_table (existing marshaler)          │
    │                                       ▼
{ header = {...}, rows = {{...},...} }    returns int64_t count of rows fed to callback
    │                                       │
    ▼                                       ▼
LuaRunner::run() JSON-encodes result.get<sol::object>(0) — only the first return value,
identical code path for both call shapes (append_json / append_json_table, unchanged)
```

### Recommended Project Structure

```
src/
├── csv_read.h          # NEW — internal-only header (no include/quiver/ counterpart)
├── csv_read.cpp         # NEW — CSVFormat construction, shared read loop, error wrapping
├── lua_runner.cpp        # bind db:read_csv / db:read_csv_stream in the existing sandboxed block
cmake/
└── Dependencies.cmake    # add csv_parser FetchContent block, forced OFF/ON options
bindings/js/src/
└── lua-api.ts            # add both db: entries — build-gated, must land in the same commit
tests/
├── test_lua_runner_csv_import.cpp   # existing file — do NOT add read_csv tests here (import-only)
└── test_lua_runner_read_csv.cpp     # NEW (naming at planner's discretion) — TEST-03 negatives +
                                       # D-01/D-05/D-06/D-12/D-13 pin tests
```

### Pattern 1: One shared parser under two entry points (LUA-03)

**What:** A single free function or `LuaRunner::Impl` static builds the `csv::CSVFormat` exactly
once and drives one read loop; `db:read_csv` collects into a table, `db:read_csv_stream` invokes a
callback per row. Signature shape (free function vs static, template vs `std::function`) is
explicitly left to Claude's discretion in CONTEXT.md.

**When to use:** Any time two Lua entry points must never diverge in parsing behavior — the
project's structural requirement for this phase (LUA-03), not a stylistic preference.

**Example (house precedent for the strict-decode option pattern to copy):**
```cpp
// Source: src/lua_runner.cpp:1006-1016 (read this session) — the correct pattern for D-15/D-17.
// nil/missing clears; sol::object (not sol::optional) so a wrong type still throws.
static std::optional<std::string> relation_target_from_lua(const sol::object& target_label,
                                                           const std::string& caller) {
    if (!target_label.valid() || target_label.get_type() == sol::type::lua_nil) {
        return std::nullopt;
    }
    if (target_label.get_type() != sol::type::string) {
        throw std::runtime_error("Cannot " + caller + ": target_label has unsupported Lua type");
    }
    return target_label.as<std::string>();
}
```

### Pattern 2: `db:transaction`'s protected_function idiom (D-08 — reuse for stream callback errors)

**What:** Take `sol::protected_function`, never `sol::function`; check `result.valid()`; on failure
extract `sol::error` and rethrow as `std::runtime_error`.

**When to use:** Any Lua callback invoked from C++ where a Lua-side error must propagate cleanly
and RAII destructors must still run during unwind.

**Example:**
```cpp
// Source: src/lua_runner.cpp:290-306 (read this session)
"transaction",
[](Database& self, sol::protected_function fn) -> sol::object {
    self.begin_transaction();
    auto result = fn(std::ref(self));
    if (!result.valid()) {
        sol::error err = result;
        try {
            self.rollback();
        } catch (...) {
        }
        throw std::runtime_error(err.what());
    }
    self.commit();
    if (result.return_count() > 0) {
        return result.get<sol::object>(0);
    }
    return sol::make_object(result.lua_state(), sol::lua_nil);
},
```

### Pattern 3: Sandboxed file-op registration site

**What:** New `db:` file-touching bindings are added as plain `bind.set_function(...)` lambdas in
the existing sandboxed block, immediately after `csv_to_bin`.

**Example (exact insertion point, read this session):**
```cpp
// Source: src/lua_runner.cpp:441-446
bind.set_function("bin_to_csv", [](Database& self, const std::string& path, sol::optional<bool> aggregate) {
    CSVConverter::bin_to_csv(resolve_sandboxed_path(self, "bin_to_csv", path), aggregate.value_or(true));
});
bind.set_function("csv_to_bin", [](Database& self, const std::string& path) {
    CSVConverter::csv_to_bin(resolve_sandboxed_path(self, "csv_to_bin", path));
});
// db:read_csv / db:read_csv_stream land here
```
This region carries **no** `NOLINT` pairs [VERIFIED: src/lua_runner.cpp:280-360 read this
session — no NOLINTBEGIN/NOLINTEND tokens appear between the transactions group and
`create_element`]; do not invent one for the new bindings.

### Anti-Patterns to Avoid

- **Multi-return from `db:read_csv`:** `LuaRunner::run` only encodes
  `result.get<sol::object>(0)` [VERIFIED: src/lua_runner.cpp:1809 — `append_json(result.get<sol::object>(0), out, 0);`, read this session], so a second Lua return value is silently dropped. Return one table.
- **Letting a csv-parser exception reach Lua unwrapped:** violates LUA-08; wrap `CSVReader`
  construction and iteration in try/catch, translate to Pattern 1.
- **Calling `CSVFormat::guess_csv()` or leaving `header_row` unset:** both invoke header/format
  guessing that silently eats rows the caller did not ask to skip (D-12).
- **A second `to_lua_table`-shaped helper:** `src/CLAUDE.md` names `to_lua_table<T>` "the only
  vector→table marshaler" [VERIFIED: src/CLAUDE.md line "`to_lua_table<T>` overloads (flat +
  nested) are the only vector→table marshalers." — read this session]. Reuse it; do not write
  `to_lua_row`.
- **`get<bool>` on the stream callback's return value:** `lua_toboolean` truthiness reads a
  no-return `nil` as falsy in some sol2 code paths; D-06 mandates `sol::optional<bool>` compared
  strictly to `false`.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Path sandboxing (in-memory rejection, containment) | A new path-resolution function | `resolve_sandboxed_path` (`src/lua_runner.cpp:772`, read this session) | Already handles the exact five cases TEST-03 needs (in-memory, escape, missing-parent, subdirectory-OK) except existence — that check must still be added separately since `weakly_canonical` does not require the path to exist |
| Vector→Lua table marshaling | A new `to_lua_row`-style function | `to_lua_table<T>` (`src/lua_runner.cpp:831-861`, read this session) | House-documented as the only marshaler; both the flat and nested overloads already fit `header` (flat strings) and `rows` (nested vector of strings) |
| Strict Lua-object type decoding for an optional parameter | A hand `if (obj.is<sol::table>())` ad hoc check | The `relation_target_from_lua` pattern: `sol::object` parameter, explicit `get_type()` checks | `sol::optional<sol::table>` silently accepts a non-table argument in Release because `SOL_SAFE_FUNCTION` is a dead define (deferred issue, see below) — a lens **empirically verified** `db:export_csv(..., "NOT_A_TABLE")` succeeding silently against the real Debug build |
| CSV parsing state machine (quotes, cross-chunk carry, chunked reads) | A hand-rolled `split(',')` reader | `csv::CSVReader` from `vincentlaucsb/csv-parser` | RFC-4180 quoting is exactly the class of bug (`import_csv`'s existing `;`→`,` global replace and quote-unaware trailing-comma stripper) this milestone exists to avoid repeating |

**Key insight:** This phase's "don't hand-roll" list is almost entirely about reusing helpers that
already exist in `lua_runner.cpp` rather than about avoiding a new dependency — the one new
dependency (csv-parser) is itself the alternative to hand-rolling a parser, and the codebase already
demonstrates (via `import_csv`) what hand-rolled CSV splitting costs.

## Runtime State Inventory

Not applicable — this is a greenfield feature addition (new bindings, new internal source file, new
CMake dependency), not a rename/refactor/migration. No existing stored data, service config, or
registered state needs updating.

## Common Pitfalls

### Pitfall 1: `SOL_SAFE_FUNCTION=1` is a dead define

**What goes wrong:** An options-table parameter typed `sol::optional<sol::table>` silently accepts
a non-table Lua value (string, number, boolean) in both Debug and Release, defaulting every option
rather than raising.

**Why it happens:** `src/CMakeLists.txt:60-61` sets `SOL_SAFE_FUNCTION=1`
[VERIFIED: src/CMakeLists.txt:59-62 — `target_compile_definitions(quiver PRIVATE\n    SOL_SAFE_NUMERICS=1\n    SOL_SAFE_FUNCTION=1\n)`, read this session], but sol2 3.5.0 reads
`SOL_SAFE_FUNCTION_CALLS`/`SOL_SAFE_FUNCTION_OBJECTS` — a different macro name entirely — so the
define is a no-op, and sol2's optional-checker forwards to an internal `&no_panic` path that never
raises (per CONTEXT.md's citation of `stack.hpp:186-197` and `stack_check_unqualified.hpp:361-374`,
not independently re-opened this session since sol2 is a pre-existing vendored dependency and the
claim was already lens-verified by running `quiver_cli` in the discuss-phase workflow).

**How to avoid:** Take the options table as `sol::object` (not `sol::optional<sol::table>`) and
hand-check `get_type()`, per D-15. This is the house precedent already used at
`relation_target_from_lua` (`src/lua_runner.cpp:1006`, read this session).

**Warning signs:** Any new optional-table Lua parameter that uses `sol::optional<sol::table>` is
suspect — five existing sites (`parse_csv_options` callers: `export_csv`/`import_csv`, plus others
CONTEXT.md names at `:338`, `:350`, `:1252`, `:1263`, `:1274`) already carry this defect and are
explicitly deferred, not fixed, in this phase.

### Pitfall 2: csv-parser's own defaults contradict this phase's requirements

**What goes wrong:** Without explicit configuration, csv-parser (a) discards any row whose field
count differs from the header (`IGNORE_ROW` default), violating the no-truncation/no-silent-drop
intent behind D-02; (b) guesses which row is the header via a heuristic, which silently eats a
title/preamble row above the real header on exactly the kind of dirty file this milestone targets;
(c) auto-enables a parallel scheduler above 50 MB with SIMD compiled in by default, which both
threatens PARSE-09 (window must not scale with CPU count) and — more seriously — adds a PUBLIC
`/arch:AVX2` compile flag that would propagate into `quiver` itself and SIGILL on pre-AVX2 hardware
for every shipped binary.

**Why it happens:** These are csv-parser's library defaults, tuned for throughput on clean files,
not for an untrusted/dirty-input Lua sandbox.

**How to avoid:** Pin all three parser-format calls in the one shared `CSVFormat`-construction site
(`format.variable_columns(KEEP_NON_EMPTY)`, `format.header_row(0)`, `format.delimiter(sep)`, never
`guess_csv()`), and force `CSV_ENABLE_THREADS=OFF` / `CSV_NO_SIMD=ON` at the CMake level (D-11,
D-12, D-13 — independently confirmed against csv-parser's own CMakeLists.txt this session).

**Warning signs:** A test file with a one-cell title row above a real header, or a ragged row with
fewer columns than the header, silently vanishing from the result is the signature of this pitfall
firing — CONTEXT.md's `preamble.csv` and `ragged.csv` fixtures exist specifically to catch it.

### Pitfall 3: `header_row=false`/empty-header ambiguity colliding with empty-file handling

**What goes wrong:** If an empty file were allowed to return `{header = nil, rows = {}}` instead of
throwing, `if csv.header then ...` would mean two different things ("caller declared no header" vs
"file was empty"), and a script cannot distinguish them.

**Why it happens:** Phase 2's `LUA-05` (declare no header) reuses the same `header = nil` shape;
without the empty-file throw the two features would collide on the same falsy sentinel.

**How to avoid:** D-03 — an empty file throws `Cannot read_csv: file 'x.csv' is empty` in the
**shared** helper (so both entry points agree, per LUA-03), not per-caller.

**Warning signs:** `db:read_csv_stream("empty.csv", fn)` returning `0` while `db:read_csv("empty.csv")`
throws is the divergence this guards against.

### Pitfall 4: A retained `CSVRow` pins the whole mmap'd chunk

**What goes wrong:** Storing a `csv::CSVRow` (rather than copying its cells to `std::string`) keeps
a `shared_ptr<void>` alive that pins the underlying memory-mapped chunk (potentially the whole
file), defeating the bounded-memory guarantee PARSE-01/PARSE-09 exist for.

**Why it happens:** `CSVRow`'s fields are views into the reader's internal buffer, not owned
strings.

**How to avoid:** Copy each field with `field.get<std::string_view>()` into a `std::string` inside
the per-row loop and let the `CSVRow` go out of scope every iteration (D-04).

**Warning signs:** Memory usage scaling with file size despite using the streaming entry point.

### Pitfall 5: `sol::function` instead of `sol::protected_function` for the stream callback

**What goes wrong:** A Lua error raised mid-callback either crashes the process (a `lua_error`
longjmp escaping C++ stack frames without unwinding) or skips RAII cleanup (`~CSVReader()` never
runs, leaving an open file handle that blocks deletion on Windows).

**Why it happens:** `sol::function` calls do not check validity and let a Lua panic propagate as a
raw longjmp; only `sol::protected_function` + `result.valid()` gives C++ a chance to convert the
failure into a normal exception that unwinds properly.

**How to avoid:** D-08 — use the exact `db:transaction` idiom (`sol::protected_function`,
`result.valid()` check, `sol::error err = result; throw std::runtime_error(err.what());`).

**Warning signs:** `std::filesystem::remove` on the CSV file failing after a callback error is the
regression signature CONTEXT.md's own test-design notes call out (Windows file locks survive a
non-unwound stack).

## Code Examples

### Reusable sandbox helper (verified, copy verbatim)

```cpp
// Source: src/lua_runner.cpp:771-805 (read this session)
static std::string
resolve_sandboxed_path(const Database& db, const std::string& operation, const std::string& path) {
    namespace fs = std::filesystem;

    const std::string& db_path = db.path();
    if (db_path == ":memory:") {
        throw std::runtime_error("Cannot " + operation +
                                 ": database is in-memory, file operations are unavailable");
    }

    auto root = fs::path(db_path).parent_path();
    if (root.empty()) {
        root = fs::current_path();
    }
    root = fs::weakly_canonical(root);

    auto candidate = fs::path(path);
    if (candidate.is_relative()) {
        candidate = root / candidate;
    }
    candidate = fs::weakly_canonical(candidate);

    const auto rel = candidate.lexically_relative(root);
    if (rel.empty() || rel == "." || rel.begin()->string() == "..") {
        throw std::runtime_error("Cannot " + operation + ": path '" + path + "' escapes the database directory '" +
                                 root.string() + "'");
    }

    return candidate.string();
}
```
Note: this does **not** check file existence (`weakly_canonical` tolerates a missing path), so
"file not found" and "path is a directory" (error catalogue entries 7-8 in D-22) must be checked
separately after this call returns.

### Test fixture conventions (verified)

```cpp
// Source: tests/test_lua_runner.h (whole file, read this session)
class LuaSandboxTest : public ::testing::Test {
protected:
    void SetUp() override {
        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        sandbox = std::filesystem::temp_directory_path() /
                  (std::string("quiver_lua_") + info->test_suite_name() + "_" + info->name());
        std::filesystem::remove_all(sandbox);
        std::filesystem::create_directories(sandbox);
    }
    void TearDown() override { std::filesystem::remove_all(sandbox); }
    std::string db_path() const { return (sandbox / "test.db").string(); }
    std::filesystem::path sandbox;
};

inline void expect_lua_error(quiver::LuaRunner& lua, const std::string& script, const std::string& substring) {
    try {
        lua.run(script);
        FAIL() << "expected script to throw: " << script;
    } catch (const std::exception& e) {
        EXPECT_NE(std::string(e.what()).find(substring), std::string::npos) << e.what();
    }
}
```
All five TEST-03 negatives (escaping path, in-memory db, missing file, directory-as-path,
subdirectory-allowed) should subclass `LuaSandboxTest` and use `expect_lua_error`, matching every
existing CSV-adjacent suite (`class LuaRunner_ImportCSV : public LuaSandboxTest` pattern is the
convention to follow, per CONTEXT.md canonical_refs — file naming itself is left to Claude's
discretion).

### DOC-01 build gate mechanics (verified)

```ts
// Source: bindings/js/test/lua-api-sync.test.ts (read this session)
// Pass 1 regex that must match the new bindings:
const setFns = [...CPP.matchAll(/\b(bind|ns)\.set_function\(\s*"([a-z_][a-z0-9_]*)"/g)];
// -> "read_csv" / "read_csv_stream" registered via bind.set_function(...) are auto-discovered.

// The doc-side check that then must pass:
const documented = (token: string) =>
  new RegExp(`${token}(?![a-z0-9_])`).test(LUA_DB_API_REFERENCE);
// every db: method must appear as literal "db:read_csv" / "db:read_csv_stream" in lua-api.ts
```
Both new names must appear as the exact literal tokens `db:read_csv` and `db:read_csv_stream`
somewhere in `LUA_DB_API_REFERENCE` — no signature/arity/shape checking is done by the test, so the
doc's prose (option table shape, callback arity, error catalogue) needs a manual re-diff, not just
the token match. The existing sandbox bullet at `bindings/js/src/lua-api.ts:97-99`
[VERIFIED: bindings/js/src/lua-api.ts:97-102, read this session — the exact text: "Every
file-touching operation (`db:export_csv`, `db:import_csv`, `db:open_file`, `db:bin_to_csv`,
`db:csv_to_bin`, `db:validate_migrations`, `expr:save`) resolves relative paths against the
directory containing the database file..."] must have both new names added to its parenthetical
list.

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| No CSV read path in Lua at all — agent transcribes file contents into scripts as literals | `db:read_csv` / `db:read_csv_stream` over `vincentlaucsb/csv-parser` | This phase | Eliminates the 360-line-transcription pattern documented in PROJECT.md's Context section |
| `import_csv`'s rapidcsv-based, whole-file-materializing parser with a hand-rolled `;`→`,` global replace | New, separate reader (`csv_read.cpp`) — deliberately not unified with `import_csv` this milestone | This phase (reader added); unification is v2 (`UNIFY-01..03`) | `import_csv`'s known data-corruption paths are untouched and explicitly out of scope here |

**Deprecated/outdated:** Nothing in the existing codebase is deprecated by this phase — it is a
pure addition. csv-parser's own defaults (parallel scheduling, SIMD, header guessing) are treated
as "wrong for us," not deprecated, and are explicitly overridden rather than relied upon.

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | csv-parser 5.3.0 internal file/line citations (`csv_format.hpp:279`, `csv_reader.cpp:104-122`, `common.hpp:410/:424`, `orchestrator.hpp:78-90`, `csv_reader.hpp:213-215`) are accurate | Standard Stack, Pitfall 2, D-08 pattern | These were lens-verified in the prior discuss-phase workflow (one lens ran `quiver_cli` against a real build) but not re-opened against a local vendored copy in this research session, since the library is not yet fetched into this tree. If a line number drifted between what the lens read and what actually ships, the planner should still pin the *documented behavior* (KEEP_NON_EMPTY vs IGNORE_ROW, header_row/guess behavior, threading-off window sizing) via a unit test rather than trust the line number alone — the CMake-level facts (option names/defaults, AVX2 flag) **were** independently re-confirmed this session against the library's own upstream CMakeLists.txt |
| A2 | csv-parser 5.3.0 is the current/latest tag as of the research date | Standard Stack | If a newer patch tag exists, `GIT_TAG 5.3.0` in the FetchContent block should be revisited before landing, though this is a locked decision (D-11) not up for relitigation without the user |

**If this table is empty:** N/A — two residual claims are logged above; both concern the internal
mechanics of a third-party dependency not yet vendored into this checkout, not this project's own
code (every claim about *this* codebase was verified by reading the actual file this session).

## Open Questions

1. **Exact free-function vs. `Impl`-static signature for the shared helper**
   - What we know: Both entry points must route through one `CSVFormat` construction and one read
     loop (LUA-03); CONTEXT.md explicitly defers the exact shape to Claude's discretion.
   - What's unclear: Whether a template-based or `std::function`-based early-stop signature reads
     better against the rest of `lua_runner.cpp`'s style.
   - Recommendation: The planner should pick one and note it in the PLAN.md; it is a
     reversible, purely internal choice with no test-visible consequence.

2. **Whether direct gtest coverage of the reader (bypassing the Lua boundary) is worth adding now**
   - What we know: `quiver_tests` cannot currently include `src/` headers (`tests/CMakeLists.txt`
     adds no such include dir) [VERIFIED: tests/CMakeLists.txt, whole file read this session — no
     `target_include_directories(quiver_tests ... src)` line exists], so today the only path to test
     `csv_read.cpp` is through the Lua surface, which is also the existing convention for LuaRunner
     behavior generally.
   - What's unclear: Whether Phase 2's PARSE-02..07 dirty-input matrix will prove awkward to express
     purely through Lua scripts.
   - Recommendation: CONTEXT.md defers this explicitly (Deferred Ideas) — do not add the two-line
     escape hatch (`target_sources` + `target_include_directories` on `quiver_tests`) in this phase
     unless testing through Lua genuinely becomes impossible for a Phase 1 scenario.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| CMake | Build configuration, new FetchContent block | ✓ | 4.3.1 (msvc build) | — |
| Ninja | Build generator (per `build-all.bat`) | ✓ | 1.13.2 | — |
| MSVC (cl.exe) | Primary compiler on this dev machine | ✓ | VS 18 Community, toolset 14.51.36231 | — |
| Network access to GitHub | `FetchContent_Declare` for `vincentlaucsb/csv-parser` | Not directly probed this session (sandboxed shell); assumed available since every other FetchContent dependency in this repo already relies on it | — | If offline, `cmake --build` will fail at configure time on the new `FetchContent_Declare` the same way it would for any existing dependency — no different fallback needed |

**Missing dependencies with no fallback:** none identified.
**Missing dependencies with fallback:** none identified — this phase adds no dependency class not
already present in the existing CMake dependency chain (FetchContent from GitHub over HTTPS).

## Security Domain

### Applicable ASVS Categories

| ASVS Category | Applies | Standard Control |
|---------------|---------|-------------------|
| V2 Authentication | No | Not applicable — no auth surface in this phase |
| V3 Session Management | No | Not applicable |
| V4 Access Control | No | Not applicable — sandboxing here is filesystem containment, not user/role access control |
| V5 Input Validation | Yes | `resolve_sandboxed_path` (path containment against a Lua script's caller-supplied string) + the new hand-rolled options-table decoder (D-15/D-17); csv-parser's own field parsing is the CSV-format input-validation layer |
| V6 Cryptography | No | Not applicable — no cryptographic material handled |
| V12 File and Resources | Yes | Path traversal prevention via `weakly_canonical` + strict containment check (existing helper, reused verbatim); denial-of-service via unbounded memory is addressed by the bounded chunk window (PARSE-09) |

### Known Threat Patterns for this stack

| Pattern | STRIDE | Standard Mitigation |
|---------|--------|----------------------|
| Path traversal (`../../etc/passwd`, absolute path outside db dir) | Tampering / Information Disclosure | `resolve_sandboxed_path`'s `weakly_canonical` + `lexically_relative` containment check (existing, reused) |
| Symlink/junction escape (a symlink inside the sandboxed dir pointing outside it) | Tampering | Not specifically covered by `weakly_canonical` (it resolves `.`/`..` lexically but historically does not always resolve symlink targets identically to `canonical` on a non-existent target) — **not previously flagged as a gap in this codebase's existing file ops** (`open_file`, `bin_to_csv`, etc. use the same helper and carry the same property), so this is a pre-existing, accepted risk surface, not a new one introduced by this phase. No action recommended beyond what already exists. |
| Memory exhaustion via adversarial CSV (huge file, pathological quoting causing full buffering) | Denial of Service | csv-parser's chunked/mmap'd reader with `CSV_ENABLE_THREADS=OFF` fixes the read window at `CSV_CHUNK_SIZE_DEFAULT` (10 MB per CONTEXT.md's citation of `common.hpp:410`), unmultiplied by worker count — addresses PARSE-09 directly |
| Malformed/dirty CSV silently corrupting data (wrong column alignment, header-row misdetection) | Tampering (of downstream data, not the file itself) | `variable_columns(KEEP_NON_EMPTY)` + `header_row(0)` pinned explicitly rather than relying on csv-parser's guessing heuristics (D-12) |
| Untrusted Lua script triggering unbounded recursion/size in the JSON return encoder | Denial of Service | Pre-existing `kMaxReturnDepth`/`kMaxReturnBytes` caps in `append_json` — unaffected by this phase since `{header, rows}` is a flat two-level table, well under either cap |

Since `security_enforcement: true` and `security_asvs_level: 1` are set in `.planning/config.json`
[VERIFIED: .planning/config.json — `"security_enforcement": true`, `"security_asvs_level": 1`,
`"security_block_on": "high"`, read this session], the planner should include a verification step
confirming the five TEST-03 sandbox negatives (escape, in-memory, missing file, directory-as-path,
subdirectory-allowed) are asserted with the exact message substrings from D-22's error catalogue,
and that no csv-parser exception can reach Lua unwrapped (LUA-08).

## Sources

### Primary (HIGH confidence — read this session)
- `src/lua_runner.cpp` (multiple ranges: 1-60, 280-360, 760-880, 990-1020, 1790-1813) — sandbox
  helper, transaction protected_function idiom, options decoder pattern, marshaler, JSON encoder
- `src/CMakeLists.txt` — SOL_ defines, QUIVER_SOURCES list, link libraries, /bigobj handling
- `cmake/Dependencies.cmake` — existing FetchContent pattern, rapidcsv pin, lua-cmake caveat
- `tests/CMakeLists.txt`, `tests/test_lua_runner.h` — fixture conventions, no `src/` include path
- `bindings/js/src/lua-api.ts` (lines 1-40, 85-115, 640-680) — doc format convention, sandbox
  bullet, binary/expression section style
- `bindings/js/test/lua-api-sync.test.ts` — exact regex mechanics of the DOC-01 build gate
- `src/CLAUDE.md`, `tests/CLAUDE.md`, `bindings/js/CLAUDE.md` — nested project conventions
- `.planning/config.json` — `nyquist_validation: false`, `security_enforcement: true`
- `.planning/PROJECT.md` — constraints, prior library-rejection rationale
- `.planning/phases/01-a-lua-script-reads-a-csv-file/01-CONTEXT.md` — the 22 locked decisions this
  research verifies against source

### Secondary (MEDIUM confidence)
- `raw.githubusercontent.com/vincentlaucsb/csv-parser/master/CMakeLists.txt` [CITED] — confirmed
  CMake option names/defaults and the AVX2-SIMD tie
- `github.com/vincentlaucsb/csv-parser` repository page [CITED] — star count, license, description

### Tertiary (LOW confidence)
- WebSearch result citing 5.3.0's presence in Vcpkg as of 2026-05-30 [CITED, not independently
  re-verified against a package index directly]

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — the one new dependency and its build-time configuration were verified
  against its own upstream CMakeLists.txt this session
- Architecture: HIGH — every pattern cited was opened and read against the current source this
  session, not inferred from CONTEXT.md's citations alone
- Pitfalls: HIGH — each pitfall traces to a specific verified line range in this codebase; the one
  exception (csv-parser's own internal file:line citations) is flagged in the Assumptions Log

**Research date:** 2026-09-15
**Valid until:** 30 days (stable C++/CMake domain; the one time-sensitive fact — csv-parser's
latest tag — should be re-checked if planning is delayed past a month)
