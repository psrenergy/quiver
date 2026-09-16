# Phase 2: The dirty files parse correctly - Context

**Gathered:** 2026-09-16
**Status:** Ready for planning
**Mode:** Smart discuss (autonomous) — four grey areas presented, all four recommendations accepted

<domain>
## Phase Boundary

The messy, arbitrary CSVs that arrive from real sources read correctly — including the two real
Maranhão files currently transcribed into scripts by hand.

**In scope:** the `header_row` option (LUA-05), the fixture suite proving PARSE-02..07 hold through
the Lua boundary (TEST-01), the two real-file regression tests (TEST-02), option-rejection coverage
(TEST-04), and a Release-build run (TEST-05).

**Out of scope:** everything Phase 1 locked (options-table shape, the `{header=, rows=}` return,
no row padding, string-only cells, the sandbox). DOC-02/DOC-03 — the agent-reference rewrite — are
Phase 3, not here. No CSV *writing*.
</domain>

<decisions>
## Implementation Decisions

### Verified before deciding — PARSE-02..07 already pass, with zero code

Run through the **existing Phase 1 reader** before this phase was scoped, on a fixture carrying a
UTF-8 BOM, CRLF endings, a quoted comma, a doubled quote, an embedded newline, and a ragged row:

```
input:  "\xEF\xBB\xBF" "a,b,c\r\n" "\"May 1, 2014\",33,x\r\n" "\"say \"\"hi\"\"\",2,y\r\n"
        "\"line1\nline2\",3,z\r\n" "short\r\n"

output: {"header":["a","b","c"],
         "rows":[["May 1, 2014","33","x"],
                 ["say \"hi\"","2","y"],
                 ["line1\nline2","3","z"],
                 ["short"]]}
```

- **PARSE-02** quoted separator → `"May 1, 2014"` is one field ✓
- **PARSE-03** quoted newline → `line1\nline2` is one field, record not split ✓
- **PARSE-04** doubled quote → `say "hi"` ✓
- **PARSE-05** BOM → header is `a`, not `\ufeffa` ✓
- **PARSE-06** CRLF → no cell carries a trailing `\r` ✓
- **PARSE-07** ragged → `["short"]` survives, neither padded nor dropped ✓

**Consequence: PARSE-02..07 are test-only requirements.** They need fixtures, not code. This
confirms the ROADMAP's own note ("acceptance properties of the chosen parser rather than code to
write") empirically rather than by assumption. If a planner proposes BOM-stripping or CRLF-trimming
code, it is redundant — re-run the probe above before adding any.

The three csv-parser settings that make this work are already pinned in `make_format`
(`src/csv_read.cpp`) by Phase 1's D-12: `variable_columns(KEEP_NON_EMPTY)`, `header_row(0)`, and
never calling `guess_csv()`. Phase 2 changes the third of those only through the new option below.

### D-20: `header_row` is 1-based; `0` means "no header"

`db:read_csv(path, { header_row = 2 })` names the file's **second line** as the header. Default is
`1`. `header_row = 0` declares the file has no header at all: `csv.header` is **absent (nil)** and
`rows[1]` is the first line of the file.

- 1-based matches every other index in this API (`rows[1]`, `header[1]`) and Lua itself. The
  binding subtracts 1 before calling csv-parser's 0-based `format.header_row()`.
- `0` for "none" keeps it to **one key of one type** — the decoder stays a number check, no mixed
  number-or-boolean shape. Phase 1's D-01 already pinned `header` as *absent, never `{}`*, when
  there is no header, so the read side of this is already specified.
- Phase 1's D-01 already wrote `header_row=2` for the Maranhão file, i.e. 1-based, so this
  confirms an existing assumption rather than introducing one.
- **Reversibility:** one-way. The key name and its base are published to every agent script the
  moment `lua-api.ts` documents them.

Rejected: `header_row = false` for none (two accepted types for one key, and `0` becomes an error
value that has to be explained); 0-based to match the C++ library (contradicts `rows[1]` in the
same returned table — the translation belongs in the binding, which is where the C++/Lua impedance
mismatch already lives).

### D-21: LUA-06 needs no code — it is already satisfied by Phase 1's D-01

"No column unreachable, none silently shadowing another" is a property of the **positional** shape
D-01 locked: `header` is a raw 1..n array with duplicates, blanks and surrounding spaces preserved
verbatim, and `rows` are positional arrays. There is no name→value map anywhere, so there is
nothing for a duplicate name to shadow, and a blank name is just an empty string at its index.

The real header row — `ANO,Residencial,,ANO,MÊS, Residencial ,,,,,` — has `ANO` twice,
`Residencial` twice (once space-padded as ` Residencial `), and six blank names. All twelve columns
are reachable as `row[1]`..`row[12]` regardless.

**Do not add a name→index map, a de-duplicating renamer, or a `columns` lookup table.** Any of
those would *create* the shadowing problem LUA-06 exists to prevent. LUA-06 is discharged by tests.

### D-22: The units row is the script's problem, not the reader's

The real file's shape is **not** what the ROADMAP describes. Actual layout of
`Energia Consumida Residencial do Maranhão.csv`:

| Line | Content | Role |
|------|---------|------|
| 1 | `SÉRIE ANUAL,,,SÉRIE MENSAL,,,,,,,` | block-title junk, **above** the header |
| 2 | `ANO,Residencial,,ANO,MÊS, Residencial ,,,,,` | the real header |
| 3 | `,MWh,,,, MWh ,,,,,` | units row, **below** the header |
| 4+ | `2005,1'114'144,,2005,01/01/2005, 93'943 ,,,,,` | data |

So there is **one** junk row above the header and **one** below it — not "two junk rows above the
header" as the ROADMAP's success criterion 3 says. With `header_row = 2`, the units row becomes
`rows[1]`, and the script skips it (`for i = 2, #csv.rows do`).

