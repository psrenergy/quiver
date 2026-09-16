# Phase 4: A Lua script writes a CSV file - Context

**Gathered:** 2026-09-16
**Status:** Ready for planning
**Mode:** Discuss — four gray areas presented; the user declined discussion and delegated all four
("use the best practices and good code"). Every decision below is **Claude's discretion**, made
against the requirements, the milestone research, and the existing code. Each one is argued, not
defaulted — a planner may follow them without re-deriving, but the rationale is recorded so a
reviewer can overturn one on its merits.

<domain>
## Phase Boundary

The CSV writer itself: `db:write_csv(path, opts)` returns a handle, `w:write_row{...}` appends one
row, `w:close()` finishes the file — plus RFC-4180 emission, cell formatting, the inherited sandbox,
and the agent reference that ships with it.

**In scope:** WRITE-01..05, WRITE-07, WRITE-08, FMT-01..06, FMT-08, FMT-09, LUA-09..11, TEST-06..09,
TEST-12, DOC-05.

**Out of scope — Phase 5, not here:** header-as-width-authority (FMT-07/TEST-10) and the flush of a
writer still open when `LuaRunner::run` returns (WRITE-06/TEST-11). A Phase-4 build writes whatever
row the script hands it, ragged or not; that is correct, not a gap.

**Out of scope — declined for the whole milestone:** a destructive-overwrite guard, an
unclosed-writer warning, a whole-file `write_csv` form, atomic write-then-rename, and every option
beyond `separator` / `header`. All five are argued down individually in
`.planning/REQUIREMENTS.md` § Out of Scope. Do not reintroduce one as an implementation detail.
</domain>

<decisions>
## Implementation Decisions

### Number formatting

- **D-34: A Lua float writes as `to_chars` gives it — no synthetic `.0`.** The float `2014.0` and
  the integer `2014` both write `2014`. FMT-04's "Lua 5.4's integer subtype is preserved" is about
  **type dispatch**, not text decoration: a Lua integer reaches `append_number`'s `std::int64_t`
  overload and a Lua float reaches its `double` overload, so `9007199254740993` survives (TEST-07).
  Decorating a whole float back to `2014.0` would mean post-processing `append_number`'s output —
  more code than reusing it, and it discards the shortest-round-trip property the helper exists for.

  Three things make this the consistent answer rather than merely the lazy one: `append_json_double`
  (`src/lua_runner.cpp:137-144`) already renders a returned Lua float exactly this way, so the
  runner's two serializers agree; `db:read_csv` returns **every cell as a string with no inference**,
  so nothing inside Quiver can tell an int cell from a float cell regardless; and catering to an
  external consumer's type inference is precisely what REQUIREMENTS § Out of Scope declines under
  "Numeric or date inference on cells". Success criterion 2 is satisfied either way — read-back is a
  string, and re-writing a string is verbatim.

  **Record the consequence in one clause** in `LUA_DB_API_REFERENCE` and `src/CLAUDE.md`: a number
  is written in its shortest round-trip form, so `2014.0` and `2014` produce the same text and a
  script that needs a decimal point writes the cell as a string. Do not add an option for it.
  — **Reversibility:** costly — the output text of every numeric cell is the thing every round-trip
  test asserts, and changing it later rewrites those fixtures and any script that grew to depend on
  the shape.

### The handle

