# Phase 1: A Lua script reads a CSV file - Context

**Gathered:** 2026-09-15
**Status:** Ready for planning

<domain>
## Phase Boundary

`vincentlaucsb/csv-parser` wired into the C++ core, with two Lua entry points on top of it —
`db:read_csv(path, opts)` (whole file) and `db:read_csv_stream(path, on_row, opts)` (row-by-row,
bounded memory) — every cell arriving as a string, every path resolved against the database
directory, plus the `lua-api.ts` entries that keep the build green.

Covers PARSE-01, PARSE-08, PARSE-09, LUA-01, LUA-02, LUA-03, LUA-04, LUA-07, LUA-08, TEST-03, DOC-01.

**Not this phase:** dirty-input handling (BOM, CRLF, quoted separators/newlines, junk header rows,
duplicate/blank header names) is Phase 2. The shipped agent-reference rewrite is Phase 3.

</domain>

<decisions>
## Implementation Decisions

Every decision below was produced by adversarial research (27 subagents across two workflows:
7 research angles, 4 syntheses, 12 refutation lenses). Verdicts are recorded per decision.
Where a refutation lens **empirically verified** a claim by running `quiver_cli`, that is noted —
those are not inferences.

### Lua surface — `db:read_csv`

- **D-01:** `db:read_csv(path, opts)` returns **one table**: `{ header = {...}, rows = {{...}, ...} }`.
  `header` is a raw 1..n array of column names with duplicates, blanks and surrounding spaces
  preserved verbatim. `rows` is a 1..n array of rows, each a 1..n array of strings, addressed
  positionally (LUA-01). The `header` key is **absent (nil), never `{}`**, when the file has none.
  — **Reversibility:** one-way — the shape is the host contract: `LuaRunner::run` JSON-encodes it
  and every claw script keys off it. Changing it breaks every shipped script.

  *Why (0/3 lenses refuted):* `LuaRunner::run` encodes only `result.get<sol::object>(0)`
  (`src/lua_runner.cpp:1806`), and `db:transaction` / `db:dry_run` collapse the callback the same
  way (`:302`, `:327`). Under multi-return, `return db:read_csv(p)` — the most natural line an
  agent writes — emits well-formed JSON of the rows and **silently drops the header at three
  sites**, with no diagnostic. House precedent is exact: `get_vector_metadata_lua:1191` returns
  `{group_name=..., value_columns={...}}`, the same descriptor+parallel-array shape;
  `read_element_by_id_lua:1390` faced this same fork with three named parts and **merged rather
  than multi-returned**. There is zero `sol::as_returns` / `variadic_results` / `std::tuple` in
  1813 lines, and zero `local a, b = db:` anywhere in the repo.

  *Rejected:* rows-only-with-header-at-row-1. `append_json_table:145` is array-iff-keys-are-1..n,
  so ever hanging a scalar key on the rows array flips `return rows` from a JSON array to a
  key-sorted object — a silent host-contract break for every existing script. It also cannot
  express Phase 2's "no header" (LUA-05), and on the real Maranhão file with `header_row=2` it must
  either break its own contract or delete row 1 (the `SÉRIE ANUAL,,,SÉRIE MENSAL` block-title row).

  *Known residual, accept and document:* `ipairs(db:read_csv(p))` gives zero iterations silently —
  the reference's own line 105 ("iterate with `ipairs`") primes exactly that. This is one
  *conditional* silent mode; the rejected shapes carry two and one *guaranteed* one respectively.

- **D-02:** Rows are **never padded** to header width. A short row stays short; `row[j]` is nil past
  its end. Pin `format.variable_columns(...)` explicitly — see D-12.
  — **Reversibility:** one-way — Phase 2's PARSE-07 forbids silent truncation, so shipping padded
  rows in Phase 1 forces a behaviour change on shipped scripts.