No `skip_rows` option. PROJECT.md's constraint governs: every documented option costs prompt tokens
forever, and no requirement asks for this one. A one-line loop offset in the script is cheaper than
a permanently documented knob.

**Planner note:** criterion 3's wording ("two junk rows above the header") describes a file that
does not exist. Satisfy the *intent* — a file with junk around the header reads with the right
column names — and cover the real shape above. A hand-written fixture may still carry two rows
above the header to test that case directly.

### D-23: TEST-02 transforms in Lua and asserts the final values

The transcribed script's hard-coded values are **post-transformation**: `93943` comes from the cell
` 93'943 ` (space-padded, apostrophe thousands separators), and `2014-05` from `"May 1, 2014"`.
The test performs that transformation in the Lua script and asserts the final values, because the
phase's point is that the file can *replace* the transcription — not merely that the reader is
faithful to bytes.

Targets from `script.lua` (the file being replaced):

- `obs_str` — `2005-01 93943`, `2023-07 386433` (from the Energia file, DD/MM/YYYY → YYYY-MM)
- `gd_str` — `2014-05 33`, `2021-07 51818.33` (from the GD file, English month name → YYYY-MM)

This also makes DOC-03's `tonumber`/`gsub` parenthesis trap a covered case rather than prose —
`gsub` returns *two* values, so `tonumber(v:gsub("'",""))` passes the replacement count as
`tonumber`'s base argument and silently returns nil. The correct form needs the extra parens:

```lua
local n = tonumber((v:gsub("'", "")))
```

### D-24: Fixtures are renamed to ASCII, content byte-identical

`tests/fixtures/ma_energia_residencial.csv` and `tests/fixtures/ma_gd_data.csv`. The source
filename carries a non-ASCII `ã` and spaces; the *content* is what the test is about, and a
non-ASCII path is a cross-platform liability in git, CMake and CI for a property no requirement
asks for.

**Content must be copied byte-for-byte** — the BOM and the CRLF endings *are* the test (PARSE-05,
PARSE-06). Copy in binary; do not let an editor or a unix tool normalize line endings. Note
`.gitattributes` enforces LF for `.cpp/.h/.dart/.jl/.py` but not `.csv`; verify the committed bytes
still contain `\r\n` and the leading `EF BB BF` after checkout.

Source (outside the repo, not a build dependency — copied once):
`C:/Development/Claw/claw-experiments/Foresight/.claw/case-ma-2.foresight/a20a2fd08893/runs/run-007/`
</decisions>

<code_context>
## Existing Code Insights

- **`src/csv_read.cpp`** — `make_format` is the one place csv-parser settings are pinned. The new
  `header_row` option flows through `Options` (in `src/csv_read.h`) into `format.header_row(n - 1)`,
  with `header_row = 0` calling `format.no_header()` instead. Phase 1's D-12 comment in that
  function explains why each existing setting is pinned; extend it, do not replace it.
- **`src/lua_runner.cpp`** — `read_csv_options_from_lua` is the shared strict decoder both entry
  points already use (Phase 1, D-14/D-15). Add `header_row` there so both forms cannot diverge
  (LUA-03). It takes `sol::object` and hand-checks types; a wrong type must throw, never fall back.
- **`tests/test_lua_runner_read_csv.cpp`** — the existing suite (49 tests) has the fixture helpers
  `write_lua_csv_file`, `lp`, `expect_prefixed_error`, and the `LuaSandboxTest` fixture. Its
  rejection-matrix section is the pattern for TEST-04's new option cases.
- **`tests/CMakeLists.txt`** — new test files are registered explicitly (no glob). `tests/fixtures/`
  does not exist yet; committed fixtures currently live only under `tests/schemas/`.
- **`bindings/js/src/lua-api.ts`** — `bindings/js/test/lua-api-sync.test.ts` parses
  `src/lua_runner.cpp` and **fails the build** until every bound name and the documented option set
  match. Adding `header_row` without documenting it breaks the JS suite.
</code_context>

<specifics>
## Specific Ideas

- **TEST-05 (Release build) is not optional and is easy to skip by accident.** `SOL_SAFE_GETTER` is
  ON in Debug and OFF in Release, and that difference has already hidden Lua marshalling bugs in
  this repo (see `tests/CLAUDE.md` on the three mixed-array tests). The new `header_row` decoder is
  exactly the kind of `sol::object` type-check that degrades silently in Release. Build Release and
  run `--gtest_filter='LuaRunner*'` before calling the phase done.
- **LUA-08 already names the error case this phase introduces**: "a header row past the end of the
  file" must raise `Cannot read_csv: ...`. That is a new negative for `header_row`, alongside a
  negative value, a non-integer, and a wrong type — all four asserting a *throw*, per TEST-04.
- The Energia file's numbers use apostrophe thousands separators and surrounding spaces
  (` 93'943 `); dates are DD/MM/YYYY. The GD file uses English month names (`"May 1, 2014"`) and
  has decimals (`51.38`). All arrive as strings — Phase 1's LUA-07 guarantees no inference.
</specifics>

<deferred>
## Deferred Ideas

- **`skip_rows` / skip-after-header option** — rejected in D-22. Revisit only if a second real file
  needs it; one file's units row is not evidence of a pattern.
- **A name→index header map** — rejected in D-21; it would reintroduce the shadowing LUA-06 forbids.
- **DOC-02 / DOC-03** (the agent-reference rewrite telling the model to read rather than transcribe,
  and the worked dirty-file example) — Phase 3. D-23's `tonumber`/`gsub` parenthesis trap is proven
  by a test here, then written up there.
- **CSV writing** — v2, per REQUIREMENTS.md.
</deferred>
