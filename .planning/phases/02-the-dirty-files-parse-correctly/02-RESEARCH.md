# Phase 2: The dirty files parse correctly - Research

**Researched:** 2026-09-16
**Domain:** csv-parser (`vincentlaucsb/csv-parser` 5.3.0) header-row semantics, and the Lua-side
option/validation/test work needed to expose `header_row` correctly.
**Confidence:** HIGH — the two most important unknowns (header-past-EOF, and the
`variable_columns`/`no_header` interaction) were verified empirically this session with a
throwaway probe compiled against the actual vendored library in this repo's build tree, not
inferred from reading source alone.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

- **D-20:** `header_row` is 1-based; `header_row = 0` means "no header" (`csv.header`
  absent/nil). Default `1`. The binding subtracts 1 for csv-parser's 0-based
  `format.header_row()`.
- **D-21:** LUA-06 needs no code — Phase 1's positional `header`/`rows` design already satisfies
  it. Do NOT design a name→index map.
- **D-22:** No `skip_rows` option. The units row is skipped by the script, not the reader.
- **D-23:** TEST-02 transforms in Lua and asserts the final transcribed values (not raw bytes).
- **D-24:** Fixtures renamed to ASCII (`tests/fixtures/ma_energia_residencial.csv`,
  `tests/fixtures/ma_gd_data.csv`), content copied byte-for-byte (BOM + CRLF preserved).
- **Already verified, zero code needed:** PARSE-02..07 already pass against the existing Phase 1
  reader. They are test-only requirements — do not add BOM-stripping or CRLF-trimming code.

### Claude's Discretion

(No explicit "Claude's Discretion" section was present in CONTEXT.md beyond the decisions above;
CONTEXT.md's own `<decisions>` block folds discretion into each D-2x entry's rationale. Treat
anything not pinned by D-20..D-24 above — e.g. the exact wording of the new past-EOF error
message, the exact shape of the `read_csv_options_from_lua` extension, test file organization
within `test_lua_runner_read_csv.cpp` — as open for the planner to decide, consistent with house
patterns documented below.)

### Deferred Ideas (OUT OF SCOPE)

- `skip_rows` / skip-after-header option — rejected in D-22.
- A name→index header map — rejected in D-21.
- DOC-02/DOC-03 (the agent-reference rewrite) — Phase 3.
- CSV writing — v2.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| PARSE-02 | Quoted separator is one field | Already verified in Phase 1 probe (CONTEXT.md). Test-only. |
| PARSE-03 | Quoted newline does not split record | Already verified. Test-only. |
| PARSE-04 | Doubled quote unescapes to one quote | Already verified. Test-only. |
| PARSE-05 | Leading UTF-8 BOM stripped, never in header/first cell | Already verified for `header_row=1`. **This research additionally verifies it holds for `header_row>1` (junk row before header) and for `no_header()`** — see Finding 4 below. |
| PARSE-06 | CRLF and LF both parse, no cell retains `\r` | Already verified. Test-only. |
| PARSE-07 | Ragged rows parse without throwing/truncating | Already verified for the *current* pinned format. **This research finds a call-order bug that can silently downgrade this guarantee for `no_header()` files** — see Finding 2 (must-fix). |
| LUA-05 | Script can name the header row or declare none | This research specifies the exact C++ call sequence (`Options.header_row` → `no_header()`/`header_row(n-1)`), the Lua-side decoder extension, and the past-EOF error the C++ layer must synthesize itself (csv-parser does not throw — Finding 1). |
| LUA-06 | Repeated/blank header names fully reachable | Already satisfied by Phase 1's D-01/D-21 positional design. No code. |
| TEST-01 | Automated tests cover every parser requirement, hand-written fixtures | Existing `tests/test_lua_runner_read_csv.cpp` pattern (helpers, fixture style) documented below; new fixtures needed for `header_row` cases including the past-EOF negative. |
| TEST-02 | Regression test on the two real Maranhão CSVs matching the transcribed script's values | Exact column indices, transformation logic (date reformatting, thousands-separator stripping), and fixture-copy-into-sandbox mechanics all worked out below (Findings 5-6). |
| TEST-04 | Option validation covered as throws, not silent fallback | Existing `SeparatorAs*`/`UnknownOptionKey*` tests are the pattern; four new `header_row` negatives identified (wrong type, non-integer, negative/zero-as-non-declaration ambiguity, past EOF). |
| TEST-05 | Tests pass in Release too (`SOL_SAFE_GETTER` off) | Confirmed root cause and exact runnable command below (Finding 7). Note: REQUIREMENTS.md's own traceability table (line 112) lists TEST-05 under Phase 3, while 02-CONTEXT.md's Phase Boundary explicitly pulls it into Phase 2 scope. Flagging the discrepancy — treat the CONTEXT.md phase boundary as authoritative per the task's own instructions. |

</phase_requirements>

## Summary

Phase 1 already pins three `csv::CSVFormat` settings in `make_format()` (`src/csv_read.cpp:32-37`):
`delimiter`, `variable_columns(KEEP_NON_EMPTY)`, and `header_row(0)`. Phase 2's only code change is
threading a new `header_row` option (1-based at the Lua boundary, `0` = no header) through that
same function and through `read_csv_options_from_lua` (`src/lua_runner.cpp:924-963`), which is the
one shared decoder both `db:read_csv` and `db:read_csv_stream` already route through.

Two library behaviors, found by reading `csv_format.cpp`/`csv_reader.cpp` and then **confirmed by
compiling and running a throwaway probe against the actual vendored csv-parser 5.3.0 in this
repo's build tree** (details in Finding 0), change the shape of the implementation from what a
naive reading of csv-parser's public API would suggest:

1. **A header row past the end of the file does not throw inside csv-parser.** It silently
   produces an empty header (`get_col_names().size() == 0`) and, because `trim_header()` drains
   the row queue trying to find the requested index, **zero data rows** — with the file otherwise
   read successfully. LUA-08's "header row past the end of the file" error must therefore be
   synthesized by `quiver::csv_read::Reader` itself after construction, not caught from csv-parser.