- **D-03:** An empty file **throws**: `Cannot read_csv: file 'x.csv' is empty` (Pattern 1).
  Keeps `if csv.header then` meaning exactly one thing — "the caller declared no header" — rather
  than doubling as "the file was empty". The check lives in the **shared** parser helper, not in
  either caller, or `db:read_csv_stream("empty.csv", fn)` returns 0 while `db:read_csv` throws,
  which is divergence on an input and violates LUA-03.

- **D-04:** Every cell reaches Lua as a **string**. No numeric or date inference, ever (LUA-07).
  Copy each cell with `field.get<std::string_view>()` into a `std::string` inside the loop and let
  the `CSVRow` die each iteration — a retained row pins its whole 10 MB mapping via `shared_ptr<void>`.

### Lua surface — `db:read_csv_stream`

- **D-05:** The callback receives **`on_row(row, index, header)`**. `row` is the same element shape
  `read_csv` puts in `rows` (1-based strings, never padded). `index` is the 1-based ordinal of the
  **data** row — the header row is not counted. `header` is the same raw-names table `read_csv`
  returns under `header`. Lua discards extra arguments, so `function(row)` and `function(row, i)`
  both work unchanged; the arity is opt-in.
  — **Reversibility:** one-way — argument order is a published contract.

  *Why:* `db` is deliberately **not** passed first. `fn(std::ref(self))` in `db:transaction` /
  `db:dry_run` (`src/lua_runner.cpp:290`, `:315`) is transaction-scoping ceremony — the callback
  gets the same object the global `db` already names — not a house rule; prepending it would push
  the actual data to position 2 for nothing. `header` **must** be reachable *during* the stream:
  you need it to find which column is `price` before processing row 1, and LUA-03 forbids the two
  forms diverging in what they expose. It is built once before the loop and pushed by reference
  per row — one `lua_pushvalue`, no per-row allocation.

  *This is the single highest-value decision in the area.* A refutation lens demonstrated the
  mis-generation it prevents: with the header unreachable, a model writes the universal streaming
  idiom `if not header then header = row; return end`, which **silently eats the first data record**
  and makes every column name a cell value. `create_element` then throws
  `Scalar attribute not found: 'ACME'` — a Pattern 2 error naming a cell value, pointing at neither
  `read_csv_stream` nor the header. On a numeric-first-column file it does not even throw; it
  writes N-1 rows of plausible data.

- **D-06:** The callback **can stop early: returning `false` stops the read.** Any other return —
  including no return at all, and `nil` — continues.
  — **Reversibility:** one-way — once `return false` is load-bearing it cannot be un-defined.

  *Why (user decision; two lenses disagreed):* without it, "print the first 5 rows of huge.csv"
  yields a script that prints exactly the right 5 lines **and streams all 4 GB** — correct output,
  zero errors, the bounded-read guarantee silently gone, and invisible on a test fixture, so it
  ships and only bites at the scale the feature exists for.

  *Mandatory implementation detail:* use `sol::optional<bool>`, which is a strict `LUA_TBOOLEAN`
  check that short-circuits past `SOL_SAFE_GETTER` (`stack_core.hpp:1139`) and is therefore
  Debug/Release-identical:
  `if (r.return_count() > 0 && r.get<sol::optional<bool>>(0) == false) break;`
  **Do not** use `get<bool>` — that is `lua_toboolean` truthiness and would read a no-return
  callback's `nil` as "stop". "Returning nothing must mean continue" is non-negotiable: a callback
  whose last statement is an `if` that did not fire returns nothing.

  *Accepted hazard, must be documented:* a callback ending `return row[1] ~= ""` returns a genuine
  Lua `false` and truncates the stream. Document it in `lua-api.ts` in the DOC-03 house style.

- **D-07:** `db:read_csv_stream` returns the number of data rows **fed to the callback**, as
  `int64_t` (not `size_t` — house counts are `int64_t`, cf. `database.h:151` `number_of_elements`).
  Under D-06 this is a partial count when the callback stopped early. Document that it counts rows
  *read*, not rows the callback *kept* — a filtering callback that logs it as "imported N" is wrong.