- **D-35: The usertype is named `CsvWriter`, not `Writer`.** The sol2 usertype name lands in the
  Lua registry as a global alongside `BinaryFile`, `BinaryMetadata` and `Expression` — all
  domain-specific. A bare `Writer` is too generic for a shared namespace and collides head-on with
  the already-tracked v2 `TOML-02` writer. The name also becomes the string added to the hardcoded
  array in `bindings/js/test/lua-api-sync.test.ts:75`. The Lua-side variable in every doc example
  stays `w` — the reference already uses ad-hoc receiver names (`f`, `r`, `md`, `e`), and the sync
  test's method check is receiver-agnostic (`includes(':name(')`), so `w` costs nothing.
  — **Reversibility:** reversible — a rename touches one `new_usertype` argument, one test array
  entry, and the reference.

- **D-36: Every Pattern 1 error names the method the script actually called.** `write_csv` for the
  constructor's errors — the options table, the sandbox, the failed open, the missing parent
  directory (WRITE-03 pins this, and it is satisfied because those all raise inside `write_csv`).
  `write_row` for a table cell, a non-finite number, and a write after close. `close` for anything
  `close` itself can raise. This is not a new rule: `resolve_sandboxed_path`
  (`src/lua_runner.cpp:851`) and `csv_read::Reader` both thread an `operation` string that is the
  public method name, and the root CLAUDE.md's Pattern 1 spec says `{operation}` is "the public
  method the user called". Thread the same `std::string operation` through `csv_write::Writer` that
  `csv_read::Reader` already takes, for the same reason.
  — **Reversibility:** costly — every TEST-12 assertion matches on the message, so changing the voice
  later rewrites the error tests.

### Code organization

- **D-37: `src/csv_write.h` + `src/csv_write.cpp`, a plain concrete class — no Pimpl.** Mirrors
  `csv_read.{h,cpp}` so the two halves of one feature sit next to each other, which is the pairing
  the root CLAUDE.md and every cross-layer table already assume. It also keeps `<fstream>` /
  `<charconv>` emission out of `lua_runner.cpp`, which is 1984 lines and is the one TU compiled with
  `/bigobj` (`src/CMakeLists.txt:68`).

  **No Pimpl.** `csv_read::Reader`'s own header says why it is Pimpl'd — to keep csv-parser's
  headers out of the sol2 translation unit. A zero-dependency writer has no such headers to hide,
  so `csv_write::Writer` holds its `std::ofstream` and its state directly. Copying the Pimpl because
  the neighbour has one would be cargo cult.

  Add `csv_write.cpp` to the `quiver` source list in `src/CMakeLists.txt` next to `csv_read.cpp`
  (line 13). No `QUIVER_API`, no public `include/quiver/` header, no C API — same posture as
  `csv_read`.
  — **Reversibility:** reversible — file organization only; nothing outside `src/` sees either shape.

- **D-38: `append_number` moves to `src/utils/number.h` as `quiver::utils::append_number`, and both
  callers use it from there.** FMT-04 requires reuse, and the helper is currently a static in
  `lua_runner.cpp`'s anonymous namespace (`:128-135`), unreachable from another TU. The alternatives
  are worse: duplicating it is exactly the divergence this project guards against everywhere, and
  templating the writer over a formatter is machinery for one call site. `src/utils/` already holds
  exactly this kind of header-only internal helper (`datetime.h`, `string.h`) under a
  `quiver::<name>` namespace, so this is a move into an existing pattern, not a new one. After the
  move, the JSON encoder and the CSV writer share one number formatter — strictly better than today.
  The move is behaviour-preserving; do not change the function while moving it.
  — **Reversibility:** reversible — a move plus two call sites, all inside `src/`.

### The agent reference (DOC-05)

- **D-39: The worked example is the COMPACT form — roughly 10-12 lines — not Phase 3's full ~35-line
  shape.** This is a deliberate departure from the immediately preceding precedent, and the reason
  is that Phase 3's precedent was justified by something the write side does not have. That example
  earned ~1000 permanent tokens per `claw` session because *reading* has a trap catalogue a model
  gets wrong unaided: BOM, CRLF, a junk row above the header and a units row below it, `header_row`
  arithmetic, and the silent `tonumber((v:gsub(...)))` parenthesis trap. **Writing has no equivalent
  trap.** Quoting, escaping, the terminator and number formatting are all the writer's job and are
  invisible to the script; `nil` and booleans need no handling. What is left is three calls in a
  loop, and a model that has seen `db:read_csv`'s section will not get them wrong.

  The example must show: `db:write_csv` with both options, a loop writing mixed string / number /
  boolean / `nil` cells, and `w:close()`. Around it, in prose rather than code, DOC-05's three
  required statements — the two options and their defaults, that the target is **truncated at open**
  (WRITE-08), and that `write_row` after `close` throws while `close` is idempotent (WRITE-05) —
  plus the D-34 number clause and the `nil`-vs-`""` note in D-40.

  **The example must be verified to run**, not merely plausible — the roadmap's criterion 5 says
  "runs verbatim". A test executes the example's Lua and round-trips the result through
  `db:read_csv`, the same guarantee Phase 3 asked for and the same reason: a wrong example in a
  system prompt teaches the wrong thing on every session forever.

  If a later reviewer wants the full form, that is a new decision with a stated per-session cost —
  not an implementation detail. Equally, do not shrink it below a complete open-write-close shape.
  — **Reversibility:** reversible — prose, though the token cost of growing it is permanent.

- **D-40: `nil` and `""` are structurally indistinguishable after a round trip — state it, do not
  try to fix it.** Both write an empty cell; both read back as `""`, because `db:read_csv` returns
  strings. The research called this unfixable and it is: CSV has no null. One clause in the
  reference next to the `nil` rule, and a line in `src/CLAUDE.md`. No sentinel, no option.
  — **Reversibility:** reversible.

### Claude's Discretion

The user delegated the entire discussion, so D-34 through D-40 above are all discretionary. Four
smaller calls the planner would otherwise guess at, settled here so it does not have to:

- **`header = {}`** (an empty list) is treated as **no header row** — same as omitting the key.
  Writing a zero-cell first row would emit a blank line, which this project's own reader deletes
  (the FMT-02 hazard). It is a degenerate input, not worth an error of its own.
- **A ragged row in Phase 4 is written as-is.** FMT-07 is Phase 5's requirement and this phase does
  not carry it. The Phase-4 writer emits exactly the cells FMT-08's integer-key pass found.
- **Test file:** `tests/test_lua_runner_write_csv.cpp`, registered in `tests/CMakeLists.txt`,
  mirroring `test_lua_runner_read_csv.cpp` and using `tests/test_lua_runner.h`'s fixture. Every
  correctness assertion round-trips through `db:read_csv` (see the trap in `<specifics>`).
- **`w:close()` returns nothing**, and `db:write_csv`'s options table is optional — a bare
  `db:write_csv("out.csv")` is valid and means `,` with no header, matching `read_csv`'s D-14.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Milestone specification
- `.planning/REQUIREMENTS.md` — the 29 v1.1 requirements; § Out of Scope argues down every declined
  option individually. The authority on what this phase must do and must not add.
- `.planning/ROADMAP.md` § Phase 4 — the five success criteria and the three inseparability notes
  (DOC-05-with-the-binding, FMT-02-with-TEST-09, the closed ownership question).
- `.planning/research/SUMMARY.md` — the milestone research: why csv-parser's `DelimWriter` was
  rejected, the five pitfalls, and the gaps this file now closes.
- `.planning/STATE.md` § Accumulated Context — the locked decisions carried into this milestone, and
  the three live Blockers/Concerns.

### Prior-phase context that still binds
- `.planning/milestones/v1.0-phases/03-the-agent-reads-instead-of-transcribing/03-CONTEXT.md` —
  D-30/D-31 (why the full worked example was chosen for *reading*; D-39 above departs from it
  deliberately) and D-33 (the CHANGELOG is now at `0.10.7 — unreleased` against manifests at
  `0.10.6`; no version bump happens in this milestone either).
- `.planning/milestones/v1.0-phases/01-a-lua-script-reads-a-csv-file/01-CONTEXT.md` — D-14/D-17/D-22,
  the options-decoding rules LUA-09 and LUA-10 require this phase to copy.

### Code the writer must mirror or reuse
- `src/csv_read.h` / `src/csv_read.cpp` — the shape `csv_write` mirrors, and the header comment that
  explains the Pimpl rationale D-37 declines to copy.
- `src/lua_runner.cpp:128-135` — `append_number`, moved by D-38.
- `src/lua_runner.cpp:851-900` — `resolve_sandboxed_path`; needs **no change** (`weakly_canonical` is
  already existence-agnostic, so it is correct for a target that does not yet exist).
- `src/lua_runner.cpp:926-1000` — `read_csv_options_from_lua`, the collect-then-validate decoder
  LUA-09 requires; the `separator` branch is reusable almost verbatim.
- `src/lua_runner.cpp:565-579` — `BinaryFile`'s usertype registration, the LUA-11 ownership pattern.
- `src/lua_runner.cpp:1085-1150` — `lua_table_to_value_map` / `table_to_element`, the type-dispatch
  pattern for cells. Do **not** use `lua_cell_as<T>`; that is for homogeneous arrays.

### Documentation targets
- `bindings/js/src/lua-api.ts` — `LUA_DB_API_REFERENCE`. A TypeScript template literal: backticks
  and `${` in prose must stay escaped or the file will not parse.
- `bindings/js/test/lua-api-sync.test.ts:75` — the hardcoded `["BinaryFile", "BinaryMetadata",
  "Expression"]` array that D-35's name is added to.
- Root `CLAUDE.md` § Design Decisions (the `db:read_csv` entry, which gains the writer) and
  `src/CLAUDE.md`. `CHANGELOG.md` is Phase 5's (DOC-06).

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- **`append_number`** (`src/lua_runner.cpp:129`) — `std::to_chars` shortest round-trip into a
  `std::string`, templated on the value type, 32-byte stack buffer. Moved by D-38; used as-is.
- **`resolve_sandboxed_path`** (`src/lua_runner.cpp:851`) — the single choke point every
  file-touching Lua operation routes through. Already rejects `:memory:` (WRITE-04's second half)
  and already wraps `std::filesystem_error` into Pattern 1 (LUA-08). Call it, change nothing.
- **`read_csv_options_from_lua`** (`src/lua_runner.cpp:934`) — collect-entries-then-validate, with
  the explicit-`get_type()`-not-`lua_cell_as` rule and the reason spelled out in comments. The
  `separator` validation (string, then `size() != 1`) transfers to the writer unchanged.
- **`tests/test_lua_runner.h`** — the fixture every `test_lua_runner_*.cpp` uses.

### Established Patterns
- **Pattern 1 everywhere** — `"Cannot {operation}: {reason}"`, with `{operation}` the public method
  the user called. D-36 applies it; TEST-12 asserts it.
- **`sol::no_constructor` + `std::unique_ptr` return** — `db:open_file` returns
  `std::make_unique<BinaryFile>(...)` and registers the usertype with `sol::no_constructor` and no
  explicit `__gc`. LUA-11 pins this; the ownership question the research left open is closed.
- **One implementation behind two entry points** — `csv_read::Reader` exists so `db:read_csv` and
  `db:read_csv_stream` cannot diverge. The writer has only one entry point by decision (no
  whole-file form), so this pattern is satisfied trivially — which is itself the argument against
  ever adding the second form.
- **Internal helpers live in `src/utils/*.h`** as header-only inline functions under a
  `quiver::<name>` namespace (`datetime.h`, `string.h`). D-38 follows it.

### Integration Points
- **`src/CMakeLists.txt:13`** — add `csv_write.cpp` to the `quiver` source list beside
  `csv_read.cpp`. `/bigobj` at `:68` is scoped to `lua_runner.cpp` alone and is not needed here.
- **`src/lua_runner.cpp:452-480`** — where `read_csv` is bound; `write_csv` goes next to it, and the
  `CsvWriter` usertype next to `BinaryFile`'s registration at `:565`.
- **No C API, no FFI, no binding work.** `db:write_csv` rides inside the already-bound generic
  `LuaRunner::run` / `quiver_lua_runner_run` path — confirmed by the research against all four
  bindings. Only `quiver_tests` gets real coverage.

</code_context>

<specifics>
## Specific Ideas

- **The round-trip rule is not a style preference — it is this project's third encounter with the
  same trap.** `export_csv` has 118 export-side string-search tests that never re-import. Every
  correctness assertion in this phase reads the file back through `db:read_csv` and compares cells.
  A test that greps the output file for a quote character proves the emitter emitted, not that the
  file is readable.
- **Spot-check `std::to_chars` on a non-finite double before relying on anything about it.** This is
  the single MEDIUM-confidence claim in the entire research body — verified against standards text,
  never executed. Compile and run `0.0/0.0` and `1.0/0.0` through it. FMT-05's `std::isfinite` guard
  throws regardless of what it renders, so the guard is required either way; the spot-check is about
  knowing what the codebase is actually guarding against.
- **FMT-02 is narrow on purpose.** Quote an empty cell only when it is the row's *only* cell.
  Quoting every empty cell would turn `a,,b` into `a,"",b` — larger output for no benefit, since a
  multi-column row with an empty field is never a blank line. TEST-09's single-column fixture is the
  only test that can see this fail.
- **FMT-08 is not pedantry.** A row's cell count comes from one pass over integer keys, never `#t` —
  `lua_rawlen` returns an arbitrary border on a table with holes, and a table with a trailing hole is
  exactly what every nullable read produces. `append_json_table` (`src/lua_runner.cpp:145+`) already
  walks a table this way and says so in a comment; copy that, not `#t`.
- **DOC-05 lands in the same commit as the binding.** `bindings/js/test/lua-api-sync.test.ts` parses
  `src/lua_runner.cpp` and fails the build until `write_csv` is a literal token in the reference.
  Splitting them makes this phase unbuildable. Note the verified blind spot, so nobody plans around a
  gate that will not fire: the usertype-method check runs over a hardcoded array and is
  receiver-agnostic, and `BinaryFile` already puts `f:close(` in the reference — so adding
  `CsvWriter` to that array newly asserts `write_row` and nothing else. **DOC-05 is the real
  guarantee; the array entry is one token of courtesy.**
- **Exercise the Release build separately before the phase is called done.** `SOL_SAFE_GETTER` is off
  in Release and has hidden Lua marshalling bugs before. Use an explicit configure
  (`-DCMAKE_BUILD_TYPE=Release -DQUIVER_BUILD_TESTS=ON -DQUIVER_BUILD_C_API=ON`), never the plain
  `release` preset — it sets `QUIVER_BUILD_TESTS=OFF` and would report success while testing nothing.

</specifics>

<deferred>
## Deferred Ideas

- **Header-as-width authority and the unclosed-writer flush** — Phase 5 (FMT-07, WRITE-06, TEST-10,
  TEST-11). Not a gap in Phase 4.
- **`CHANGELOG.md`, root `CLAUDE.md` and `src/CLAUDE.md` as the milestone record** — DOC-06, Phase 5.
  Phase 4 still edits the reference (DOC-05) and should add the D-34/D-40 clauses where it touches
  `src/CLAUDE.md`, but the milestone write-up is Phase 5's.
- **The full ~35-line worked example** — D-39 chose the compact form. Growing it is a new decision
  with a stated permanent per-session cost, not an implementation detail.
- **A `.0` suffix, a numeric-format option, or any type-inference hint** — D-34 declines all three.
- **TOML read/write for Lua** (`TOML-01`/`TOML-02`) — v2. D-35's `CsvWriter` naming leaves room for
  it deliberately.
- **`import_csv`/`CSVOptions` unification** (`UNIFY-01..03`) and **write throughput**
  (`PERF-01`/`PERF-02`) — v2, unchanged.
- **Pre-existing and untouched:** `import_csv`'s global `;`→`,` replace and its quote-unaware
  trailing-comma stripper are live data-corruption paths. Out of scope here; tracked as UNIFY-01.

</deferred>

---

*Phase: 4-a-lua-script-writes-a-csv-file*
*Context gathered: 2026-09-16*