2. **`CSVFormat::header_row(int row)` has a call-order-dependent side effect**: when `row < 0`
   (i.e. `no_header()`, which is defined as `header_row(-1)`), it **overwrites
   `variable_column_policy` to `KEEP`** (`csv_format.cpp:44`). The current `make_format()` calls
   `variable_columns(KEEP_NON_EMPTY)` *before* `header_row(0)`, which is safe today only because
   `0` is not negative. The moment Phase 2 adds a "no header" path that calls `no_header()`
   (`header_row(-1)`), if that call happens *after* `variable_columns(KEEP_NON_EMPTY)`, it silently
   downgrades the policy from `KEEP_NON_EMPTY` to plain `KEEP` for every no-header read — changing
   observable behavior on blank lines (fires the callback / increments the count under `KEEP`,
   doesn't under `KEEP_NON_EMPTY`) and is a live regression risk for D-13. **`make_format()` must
   call `header_row`/`no_header` first, then `variable_columns(KEEP_NON_EMPTY)` last**, so the
   explicit pin always wins regardless of the requested header mode.

**Primary recommendation:** Reorder `make_format()` to set header mode before variable-columns
policy; make the C++ `Reader` throw its own Pattern 1 "header row not found" error whenever a
caller explicitly requested a header (Lua `header_row >= 1`) and the resulting `header()` comes
back empty — this uniformly covers both "past EOF" and the degenerate "requested row is a fully
blank line" edge case without needing to distinguish them.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| CSV parsing / header-row semantics | C++ core (`src/csv_read.cpp`) | — | Single Pimpl'd wrapper around csv-parser; only consumer is Lua. |
| Option decoding / validation (`header_row` type, range, throw-on-bad-value) | C++ Lua binding (`src/lua_runner.cpp`, `read_csv_options_from_lua`) | — | Shared decoder for both `db:read_csv`/`db:read_csv_stream`, per LUA-03; this is where LUA-08's negatives are raised for Lua-supplied bad values (wrong type, non-integer, negative). |
| Past-EOF header detection | C++ core (`quiver::csv_read::Reader`) | — | csv-parser itself does not signal this (Finding 1); it must be synthesized where the library's silent behavior is observed, immediately after construction — not deferred to the Lua layer, which has no visibility into row counts. |
| Real-file value transformation (date reformatting, thousands-separator stripping) | Lua script (test/user script) | — | Per LUA-07 (no inference) and D-23 (test transforms and asserts final values) — this is explicitly script-side logic, not reader logic. |
| Test fixtures | `tests/fixtures/` (new dir) + `tests/test_lua_runner_read_csv.cpp` | — | Following the existing `tests/schemas/` convention of shared, path-based fixtures; `tests/test_lua_runner_read_csv.cpp` already owns all `db:read_csv`/`db:read_csv_stream` coverage with no sibling elsewhere (documented in `tests/CLAUDE.md`). |

## Standard Stack

No new dependencies. This phase extends the existing internal `quiver::csv_read` wrapper
(`src/csv_read.h/.cpp`) around the already-vendored `vincentlaucsb/csv-parser` 5.3.0
(`cmake/Dependencies.cmake`), which Phase 1 already pinned and fetches via `FetchContent`. No
package legitimacy audit applies — no new package is introduced.

## Findings (empirically verified this session)

### Finding 0 — Method: how these were verified

`quiver_sandbox` (`tests/sandbox/sandbox.cpp`, `tests/CMakeLists.txt:112-120`) normally links only
`quiver` + `quiver_compiler_options`, not `csv` — so it cannot include csv-parser headers as-is.
For this research, `csv` was **temporarily** added to `quiver_sandbox`'s link libraries and a probe
`main()` was written that exercises `csv::CSVFormat`/`csv::CSVReader` directly (the same headers
`src/csv_read.cpp` already includes: `<internal/csv_reader.hpp>`). Built with
`cmake --build build --config Debug --target quiver_sandbox`, run as
`./build/bin/quiver_sandbox.exe`. **Both files were reverted to their original content
immediately after (`git status --porcelain` confirmed clean, only unrelated `.gsd/` untracked),
and `quiver_sandbox` was rebuilt from the restored source to leave the binary consistent with the
committed tree.** No repo file was left modified. This is the recommended technique for the
planner/executor if further csv-parser behavior needs checking later — `quiver_sandbox` is exactly
the intended scratch target for this (`tests/CLAUDE.md`: "intentional scratch target… links only
against the C++ core", extend its link libraries transiently, never commit the change).

### Finding 1 — `header_row(n)` past EOF: csv-parser is SILENT, not throwing `[VERIFIED: build/_deps/csv_parser-src/include/internal/csv_reader.cpp:74-83, empirically run this session]`

Source (`csv_reader.cpp:74-83`):
```cpp
CSV_INLINE void CSVReader::trim_header() {
    if (!this->header_trimmed) {
        for (int i = 0; i <= this->_format.header && !this->records->empty(); i++) {
            if (i == this->_format.header && this->col_names->empty()) {
                this->set_col_names(this->records->pop_front());
            }
            else {
                this->records->pop_front();
            }
        }
        this->header_trimmed = true;
    }
}
```
The loop pops one raw record per iteration up to the requested header index. If the file runs out
of records (`this->records->empty()`) before `i` reaches `_format.header`, the loop simply stops —
`col_names` is never set (stays empty), no exception is raised anywhere in this path, and **every
row in the file has already been consumed by the trim loop**, so zero data rows remain.

**Empirical confirmation** (probe, 3-line file `a,b,c\n1,2,3\n4,5,6\n`, `header_row(5)` 0-based,
`variable_columns(KEEP_NON_EMPTY)` already set):
```
[TestA header_row(5) on 3-line file] header (0): []
[TestA] rows iterated after header_row(5): 0
```
No exception was thrown; the reader constructed successfully with an empty header and zero rows.

**Implication for LUA-08:** the `Cannot read_csv: ...` "header row past the end of the file" error
must be raised by `quiver::csv_read::Reader`'s constructor (in `src/csv_read.cpp`, alongside the
existing not-found/directory/empty checks) by **checking the result after construction**, not by
catching an exception from csv-parser. Recommended condition: *the Lua caller explicitly requested
a header row (`header_row_lua >= 1`) AND `reader.get_col_names().empty()`* → throw. This also
covers the (untested, but logically implied) case where the requested header row exists but is a
fully blank line — `set_col_names` would receive a zero-length vector too, which is
indistinguishable from "ran out of rows" using only `get_col_names()`. Treating both as the same
"header row not found/empty" error is a reasonable, deliberately simple choice: there is no
`no requirement asks for `distinguishing "blank header line" from "past EOF"`, and a header that
resolves to zero column names is unusable either way.

**Do not** use `header()` emptiness alone to detect this — `header_row = 0` (the "no header"
case, Lua-side) *also* produces an empty `get_col_names()` by design (Finding 3). The
disambiguating signal must be the caller's original request (was a header explicitly wanted?),
which is available in `Options.header_row` before it's translated into the csv-parser call.

### Finding 2 — `no_header()` silently overwrites `variable_column_policy` — call order matters `[VERIFIED: build/_deps/csv_parser-src/include/internal/csv_format.cpp:43-49, empirically run this session]`

Source (`csv_format.cpp:43-49`):
```cpp
CSV_INLINE CSVFormat& CSVFormat::header_row(int row) {
    if (row < 0) this->variable_column_policy = VariableColumnPolicy::KEEP;

    this->header = row;
    this->header_explicitly_set_ = true;
    this->col_names = {};
    this->col_names_explicitly_set_ = false;
    return *this;
}
```
`no_header()` is defined as `header_row(-1)` (`csv_format.hpp:99-105`), so calling it **always**
resets `variable_column_policy` to `KEEP` (not `KEEP_NON_EMPTY`) as a side effect, *if it runs
after* an earlier `variable_columns(...)` call.

**Empirical confirmation:**
```
[TestB] policy after variable_columns() THEN no_header(): 1 (KEEP_NON_EMPTY=2, KEEP=1, IGNORE_ROW=0)
[TestB] policy after no_header() THEN variable_columns(): 2 (KEEP_NON_EMPTY=2, KEEP=1, IGNORE_ROW=0)
```
Calling `variable_columns(KEEP_NON_EMPTY)` **first**, then `no_header()`, ends with policy `KEEP`
(1) — the pin is lost. Calling `no_header()` first, then `variable_columns(KEEP_NON_EMPTY)`,
correctly ends with `KEEP_NON_EMPTY` (2).

**Practical difference between `KEEP` and `KEEP_NON_EMPTY`** (both already satisfy PARSE-07's
"ragged rows don't throw or truncate" — the difference is blank-line handling, which is D-13's
concern): under `KEEP`, a fully blank line becomes a zero-length row that fires the row-sink/counts
as a row; under `KEEP_NON_EMPTY`, `accept_row` (`csv_reader.cpp:104-107`) explicitly discards a
zero-length row before it ever reaches the sink or the row count (D-13's requirement — "a blank
line mid-file is not a row"). If Phase 2's implementation adds the no-header path by calling
`no_header()` *after* the existing `variable_columns(KEEP_NON_EMPTY)` call in `make_format()`
(the naturally tempting edit — just insert a branch where `header_row(0)` currently sits), it
silently reintroduces phantom blank-line rows for every no-header file, regressing D-13 with no
test currently guarding the *order* of these two calls (only their end-state via `header_row(0)`,
which is not the no-header path).

**Required fix:** in `make_format()` (`src/csv_read.cpp`), set the header mode
(`no_header()` or `header_row(n-1)`) **before** the `variable_columns(KEEP_NON_EMPTY)` call, so the
explicit pin always applies last regardless of which header mode was requested. Concretely, reorder
the three existing lines:
```cpp
csv::CSVFormat format;
format.delimiter(options.separator);
if (options.header_row == 0) {
    format.no_header();
} else {
    format.header_row(options.header_row - 1);
}
format.variable_columns(csv::VariableColumnPolicy::KEEP_NON_EMPTY);  // must be last
return format;
```
This is a one-line reordering risk that a naive "just add a branch where `header_row(0)` already
is" edit would get backwards. Add a test that specifically exercises a blank line mid-file **with
`header_row = 0`** (not just with the default header) to guard this ordering permanently — there
is currently no such test in `test_lua_runner_read_csv.cpp` because `header_row` doesn't exist yet.

### Finding 3 — `no_header()` correctly yields empty header + all rows as data, when order is correct `[VERIFIED: empirically run this session]`

```
[TestC no_header()] header (0): []
[TestC] rows iterated: 3
```
(3-line file with no header requested, `no_header()` called before `variable_columns`.) Confirms
D-20's contract: `header_row = 0` (Lua) → `no_header()` (C++) → `get_col_names()` is a genuine
empty vector, and `reader.header().empty()` is exactly the condition Phase 1's `read_csv`/
`read_csv_stream` bindings already use to omit `header` from the returned table
(`src/lua_runner.cpp:473-476`, comment: "header_row(0) always designates a header for a non-empty
file today; the guard is forward-looking" — Phase 2 is that forward-looking case arriving).

### Finding 4 — BOM stripping is unconditional, independent of header row position or "no header" mode `[VERIFIED: build/_deps/csv_parser-src/include/internal/parser/driver.cpp:39-73 (`get_bom_skip_or_throw`), empirically run this session]`

BOM detection/skip happens once, on the raw byte stream (`get_bom_skip_or_throw`, called from the
mmap-based head read), entirely independent of where the header row is or whether one exists.
Confirmed three ways:

- **Header not at row 0** (junk row 0 with a BOM, real header at 0-based row 1):
  ```
  [TestD header_row(1) with BOM on row 0] header (3): ['x', 'y', 'z']
  [TestD] row[0]='1'
  ```
  Header extracted cleanly (`'x'`, not `'\ufeffx'`); first data cell clean (`'1'`).
- **`no_header()` with a BOM at the very start of the file:**
  ```
  [TestG no_header() with BOM] header (0): []
  [TestG] row[0] length=1 bytes=61  value='a'
  ```
  (hex `0x61` = ASCII `'a'`, confirmed via byte dump, not just visual inspection — the BOM's 3
  bytes are gone, not merely invisible in the terminal.)

**Conclusion:** no special-casing is needed for BOM handling under `header_row != 1` or
`header_row = 0`. Phase 1's PARSE-05 code path (none — it's a csv-parser built-in) continues to
cover every `header_row` value with no additional code.

### Finding 5 — Blank line BEFORE the header counts toward the header index `[VERIFIED: empirically run this session]`

```
[TestF header_row(1) with blank line at row 0] header (3): ['a', 'b', 'c']
[TestF] row[0]='1'
```
File content: `"\na,b,c\n1,2,3\n"` (blank line 0, header at 0-based row 1, data at row 2).
`header_row(1)` correctly picked `a,b,c` — meaning `trim_header`'s counting loop operates on the
**raw** record queue (before `accept_row`'s `variable_columns` filtering is applied), so a blank
line does count as one index position even though it will never appear as a *data* row under
`KEEP_NON_EMPTY`. **This matters for how a user picks `header_row`**: if a file has a genuinely
blank separator line above the header, that blank line still occupies one index. No code change
is implied — this is purely a fact for the Lua-facing documentation (Phase 3, DOC-02/03) and for
choosing correct `header_row` values in the two real fixtures. Neither real Maranhão file has a
blank line before its header, so this does not affect TEST-02.

### Finding 6 — `header_row` naming a row exactly at EOF (header found, zero data after) is NOT an error `[VERIFIED: empirically run this session]`

```
[TestE header_row(1) is the last line] header (3): ['a', 'b', 'c']
[TestE] rows iterated: 0
```
A header that *is* found (non-empty) with zero data rows following it is a legitimate
"header-only file" — same as Phase 1's existing `HeaderOnlyFileYieldsEmptyRows` test
(`test_lua_runner_read_csv.cpp:167`), just reached via an explicit `header_row` instead of the
default. This is the case that must be distinguished from Finding 1 (header genuinely not found):
the distinguishing signal is whether `get_col_names()` came back non-empty, not the row count.

## Real fixture files: exact structure and required transformation (TEST-02)

Both files were read directly (`Read` tool + `head -c`/`xxd`/`grep`), not inferred.

### Energia Consumida Residencial do Maranhão.csv

**Byte-level facts** `[VERIFIED: read directly this session]`: starts with UTF-8 BOM
(`EF BB BF`), line endings are CRLF (`\r\n`) throughout, 261 total lines.

Structure (1-based line numbers, matching D-22 in CONTEXT.md):

| Line | Content | Role |
|------|---------|------|
| 1 | `SÉRIE ANUAL,,,SÉRIE MENSAL,,,,,,,` | junk title row, above header |
| 2 | `ANO,Residencial,,ANO,MÊS, Residencial ,,,,,` | the real header — **11 columns** |
| 3 | `,MWh,,,, MWh ,,,,,` | units row — becomes `rows[1]` once `header_row=2` |
| 4–261 | e.g. `2005,1'114'144,,2005,01/01/2005, 93'943 ,,,,,` | 258 data rows |

**`header_row = 2`** (1-based, per D-20) → C++ `format.header_row(1)` (0-based). Lua's
`csv.rows[1]` = the units row (line 3); real data starts at `csv.rows[2]`.

**Column indices (1-based, within each row array)** `[VERIFIED: read raw file bytes this
session — grep line 226 quoted verbatim below]`:
- `row[4]` — year (annual block's own year, mirrored; e.g. `"2023"`) — **not the one to use for the
  date**, since the annual and monthly blocks are duplicated but only the monthly (cols 4-6) stays
  populated for the whole file range (columns 1-3, the annual block, go empty after the annual
  series ends around 2009 — confirmed: line 226 is `,,,2023,01/07/2023, 386'433 ,,,,,`, cols 1-3
  blank).
- `row[5]` — date, format `DD/MM/YYYY`, day is always `"01"` (verified: every date in the file
  matches `^01/`) — e.g. `"01/07/2023"`.
- `row[6]` — value, format ` NNN'NNN'NNN ` (leading/trailing space, apostrophe thousands
  separators) — e.g. `" 386'433 "`.
- `row[1]`, `row[2]`, `row[3]`, `row[7]`–`row[11]` — irrelevant to TEST-02 (annual block / trailing
  blanks).

**Verified target values** (D-23's `obs_str` targets), confirmed present in the raw file:
- Line 226 (raw, via `grep -n "01/07/2023"`): `,,,2023,01/07/2023, 386'433 ,,,,,` →
  `row[5]="01/07/2023"`, `row[6]=" 386'433 "` → target `2023-07 386433`.
- The `2005-01 93943` target (from `01/01/2005` / `" 93'943 "`) is the same file's first data
  line (line 4, shown above verbatim in the hex dump).

**Required Lua transformation** (in the test/replacement script, per D-23 — this is
script-side logic, not reader logic, per LUA-07):
```lua
-- date: "DD/MM/YYYY" -> "YYYY-MM"
local dd, mm, yyyy = row[5]:match("(%d%d)/(%d%d)/(%d%d%d%d)")
local date_key = yyyy .. "-" .. mm

-- value: " NNN'NNN'NNN " -> integer
-- NOTE the double-parens: gsub returns TWO values (string, count); tonumber would otherwise
-- receive the replacement count as its base argument and silently return nil (D-23's documented
-- trap, to be written up in Phase 3's DOC-03).
local n = tonumber((row[6]:gsub("['%s]", "")))
```
Loop skips `rows[1]` (the units row): `for i = 2, #csv.rows do ... end`.

### gd_data_maranhao.csv

**Byte-level facts** `[VERIFIED: read directly this session]`: **no BOM**, LF-only line endings
(confirmed via hex dump — no `0d` bytes present), 103 total lines.

Structure:

| Line | Content | Role |
|------|---------|------|
| 1 | `project_updated_at: Month,Cumulative sum of installed_capacity_kw` | header — **2 columns**, `header_row = 1` (the D-20 **default**, no option needed) |
| 2–103 | e.g. `"May 1, 2014",33` | 102 data rows |

**Column indices (1-based)**:
- `row[1]` — date, format `"Month D, YYYY"` (English month name, day is always `1` — verified via
  `grep -oE '"[A-Za-z]+ [0-9]+,'` across the whole file, every match is `<Month> 1,`), e.g.
  `"May 1, 2014"`.
- `row[2]` — value, **already a plain decimal string** (e.g. `"33"`, `"51818.33"`) — no
  thousands-separator or whitespace cleanup needed, unlike the Energia file.

**Verified target values** (D-23's `gd_str` targets), confirmed present in the raw file:
- Line 2: `"May 1, 2014",33` → target `2014-05 33`.
- Line 72 (via `grep -n "2021"`): `"July 1, 2021",51818.33` → target `2021-07 51818.33`.

**Required Lua transformation** — English month name has no `os.date`/`os.time` available (the
Lua sandbox has `os` unloaded per root CLAUDE.md's design decision), so a hand-rolled lookup table
is required:
```lua
local MONTHS = {
  January=1, February=2, March=3, April=4, May=5, June=6,
  July=7, August=8, September=9, October=10, November=11, December=12,
}
local month_name, _, year = row[1]:match("(%a+) (%d+), (%d+)")
local date_key = string.format("%d-%02d", tonumber(year), MONTHS[month_name])
local value = tonumber(row[2])  -- no cleanup needed, already plain decimal text
```
Loop starts at `rows[1]` (no units row to skip) — `for i = 1, #csv.rows do ... end`.

## Test fixture mechanics

### Registration (`tests/fixtures/` does not exist yet)

`tests/CMakeLists.txt` registers test sources explicitly (no glob) — this convention does **not**
extend to data fixtures. `tests/schemas/` (the existing shared-fixture directory) contains
`.sql` files that are never listed in CMake at all; they're located at test runtime purely via
`quiver::test::path_from(__FILE__, "schemas/valid/...")`
(`tests/test_utils.h:12-15,28-30`), which builds an absolute path from the **compiled-in
`__FILE__`** of the test source, independent of the working directory the test binary is launched
from. The same mechanism works unmodified for a new `tests/fixtures/` directory — no CMake change
is needed to "install" or "copy" fixtures into the build tree; they're read straight from the
source tree at test run time. Add:
```cpp
quiver::test::path_from(__FILE__, "fixtures/ma_energia_residencial.csv")
```
from within `tests/test_lua_runner_read_csv.cpp` (same directory as `test_utils.h`'s definitions,
one level up from `schemas/`, so the relative fixture path is `"fixtures/..."` not
`"../fixtures/..."` — confirm by checking `test_lua_runner_read_csv.cpp`'s own directory, which is
`tests/`, the same level as `tests/schemas/`).

### Sandbox copy step (required — `db:read_csv` resolves relative to the DB directory)

`LuaSandboxTest` (`tests/test_lua_runner.h:23-37`) exposes a protected `std::filesystem::path
sandbox` member — the per-test temp directory containing `test.db`. Since `resolve_sandboxed_path`
resolves a script's relative path against that directory (not the source tree), the committed
fixture must be **copied** into `sandbox` before the Lua script reads it by a relative name:
```cpp
std::filesystem::copy_file(
    quiver::test::path_from(__FILE__, "fixtures/ma_energia_residencial.csv"),
    sandbox / "ma_energia_residencial.csv");
```
`std::filesystem::copy_file` copies bytes as-is on both Windows and POSIX (no text-mode line-ending
translation), so the BOM and CRLF bytes survive the copy — this preserves what D-24 requires
("content must be copied byte-for-byte… verify the committed bytes still contain `\r\n` and the
leading `EF BB BF` after checkout"). Existing tests already write files directly into `sandbox`
via `write_lua_csv_file` (`tests/test_lua_runner_read_csv.cpp:20-23`, itself
`std::ofstream(path, std::ios::binary)` — also binary-safe); `copy_file` is the same idea for a
fixture that already exists on disk rather than one built in the test body.

### Verifying the committed bytes survive git (D-24's own caveat)

`.gitattributes` enforces LF only for `.cpp/.h/.dart/.jl/.py` — `.csv` is untouched, so no
normalization risk exists for these two fixtures *if* they are committed with `git add` in binary
mode (default on all platforms; Windows `core.autocrlf` could theoretically interfere only if a
global/user config forces CRLF normalization on all text-like files without a `.gitattributes`
override — worth an explicit `git check-attr -a tests/fixtures/ma_energia_residencial.csv` after
adding, and a hex-dump spot-check of the file at `HEAD` after commit, to be safe. This is a
one-time verification step for whoever adds the fixtures, not ongoing code.

## Options table and validation (LUA-05, TEST-04)

### Existing decoder to extend

`read_csv_options_from_lua` (`src/lua_runner.cpp:924-963`) is the one shared decoder. Its existing
shape:
```cpp
static csv_read::Options read_csv_options_from_lua(const sol::object& options, const std::string& operation) {
    csv_read::Options result;
    if (!options.valid() || options.get_type() == sol::type::lua_nil) return result;
    if (options.get_type() != sol::type::table) {
        throw std::runtime_error("Cannot " + operation + ": options must be a table");
    }
    std::vector<std::pair<std::string, sol::object>> entries;
    options.as<sol::table>().for_each([&](sol::object key, sol::object value) {
        entries.emplace_back(key.as<std::string>(), std::move(value));
    });
    std::optional<sol::object> separator_value;
    for (auto& entry : entries) {
        if (entry.first != "separator") {
            throw std::runtime_error("Cannot " + operation + ": unknown option '" + entry.first + "'");
        }
        separator_value = entry.second;
    }
    // ... separator validation ...
    return result;
}
```
Phase 2 extends the `if (entry.first != "separator")` rejection to accept `"header_row"` too, and
adds a second validated field. **The existing `FutureHeaderKeyIsAnUnknownOptionToday` test
(`tests/test_lua_runner_read_csv.cpp:436-444`) asserts `{ header = false }` throws "unknown option
'header'"** — note its comment says "A key Phase 2 will legitimately add", but the *actual* key
name settled on in CONTEXT.md's D-20 is `header_row`, not `header`. This test's assertion likely
still holds unmodified after Phase 2 (`header` remains an unknown key forever — only `header_row`
becomes known), but its **comment is now factually wrong** and should be corrected by the planner
to avoid confusing a future reader into thinking `header` was the intended option name.

### `Options` struct extension (`src/csv_read.h`)

```cpp
struct Options {
    char separator = ',';
    int64_t header_row = 1;  // 1-based; 0 = no header (D-20)
};
```

### Validation rules for `header_row` (TEST-04 negatives)

Following the house pattern for "must be an integer" (`src/lua_runner.cpp:1661`:
`if (!cell.first.is<int64_t>() || cell.first.as<int64_t>() < 1)`, used for group-column indices)
and the existing `separator` validation's explicit-`get_type()` style (D-17: never route through
`lua_cell_as`, which surfaces a raw sol2 message):

```cpp
if (header_row_value) {
    if (!header_row_value->is<int64_t>()) {
        throw std::runtime_error("Cannot " + operation + ": option 'header_row' must be an integer");
    }
    auto header_row = header_row_value->as<int64_t>();
    if (header_row < 0) {
        throw std::runtime_error("Cannot " + operation + ": option 'header_row' must not be negative");
    }
    result.header_row = header_row;
}
```
**Verify `.is<int64_t>()` behaves identically in Release** (this is exactly the class of
`sol::object` type-check TEST-05 exists to catch — see Finding 7). It is the same idiom already
used at `src/lua_runner.cpp:1661`, so if that call site is Debug/Release-safe today, this one is
by construction — but the phase's own TEST-05 requirement means this must be *run*, not assumed.

**Four throwing negatives for TEST-04** (mirroring the existing `separator` negative-test
structure at `tests/test_lua_runner_read_csv.cpp:455-491`):
1. `{ header_row = "2" }` → wrong type → `"Cannot read_csv: option 'header_row' must be an integer"`
2. `{ header_row = 2.5 }` → non-integer number → same message (relies on `.is<int64_t>()` rejecting
   a non-whole float; **verify this empirically when implementing** — sol2's exact `is<int64_t>()`
   semantics for a Lua float like `2.5` were not probed in this research session and should be
   checked with a two-line addition to the probe technique in Finding 0 before relying on it).
3. `{ header_row = -1 }` → negative → `"Cannot read_csv: option 'header_row' must not be negative"`
4. `{ header_row = 99 }` on a file with fewer than 99 lines → **the past-EOF case (Finding 1)** →
   a *different* error, raised by `csv_read::Reader`'s constructor, not the options decoder — e.g.
   `"Cannot read_csv: header row 99 not found in file '<path>'"` (exact wording is open — no
   existing message in the D-22 catalogue from Phase 1 covers this case; follow the catalogue's
   established grammar `Cannot <op>: <reason>` and thread `operation`/`original_path` the same way
   the existing nine messages do).

## Common Pitfalls

### Pitfall 1: Trusting csv-parser to validate `header_row` bounds
**What goes wrong:** Assuming a header row past EOF throws from within `csv::CSVReader`'s
constructor, and therefore relying on the existing generic catch-and-wrap
(`src/csv_read.cpp`'s `catch (const std::exception& e)` around the `CSVReader` construction) to
produce the LUA-08 error "for free".
**Why it happens:** Every *other* csv-parser failure this repo has encountered so far (bad file,
I/O errors) does throw from inside that same try block, so it's a reasonable but wrong
generalization.
**How to avoid:** Add an explicit post-construction check (Finding 1) rather than relying on an
exception.
**Warning signs:** A test asserting `header_row = 99` throws would pass vacuously if the assertion
is written loosely (e.g. checking only that *some* row-count-related empty response occurs) —
write the test to assert the exact `Cannot read_csv: ...` message, and confirm via
`expect_prefixed_error`/`expect_lua_error` that it is a genuine throw, not a silently-empty
success (an easy false-positive: `db:read_csv(...)` returning `{header=nil, rows={}}` "looks like"
an empty-but-valid no-header file if the test doesn't check that a header was actually requested).

### Pitfall 2: Reordering `make_format()`'s calls incorrectly
**What goes wrong:** Adding the `header_row`/`no_header()` branch in the same textual position
`header_row(0)` currently occupies (i.e., *after* `variable_columns(KEEP_NON_EMPTY)`), which is the
natural, minimal-diff edit — and which reintroduces the call-order bug in Finding 2.
**Why it happens:** The existing code's three lines look independent/order-agnostic at a glance;
nothing in the current comment block flags that `header_row`/`no_header` has a side effect on
`variable_column_policy`.
**How to avoid:** Move `variable_columns(KEEP_NON_EMPTY)` to be the *last* call in `make_format()`,
after header mode is set, and add an inline comment citing `csv_format.cpp:44`
(`if (row < 0) this->variable_column_policy = VariableColumnPolicy::KEEP;`) so a future edit
doesn't reorder it back.
**Warning signs:** A test with a blank line mid-file under `header_row = 0` unexpectedly firing the
row-sink/incrementing the row count for that blank line.

### Pitfall 3: Conflating "no header" (`header_row=0`) with "header not found" (past EOF)
**What goes wrong:** Both produce `reader.header().empty() == true`; a check that only looks at
`header().empty()` cannot tell them apart, and would either (a) wrongly throw for every
legitimate no-header file, or (b) wrongly succeed silently for a past-EOF request.
**How to avoid:** Gate the past-EOF check on the *caller's original request* (`header_row_lua >=
1`, before translation to the 0-based csv-parser call), not on the post-construction header state
alone.

## Code Examples

### `make_format()` after the fix (Finding 2's reordering)

```cpp
// Source: this session's research, verified against build/_deps/csv_parser-src 5.3.0
csv::CSVFormat make_format(const Options& options) {
    csv::CSVFormat format;
    format.delimiter(options.separator);
    // Header mode MUST be set before variable_columns(): CSVFormat::header_row(int row), when
    // row < 0 (i.e. no_header()), overwrites variable_column_policy to KEEP as a side effect
    // (csv_format.cpp:44) -- setting variable_columns() afterward is what makes our
    // KEEP_NON_EMPTY pin win regardless of which header mode was requested.
    if (options.header_row == 0) {
        format.no_header();
    } else {
        format.header_row(static_cast<int>(options.header_row - 1));
    }
    format.variable_columns(csv::VariableColumnPolicy::KEEP_NON_EMPTY);
    return format;
}
```

### Past-EOF detection in `Reader::Reader` (after existing not-found/directory/empty checks)

```cpp
// Source: this session's research (Finding 1) -- csv-parser does not throw for this case.
try {
    csv::CSVReader reader(resolved_path, make_format(options));
    auto header = reader.get_col_names();
    if (options.header_row != 0 && header.empty()) {
        throw std::runtime_error("Cannot " + operation + ": header row " +
                                  std::to_string(options.header_row) + " not found in file '" +
                                  original_path + "'");
    }
    impl_ = std::make_unique<Impl>(operation, original_path, std::move(header), std::move(reader));
} catch (const std::exception& e) {
    throw std::runtime_error("Cannot " + operation + ": cannot read file '" + original_path + "': " + e.what());
}
```
**Caution:** the `throw` for the past-EOF case must not be re-caught and re-wrapped by the
surrounding `catch` (which would double-prefix "Cannot read_csv: cannot read file... Cannot
read_csv: header row..."). Either raise it outside that try block (after catching only the
`CSVReader` construction itself in a narrower try), or check `e.what()`'s content isn't already
Pattern-1-prefixed before wrapping — the cleaner fix is scoping the try to just the `CSVReader`
constructor call and doing the empty-header check outside it, mirroring how the three
not-found/directory/empty checks already sit outside any try block in the current code.

## State of the Art

Not applicable — no external ecosystem shift; this is entirely about correctly using a
library already vendored in Phase 1.

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | `sol::object::is<int64_t>()` rejects a non-integer Lua float (e.g. `2.5`) the same way in Debug and Release | Options table validation | If it accepts `2.5` and truncates silently, `header_row = 2.5` would misbehave without throwing — a TEST-04/TEST-05 gap. Verify with a two-line probe (extend Finding 0's technique) before relying on it in the plan. |
| A2 | The exact wording `"Cannot read_csv: header row N not found in file '<path>'"` is acceptable phrasing for the past-EOF error | Options/error catalogue extension | No requirement pins exact wording beyond the `Cannot read_csv: ...` prefix (LUA-08), so this is a naming choice, not a correctness risk — but the planner should confirm it reads naturally next to Phase 1's D-22 catalogue and doesn't collide with any other message. |
| A3 | `tests/fixtures/` (new dir) needs no CMake registration, following the `tests/schemas/` precedent | Test fixture mechanics | Low risk — `tests/schemas/` is proven to work this way today across every C++/C API suite; the same `path_from(__FILE__, ...)` mechanism is file-format-agnostic. |

**If this table is empty:** N/A — see entries above; none are high-risk, all are cheap to
verify at implementation time using the exact technique demonstrated in Finding 0.

## Open Questions

1. **Exact wording of the past-EOF error message.**
   - What we know: must start with `Cannot read_csv: ` (or `Cannot read_csv_stream: `) per LUA-08
     and follow Pattern 1 grammar.
   - What's unclear: the exact noun phrase (`"header row N not found"` vs `"header row N is past
     the end of file"` vs something else).
   - Recommendation: planner picks final wording; not a technical risk, purely a naming decision.
     Suggest matching the phrasing style of the requirement text itself ("header row past the end
     of the file") for grep-ability between REQUIREMENTS.md and the test assertions.

2. **Does `.is<int64_t>()` reject `2.5` identically in Debug and Release?**
   - What we know: it's the existing house idiom (`src/lua_runner.cpp:1661`) for "must be an
     integer", already used elsewhere in this codebase.
   - What's unclear: not verified for this exact input (`2.5`) or cross-checked against
     `SOL_SAFE_GETTER`'s Debug/Release behavior in this research session.
   - Recommendation: extend this session's `quiver_sandbox` probe technique (Finding 0) with a
     two-line sol2-only check before finalizing the validation code, or simply write the TEST-04
     non-integer case and run it under both `scripts/build-all.bat` (Debug, implicit) and the
     Release command below (TEST-05) before considering the phase done.

## Finding 7: TEST-05 — concrete Release-build commands

`SOL_SAFE_GETTER` defaults ON when compiled as a debug build and OFF otherwise
`[VERIFIED: build/_deps/sol2-src/include/sol/version.hpp:323-337]`:
```cpp
#if defined(SOL_SAFE_GETTER)
    ...
#else
    #if SOL_IS_ON(SOL_ALL_SAFETIES_ON)
        #define SOL_SAFE_GETTER_I_ SOL_ON
    #elif SOL_IS_ON(SOL_DEBUG_BUILD)
        #define SOL_SAFE_GETTER_I_ SOL_DEFAULT_ON
    #else
        #define SOL_SAFE_GETTER_I_ SOL_DEFAULT_OFF
    #endif
#endif
```
This repo defines neither `SOL_SAFE_GETTER` nor `SOL_ALL_SAFETIES_ON` explicitly
(`src/CMakeLists.txt:60-62` only sets `SOL_SAFE_FUNCTION=1`, which is itself a dead define per
01-CONTEXT.md's deferred item — it should be `SOL_SAFE_FUNCTION_CALLS`/`SOL_SAFE_FUNCTION_OBJECTS`
but is not, and that's out of scope here), so `SOL_DEBUG_BUILD`'s own NDEBUG-based detection is
what flips this — i.e. plain `CMAKE_BUILD_TYPE=Release` (which defines `NDEBUG`) is sufficient to
turn `SOL_SAFE_GETTER` off, with no extra defines needed in this repo's CMake.

**Two ways to produce a Release build here**, both already documented at the root level:

1. **`CMakePresets.json`'s `release` preset does NOT build tests** (`"QUIVER_BUILD_TESTS": "OFF"`,
   `CMakePresets.json:29-37`) — using it alone would produce a Release `quiver_tests.exe`-less
   build. Do not use this preset for TEST-05 as-is.
2. **`scripts/build-all.bat --release`** (or the manual configure line it wraps) is the correct
   path — it always sets `QUIVER_BUILD_TESTS=ON` regardless of build type
   (`scripts/build-all.bat:53`: `cmake ... -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DQUIVER_BUILD_TESTS=ON
   -DQUIVER_BUILD_C_API=ON`), reusing the single-config Ninja `build/` directory in place. This
   **reconfigures the existing `build/` dir from Debug to Release**, which is fine functionally
   (Ninja handles the switch) but means a subsequent Debug run needs to reconfigure back, and any
   probe technique from Finding 0 done in Debug won't carry over without rebuilding.

**Recommended runnable commands** (use a separate directory to avoid disturbing the existing
Debug `build/` used throughout day-to-day development, mirroring what `windows-release`'s preset
does with `build/windows-release`):
```bash
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DQUIVER_BUILD_TESTS=ON -DQUIVER_BUILD_C_API=ON
cmake --build build-release --config Release
./build-release/bin/quiver_tests.exe --gtest_filter='LuaRunner*'
```
(Or, to match the root CLAUDE.md's documented path exactly and accept the in-place reconfigure:
`scripts/build-all.bat --release`, which additionally runs the C API tests and every binding
suite — slower, but this is the house-blessed one-liner and requires no new directory.)

**Concrete risk for this phase's new code:** the `header_row` option's `.is<int64_t>()` type
check (see Options table section above) is exactly the kind of `sol::object` decoding that
previously hid bugs only visible in Release — per `tests/CLAAUDE.md`'s note on the three mixed-array
tests (`CreateElementMixedIntegerAndBooleanArray`, etc.), which "cover bugs that only manifested
with `SOL_SAFE_GETTER` off... a Debug-only run cannot prove the fix." The `LuaRunner*` filter above
already includes the new `header_row` tests (`LuaRunner_ReadCsv` test suite) without needing a
narrower filter.

## Environment Availability

Not applicable — this phase touches only code already built and tested in this environment
(csv-parser is already vendored and building; CMake/Ninja/MSVC toolchain already confirmed
working via the builds performed during this research session).

## Validation Architecture

### Test Framework

| Property | Value |
|----------|-------|
| Framework | GoogleTest (already vendored, `googletest` v1.17.0) |
| Config file | `tests/CMakeLists.txt` (explicit source registration, `gtest_discover_tests`) |
| Quick run command | `./build/bin/quiver_tests.exe --gtest_filter='LuaRunner_ReadCsv.*'` |
| Full suite command | `./build/bin/quiver_tests.exe` (all C++ core tests) |

### Phase Requirements → Test Map

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| PARSE-02..07 | Dirty-CSV parsing properties | unit (via Lua boundary) | `quiver_tests.exe --gtest_filter='LuaRunner_ReadCsv.*'` | ✅ existing fixtures already cover this per 02-CONTEXT.md's verified probe; new `header_row`-specific ragged/blank-line fixtures needed — Wave 0 gap below |
| LUA-05 | `header_row` names/omits header | unit | same filter, new `LuaRunner_ReadCsv` tests | ❌ Wave 0 |
| LUA-06 | Duplicate/blank header names reachable | unit (documentation via test, no new code) | same filter | ✅ can reuse the existing `header`/`rows` positional tests conceptually; a new explicit test naming duplicate/blank headers under `header_row` is still worth adding | ❌ Wave 0 (new test, no new code) |
| TEST-01 | Every parser requirement has a fixture | unit | same filter | Partially ✅ (Phase 1 fixtures), partially ❌ (header_row negatives) |
| TEST-02 | Real Maranhão files regression | unit/integration | same filter, new test class | ❌ Wave 0 — needs `tests/fixtures/` dir + two fixture files |
| TEST-04 | Option validation throws | unit | same filter | ❌ Wave 0 — four new negatives (see Options section) |
| TEST-05 | Release-build parity | full-suite, manual trigger | `cmake -S . -B build-release ... && cmake --build build-release --config Release && ./build-release/bin/quiver_tests.exe --gtest_filter='LuaRunner*'` | N/A — a build/run step, not a new test file |

### Sampling Rate

- **Per task commit:** `./build/bin/quiver_tests.exe --gtest_filter='LuaRunner_ReadCsv.*'` (Debug,
  fast).
- **Per wave merge:** full `quiver_tests.exe` (Debug).
- **Phase gate:** the Release build/run sequence in Finding 7, before `/gsd-verify-work`.

### Wave 0 Gaps

- [ ] `tests/fixtures/ma_energia_residencial.csv` — real Energia file, byte-for-byte copy (D-24)
- [ ] `tests/fixtures/ma_gd_data.csv` — real GD file, byte-for-byte copy (D-24)
- [ ] New tests in `tests/test_lua_runner_read_csv.cpp`: `header_row` happy paths (explicit header
      row, no-header), the four TEST-04 negatives (wrong type, non-integer, negative, past-EOF),
      TEST-02's two real-file regressions, and a blank-line-under-`header_row=0` regression guard
      for Finding 2's ordering bug.
- [ ] Correction to the misleading comment on `FutureHeaderKeyIsAnUnknownOptionToday`
      (`tests/test_lua_runner_read_csv.cpp:436-444`) — the key it anticipates is `header`, not the
      actual `header_row`; the test's assertion likely still passes, but its comment should say so
      explicitly rather than implying `header_row` will make it obsolete.
- [ ] No new test framework needed — GoogleTest infrastructure is complete.

## Security Domain

Not applicable in the ASVS sense — this is a local file-parsing library feature with no network,
auth, or session surface. The one security-relevant property already enforced and unchanged by
this phase is the existing path sandbox (`resolve_sandboxed_path`, root CLAUDE.md design decision),
which Phase 1 already covers with dedicated tests (`EscapingPathThrowsForReadCsv`,
`InMemoryDatabaseThrowsForReadCsv`) — this phase adds no new file-path input surface (`header_row`
is a bounds/type-validated integer, not a path).

## Sources

### Primary (HIGH confidence — read directly this session, cited with line numbers)
- `C:/Development/Quiver/quiver3/build/_deps/csv_parser-src/include/internal/csv_format.hpp` —
  `header_row`/`no_header` declarations (lines 86-105), `VariableColumnPolicy` enum (24-30).
- `C:/Development/Quiver/quiver3/build/_deps/csv_parser-src/include/internal/csv_format.cpp` —
  `CSVFormat::header_row` implementation (lines 43-49), the `variable_column_policy` side effect.
- `C:/Development/Quiver/quiver3/build/_deps/csv_parser-src/include/internal/csv_reader.cpp` —
  `trim_header` (74-83), `accept_row`/`KEEP_NON_EMPTY` filtering (104-107), `init_parser`.
- `C:/Development/Quiver/quiver3/build/_deps/csv_parser-src/include/internal/parser/driver.cpp` —
  `resolve_format_from_head` (98-133), `get_bom_skip_or_throw` (39-73).
- `C:/Development/Quiver/quiver3/build/_deps/csv_parser-src/include/internal/parser/guessing.cpp`
  — `guess_format`/`calculate_score` (used only when header is not explicitly set — confirms our
  explicit-header path never invokes this).
- `C:/Development/Quiver/quiver3/build/_deps/sol2-src/include/sol/version.hpp` — `SOL_SAFE_GETTER`
  default logic (315-337).
- `C:/Development/Quiver/quiver3/src/csv_read.h`, `src/csv_read.cpp` — current Phase 1
  implementation, read in full.
- `C:/Development/Quiver/quiver3/src/lua_runner.cpp` — `read_csv`/`read_csv_stream` bindings
  (449-516), `read_csv_options_from_lua` (916-963), `collect_group_columns`'s `.is<int64_t>()`
  precedent (1652-1670).
- `C:/Development/Quiver/quiver3/tests/test_lua_runner_read_csv.cpp` — full existing test suite
  read, including the soon-to-be-stale `FutureHeaderKeyIsAnUnknownOptionToday` (436-444).
- `C:/Development/Quiver/quiver3/tests/test_lua_runner.h`, `tests/test_utils.h` — fixture
  mechanics (`LuaSandboxTest`, `path_from`/`VALID_SCHEMA`).
- The two real fixture CSVs and `script.lua`, read directly via `head -c`/`xxd`/`grep`/`wc -l`
  (paths in the task prompt).
- `.planning/phases/02-the-dirty-files-parse-correctly/02-CONTEXT.md`,
  `.planning/phases/01-a-lua-script-reads-a-csv-file/01-CONTEXT.md`,
  `.planning/REQUIREMENTS.md`, root `CLAUDE.md`, `src/CLAUDE.md`, `tests/CLAUDE.md`,
  `bindings/js/src/lua-api.ts`, `CMakePresets.json`, `scripts/build-all.bat`, `CHANGELOG.md`.

### Empirical (HIGH confidence — this session's own test runs)
- Findings 1-6 and Finding 7's `SOL_SAFE_GETTER` default: compiled and run via a temporary probe
  added to `tests/sandbox/sandbox.cpp` + a temporary `csv` link on `quiver_sandbox`
  (`tests/CMakeLists.txt`), both reverted; verified clean via `git status --porcelain` and a
  rebuild of the restored `quiver_sandbox` target.

### Secondary / Tertiary
None used — every claim above traces to a file read this session or a command run this session.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — no new dependency, extending an already-integrated library.
- Architecture: HIGH — extends an existing, already-reviewed C++/Lua boundary with no new
  architectural surface.
- Pitfalls: HIGH — the two most impactful pitfalls (Findings 1 and 2) were caught by empirical
  testing against the real vendored library, not inference.
- Real-file transformation logic (TEST-02): HIGH — column indices and values verified against
  the raw file bytes and cross-checked against the transcribed script's own hard-coded targets.

**Research date:** 2026-09-16
**Valid until:** No expiry driver — this is internal-library behavior pinned to the vendored
csv-parser 5.3.0 commit already fetched in this repo; re-verify only if `cmake/Dependencies.cmake`
ever bumps the csv-parser version.