- **D-08:** A Lua error raised inside the callback **propagates verbatim and unwrapped**, and the
  file is closed. Take `sol::protected_function` — **never** `sol::function` — check
  `result.valid()` after every call, and on failure use the `db:transaction` idiom verbatim
  (`src/lua_runner.cpp:294-299`): `sol::error err = result; throw std::runtime_error(err.what());`.
  No manual cleanup: the `csv::CSVReader` is a stack local and `~CSVReader()` (`csv_reader.hpp:213`)
  joins its scheduler during normal C++ unwinding, and sol2's trampoline catches the exception
  *before* calling `lua_error` (`trampoline.hpp:103-128`), so unwinding completes first.
  — **Reversibility:** reversible in shape, but `sol::function` here is a defect, not a trade-off.

### Core placement and build

- **D-09:** The reader is **internal**: new `src/csv_read.h` + `src/csv_read.cpp`. Not
  `include/quiver/`, no `QUIVER_API`, no C API, not bound in Julia/Dart/Python/JS.
  — **Reversibility:** costly — promoting it later means a public header, a C API surface, and
  four FFI bindings.

  *Why (placement survived 3/3):* root CLAUDE.md's "all public C++ methods should be bound to C
  API, then to every binding" rule attaches to *public* headers. REQUIREMENTS.md explicitly puts
  Julia/Dart/Python/JS exposure out of scope ("those hosts all have native CSV libraries; Lua needs
  this precisely because `io` is deliberately absent"). Keeping the reader internal means the rule
  never fires — no documented exception needed. Public headers today contain zero third-party
  includes, and `cmake/Platform.cmake:39-41` sets hidden visibility.

  *Caveat to plan around:* every internal helper in `src/` today is header-only inline
  (`utils/string.h`, `database_internal.h`, `binary/binary_utils.h`); all 38 `QUIVER_SOURCES`
  entries implement a public header. `csv_read.cpp` is the first `.cpp` with no public header.
  Defensible on rebuild-time grounds, but it is a **new pattern** — note it in `src/CLAUDE.md`.

- **D-10:** csv-parser's headers are included **only** in `src/csv_read.cpp`, never in
  `lua_runner.cpp`. Include `<internal/csv_reader.hpp>`, not `<csv.hpp>` — the latter also pulls
  the DataFrame and the writer. `lua_runner.cpp` already needs `/bigobj` on MSVC for sol2's
  template depth; PROJECT.md calls a header-only parser landing in that TU "a concrete build-size
  risk, not a theoretical one".

- **D-11:** CMake (`cmake/Dependencies.cmake`, after rapidcsv), all four FORCEd before
  `FetchContent_MakeAvailable`:
  `CSV_ENABLE_THREADS=OFF`, `CSV_NO_SIMD=ON`, `CSV_BUILD_PROGRAMS=OFF` (defaults ON),
  `CSV_BUILD_TESTS=OFF`. Link `csv` **PRIVATE** in `src/CMakeLists.txt`, next to rapidcsv.
  Add `set_target_properties(csv_no_simd PROPERTIES EXCLUDE_FROM_ALL YES)` — it is a full duplicate
  of csv's 9 sources that nothing links (~8.5 s per clean build). csv-parser declares no
  `install()`/`export()` rules, so `EXCLUDE_FROM_ALL` is safe here — **do not** delete that line by
  analogy with the lua-cmake note directly above it in the same file.
  — **Reversibility:** one-way for `CSV_NO_SIMD` — see below.

  **`CSV_NO_SIMD=ON` is not a build-time nicety.** With SIMD on, csv-parser adds `/arch:AVX2` as a
  **PUBLIC** compile option, which propagates into `quiver` itself and would SIGILL on pre-AVX2 x86
  for every shipped PyPI wheel, npm native, Julia artifact and S3 binary. Note this in the CMake
  comment so nobody "optimizes" it back on.

  *Single-include is not an option:* `single_include/csv.hpp` is an `#error` stub at 5.3.0.
  FetchContent via its CMake is the only path.

### Parser configuration — three library defaults are wrong for us

- **D-12:** Build the `CSVFormat` in **exactly one place** (the shared helper), and pin all three:
  - `format.delimiter(<separator>)` — from the options table, defaults to `,` (PARSE-08).
  - `format.variable_columns(csv::VariableColumnPolicy::KEEP_NON_EMPTY)` — the library default is
    `IGNORE_ROW` (`csv_format.hpp:279`), and `accept_row` (`csv_reader.cpp:104-122`) **silently
    discards** every row whose field count differs from the header under it. That directly violates
    D-02. `KEEP_NON_EMPTY` over plain `KEEP` is the user's call (D-13): ragged rows survive
    identically, but a blank line does not fire the callback or increment the count.
  - `format.header_row(0)` — `CSVFormat` only sets `header_explicitly_set_` via
    `header_row()`/`no_header()` (`csv_format.hpp:249-252`). With only `.delimiter()` set,
    `resolve_format_from_head` (`parser/driver.cpp:98-111`) computes `infer_header = true` and runs
    `guess_header_row` (`parser/guessing.cpp:48-66`), and `trim_header` (`csv_reader.cpp:73-86`)
    then pops every record up to and including the guessed index. On a file with a one-cell title
    row above a 12-column header — i.e. the Maranhão files this milestone exists for — that
    **silently eats the title row** and yields a header the script never asked for, data-dependently.
  — **Reversibility:** one-way — all three change observable output; Phase 2 would be a regression.

  Also do **not** call `CSVFormat::guess_csv()` (would reinterpret a comma file as `;`/`|`/tab/`^`).
  `trim_chars` is empty and `_eager_field_classification` is false by default, so header spacing is
  preserved verbatim and no type inference runs — both are what D-01/D-04 need; leave them alone.

- **D-13:** A blank line mid-file is **not** a row (`KEEP_NON_EMPTY`). Ragged rows still survive
  (PARSE-07 is satisfied either way), but a blank line does not fire the callback or increment
  `index`. Otherwise the returned count is not the data-row count D-07 advertises, and every
  trailing newline in a hand-edited file becomes a phantom empty row every script must guard.

### Options table

- **D-14:** One trailing, optional options table on **both** entry points — `db:read_csv(path, opts)`
  and `db:read_csv_stream(path, on_row, opts)` — decoded by **one shared decoder** (LUA-03).
  **Exactly one key in Phase 1: `separator`**, a single-character string, defaulting to `","`.
  PROJECT.md's prompt-weight constraint applies to everything beyond it: "every documented option
  costs tokens forever, so the API surface has to earn each knob."
  — **Reversibility:** costly — the key name is published; Phase 2 adds header keys to this table.

- **D-15:** **Take the options parameter as `sol::object`, not `sol::optional<sol::table>`**, and
  hand-check the type. This is the house precedent at `src/lua_runner.cpp:1006`
  (`relation_target_from_lua`), whose own comment documents the trap.
  — **Reversibility:** reversible, but the alternative is a live silent-corruption path.

  **Empirically verified by a refutation lens running `quiver_cli` against the real build**, not
  inferred: `db:export_csv("Configuration","","out.csv","NOT_A_TABLE")` → succeeds, file written,
  defaults used, no error. Same for a number and a boolean. Root cause, all checkable:
  `src/CMakeLists.txt:60-61` defines `SOL_SAFE_FUNCTION=1`, but **sol2 has no macro by that name** —
  it reads `SOL_SAFE_FUNCTION_CALLS` / `SOL_SAFE_FUNCTION_OBJECTS`
  (`build/_deps/sol2-src/include/sol/version.hpp:372-394`), so the define is a **no-op**; the
  arg-check gate is `detail::default_safe_function_calls` (`stack.hpp:186-197`), DEFAULT_OFF in
  Release; and sol2's optional checker forwards to `unqualified_check<ValueType>(..., &no_panic, ...)`
  (`stack_check_unqualified.hpp:361-374`), so it never raises on its own.

  Consequence if not fixed: `db:read_csv("f.csv", ";")` — the likeliest user mistake, and literally
  LUA-08's "a bad option value" — silently parses with `,`, so `a;b;c` becomes **one** column,
  `row[1]` is the whole raw line, `row[2]` is nil, and `tonumber(nil)` returns nil without raising.
  Every row inserts, nothing throws, the database fills with garbage. That is exactly the failure
  mode TEST-04 exists to rule out and the Debug/Release divergence TEST-05 exists to catch.

  The mis-generation is *primed by the neighbours*: every adjacent db file-method in the shipped
  reference takes a bare scalar in slot 2 — `db:open_file(path, "w", md)` (`lua-api.ts:657`),
  `db:bin_to_csv(path, false)` (`:666`).

- **D-16:** An **unknown option key throws**: `Cannot read_csv: unknown option 'delim'`. This
  deliberately diverges from `parse_csv_options` (`src/lua_runner.cpp:807`), which silently ignores
  unknown keys. A typo'd key otherwise silently does nothing — the exact failure mode TEST-04 rules
  out — and it gets worse in Phase 2 when LUA-05/LUA-06 add real keys worth typo'ing.

- **D-17:** Validate the value **strictly and by hand**, not via `lua_cell_as`. A lens verified that
  `lua_cell_as<std::string>` on a Lua number raises a **raw sol2 message**
  (`stack index -1, expected string, received number`), which is neither Pattern 1 nor stable across
  build types — LUA-08 forbids surfacing a parser/stream error. Check `get_type()` explicitly.
  Collect the entries first and validate after: throwing out of sol2's `for_each` abandons the
  traversal mid-stack.

- **D-18:** The memory window is a **fixed internal constant with no caller-visible knob** — write
  no `format.chunk_size(...)` call at all. csv-parser's `CSV_CHUNK_SIZE_DEFAULT` is 10 MB
  (`common.hpp:410`), the floor is 512,000 bytes (`:424`), and with `CSV_ENABLE_THREADS=0`
  `read_window_size` (`orchestrator.hpp:78-90`) returns `chunk_size` **unmultiplied** by the worker
  count — which is precisely what PARSE-09 ("not tied to the host's CPU count") requires. Every
  legal value performs the same. If `CSV_ENABLE_THREADS` is ever turned back on, add
  `format.threading(false)` in the shared helper.

### Errors

- **D-19:** Each entry point **names itself** in its own errors: `db:read_csv_stream` reports
  `Cannot read_csv_stream: ...`, not `Cannot read_csv: ...`.
  — **Reversibility:** reversible — message text only.

  *Why (user decision):* root CLAUDE.md Pattern 1 says `{operation}` is "the public method name the
  user called", and `resolve_sandboxed_path`'s own comment repeats it. Decisive: the sandbox helper
  **will** say `Cannot read_csv_stream` for a path escape regardless of what the parser says, so
  taking LUA-08's wording literally would make the two halves of the same call disagree with each
  other. LUA-08 and Phase 1 success criterion 4 write `Cannot read_csv: ...` because they treat
  `read_csv` as the name of the *feature*; read it that way.

- **D-20:** Thread the operation name through the shared helper (the way `resolve_sandboxed_path`
  already does) and **never let a csv-parser error reach Lua** (LUA-08). Wrap the `CSVReader`
  construction in try/catch. Note `resolve_sandboxed_path` uses `fs::weakly_canonical`, which does
  **not** require the path to exist — so a missing file passes the sandbox gate untouched and must
  be caught separately.

- **D-21:** Error messages quote the **script's own spelling** of the path, not the resolved
  absolute path, so the locked text `file 'x.csv' is empty` holds and tests can pin full message
  strings. `resolve_sandboxed_path` returns the canonicalized absolute path, so the caller must
  pass both.

- **D-22:** Phase-1 error catalogue, in evaluation order. `<op>` is `read_csv` or `read_csv_stream`
  per D-19. Spell the not-found / not-a-directory messages the way the only sibling sandboxed file
  op already does (`db:validate_migrations` → `src/database.cpp:252`, `:263-266`,
  `src/migrations.cpp:13`) rather than inventing a new phrasing:
  1. `Cannot <op>: database is in-memory, file operations are unavailable`  *(existing helper)*
  2. `Cannot <op>: path '<p>' escapes the database directory '<root>'`  *(existing helper)*
  3. `Cannot <op>: options must be a table`
  4. `Cannot <op>: unknown option '<key>'`
  5. `Cannot <op>: option 'separator' must be a string`
  6. `Cannot <op>: option 'separator' must be a single character`
  7. `Cannot <op>: file not found: <p>`
  8. `Cannot <op>: path is a directory: <p>`
  9. `Cannot <op>: file '<p>' is empty`
  10. `Cannot <op>: cannot read file '<p>': <reason>`  *(the csv-parser wrapper)*

### Claude's Discretion

- Exact internal signature of the shared helper (free function vs `LuaRunner::Impl` static,
  template vs `std::function`), provided both entry points route through **one** loop and **one**
  `CSVFormat` construction (LUA-03).
- Whether the early-stop `bool` rides the internal callback's return type or a separate predicate.
- Test file naming and split, within the constraints in Integration Points below.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Requirements and scope
- `.planning/REQUIREMENTS.md` — PARSE-01/08/09, LUA-01/02/03/04/07/08, TEST-03, DOC-01 are this
  phase. LUA-05/LUA-06 and PARSE-02..07 are Phase 2 and must not be pre-empted.
- `.planning/ROADMAP.md` §"Phase 1" — the five success criteria, and the three Notes (DOC-01
  build-gate, LUA-03 structural, sandbox reuse).
- `.planning/PROJECT.md` §Constraints — the licensing, prompt-weight and `/bigobj` constraints that
  drove D-10, D-11, D-14, D-18.

### Code the phase touches
- `src/lua_runner.cpp` — `resolve_sandboxed_path` (:772, reuse as-is), `parse_csv_options` (:807,
  the decoder **not** to copy — see D-15/D-16), `relation_target_from_lua` (:1006, the decoder
  **to** copy), `to_lua_table` (:831, the only vector→table marshaler), the `db:transaction`
  protected_function idiom (:294-299), the sandboxed file-op block (:424-446, where the new
  bindings go), `append_json_table` (:145) and `run` (:1806) for the host contract.
- `bindings/js/src/lua-api.ts` — `LUA_DB_API_REFERENCE`. Both new names must appear as literal
  tokens or the build fails. Sandbox bullet at :97-99 needs both added.
- `bindings/js/test/lua-api-sync.test.ts` — the build gate. Its regex
  `\b(bind|ns)\.set_function\(\s*"…"` catches both new names. It checks **names only**, not
  return shapes or options — so a Lua test is the only guard on those.
- `cmake/Dependencies.cmake` — FetchContent idiom; the lua-cmake `EXCLUDE_FROM_ALL` note directly
  above the new block, which does **not** apply to csv-parser (D-11).
- `src/CMakeLists.txt` — `QUIVER_SOURCES` list, PRIVATE link block, and the `SOL_` defines at
  :60-61 (one of which is dead — see D-15).
- `tests/CMakeLists.txt` — explicit source list, no glob. New test files must be registered.
- `tests/test_lua_runner.h` — the `LuaSandboxTest` fixture: members are `sandbox` and `db_path()`;
  existing CSV suites subclass as e.g. `class LuaRunner_ImportCSV : public LuaSandboxTest`, use a
  file-local `write_lua_csv_file`, a runner named `lua`, and an explicit `Database::from_schema`.

### House rules that constrain the shape
- `CLAUDE.md` §"C++ Error Message Patterns" — Pattern 1 grammar (D-19, D-22).
- `CLAUDE.md` §"Design Decisions" — the Lua sandbox policy and the per-binding omission list that
  D-09 adds to.
- `src/CLAUDE.md` — File Map (needs a `csv_read.h/.cpp` row) and the LuaRunner sandboxed-operation
  list at :310-312 (needs both new names). Also states `to_lua_table<T>` is **the only**
  vector→table marshaler — do not write a second one.
- `tests/CLAUDE.md` — fixture conventions and suite registration.

### Upstream library facts (verified against csv-parser 5.3.0 source)
- `csv_format.hpp:279` — `variable_columns` defaults to `IGNORE_ROW`.
- `csv_reader.cpp:104-122` — `accept_row` silently discards mismatched rows under `IGNORE_ROW`.
- `csv_format.hpp:249-252` / `parser/driver.cpp:98-111` / `parser/guessing.cpp:48-66` /
  `csv_reader.cpp:73-86` — the header-guessing chain that D-12 disables.
- `common.hpp:410`/`:424` — `CSV_CHUNK_SIZE_DEFAULT` 10 MB, floor 512,000 bytes.
- `orchestrator.hpp:78-90` — `read_window_size` unmultiplied under `CSV_ENABLE_THREADS=0` (PARSE-09).
- `csv_reader.hpp:213-215` — `~CSVReader()` joins its scheduler (D-08 RAII).

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- **`resolve_sandboxed_path`** (`src/lua_runner.cpp:772`) — reuse verbatim for LUA-04. Handles the
  in-memory rejection and strict containment already. **Does not** check existence
  (`fs::weakly_canonical`), so D-20's separate not-found check is required.
- **`to_lua_table`** (`src/lua_runner.cpp:831`) — the row marshaler. 1-based, `create_table`,
  identical to anything a new helper would write. `src/CLAUDE.md` names it the only one; do not
  duplicate it.
- **`relation_target_from_lua`** (`src/lua_runner.cpp:1006-1016`) — the correct strict-decoding
  pattern for D-15/D-17: `sol::object` plus an explicit `get_type()` check, with a comment already
  documenting why `sol::optional` is wrong.
- **`db:transaction` error handling** (`src/lua_runner.cpp:294-299`) — the exact three lines D-08
  needs.
- **`LuaSandboxTest`** (`tests/test_lua_runner.h`) — the fixture for all TEST-03 negatives.

### Established Patterns
- Sandboxed file ops are registered as plain `bind.set_function` in the `:424-446` block. That
  region carries **no** `NOLINT` pairs — the three `NOLINTBEGIN(performance-unnecessary-value-parameter)`
  pairs in the file wrap only the `new_usertype`/namespace blocks (:260-356, :456-531, :541-605).
  `open_file` (:432) takes `sol::optional<BinaryMetadata>` by value with no NOLINT. Do not invent a
  fourth pair.
- Options tables are always the **last** parameter and always optional.
- Counts are `int64_t` (`database.h:151`), not `size_t`.
- Public headers contain zero third-party includes; hidden visibility is set at
  `cmake/Platform.cmake:39-41`.

### Integration Points
- New bindings land in the sandboxed file-op block after `csv_to_bin` (`src/lua_runner.cpp:446`).
- `src/csv_read.cpp` joins `QUIVER_SOURCES` after `database_csv_import.cpp`; `csv` joins the PRIVATE
  link list after `rapidcsv`.
- **`quiver_tests` cannot reach `src/`** — `tests/CMakeLists.txt` adds no such include dir, so the
  reader is tested through the Lua surface, which is the existing convention anyway. If Phase 2's
  PARSE-02..07 matrix wants direct gtest coverage, the escape hatch is two lines
  (`target_sources` + `target_include_directories` on `quiver_tests`) and still needs no public
  header. (Note `quiver_c` *can* reach `src/` — `src/CMakeLists.txt:145-147` — but that is
  irrelevant here since there is no C API surface.)
- **DOC-01 must land in the same commit as the binding**, not after:
  `bindings/js/test/lua-api-sync.test.ts` fails the JS suite the moment a `db:` name exists in
  `lua_runner.cpp` without a literal token in `lua-api.ts`.
- Paperwork that is easy to miss and is required by house rules: `src/CLAUDE.md` File Map + the
  sandboxed-operation list, a root `CLAUDE.md` line recording that `read_csv` is Lua-only with no
  C++/C API counterpart (this is the first `db:` method with no counterpart anywhere — the house
  documents every per-binding asymmetry), and a `CHANGELOG.md` entry under the unreleased heading.

</code_context>

<specifics>
## Specific Ideas

- **The doc entry must carry the traps, not just the signature.** DOC-03 already establishes that
  this project treats silent-failure traps as first-class reference content (the `tonumber`/`gsub`
  parenthesis trap). The Phase-1 entry should carry, in the same house style: that `index 1` is the
  first **data** row so `if index == 1 then return end` silently drops a record; that the returned
  count is rows *read*, not rows *kept*; that `return false` stops (and therefore that a callback
  ending in a comparison will truncate); and the options table shown as a **literal**
  (`{ separator = "," }`), not a placeholder `opts` — the placeholder is what invites the positional
  `db:read_csv("f.csv", ";")` mis-generation D-15 describes.

- **Two fixtures that fail loudly against the library defaults**, beyond the TEST-03 sandbox
  negatives — these are the cheapest guards on D-12 and they belong in Phase 1, not Phase 2,
  because they pin behaviour Phase 1 ships:
  - `ragged.csv` — 3-column header, one 2-column row, one 4-column row. Assert all three rows
    arrive and `#rows[1] == 2`. Fails under `IGNORE_ROW`.
  - `preamble.csv` — a one-cell title line above the real header. Assert nothing was eaten. Fails
    under a guessed `header_row`.

- **One round-trip assertion pins the whole host contract cheaply:** `EXPECT_EQ` on the JSON string
  from `run()` for a small file (`{"header":[...],"rows":[[...]]}`). It pins the key names, the
  nil-not-`{}` rule and the encoder's array-vs-object branch in a single assertion — and it is
  exactly what the rejected return shapes break. The encoder sorts object keys and emits `[]` for
  an empty `rows`, so the round trip is clean.

- **One test catches a `sol::function` regression:** after a callback raises mid-stream, assert
  `std::filesystem::remove` on the CSV **succeeds**. On Windows an open mmap/ifstream blocks it, so
  this is the check that fails if someone swaps `sol::protected_function` for `sol::function` and
  the Release longjmp skips `~CSVReader`.

</specifics>

<deferred>
## Deferred Ideas

- **`SOL_SAFE_FUNCTION=1` at `src/CMakeLists.txt:60-61` is a dead define** — sol2 3.5.0 reads
  `SOL_SAFE_FUNCTION_CALLS` / `SOL_SAFE_FUNCTION_OBJECTS`. Every existing `sol::optional<sol::table>`
  options parameter in the Lua surface therefore silently accepts a non-table in both Debug and
  Release: verified live on `db:export_csv` and `db:query_string`. Phase 1 works around it locally
  (D-15) rather than fixing it globally, because the fix changes behaviour on five existing sites
  (`:338`, `:350`, `:1252`, `:1263`, `:1274`) and belongs in its own change with its own tests.
  **Raise as a separate issue.**
- **Blank-line and header-guess audit of `import_csv`** — the same class of parser-default trap
  (D-12) may have rapidcsv analogues on the existing import path, which PROJECT.md already flags as
  carrying two live data-corruption paths. Out of scope here by explicit decision (UNIFY-01/02/03
  are v2).
- **CSV writing from Lua** (`db:write_csv`) — v2, WRITE-01.
- **Direct gtest coverage of the reader** without the Lua boundary — only if Phase 2's PARSE-02..07
  matrix proves awkward through Lua; the two-line escape hatch is recorded in Integration Points.

</deferred>

---

*Phase: 1-A Lua script reads a CSV file*
*Context gathered: 2026-09-15*
