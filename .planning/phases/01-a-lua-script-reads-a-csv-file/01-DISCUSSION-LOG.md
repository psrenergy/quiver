# Phase 1: A Lua script reads a CSV file - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-09-15
**Phase:** 1-A Lua script reads a CSV file
**Areas discussed:** read_csv return shape, Stream callback contract, Core reader surface, Options and memory window

**Method:** the user asked for researched best practice rather than asserted opinion, so each area
was settled by an adversarial research workflow rather than by preference. Two workflows, 27
subagents total: 7 research angles, 4 syntheses, 12 refutation lenses, 3 verdict passes. Several
lenses verified claims by building and running `quiver_cli` against the real tree. Refutation
verdicts are recorded per area below.

---

## read_csv return shape

*Research: 4 angles (house conventions, cross-language prior art, LLM ergonomics, Phase 2
forward-compat) → 1 synthesis → 3 refutation lenses. **Verdict: 0/3 refuted.***

| Option | Description | Selected |
|--------|-------------|----------|
| One table `{header, rows}` | Single table with named keys; survives `return db:read_csv(p)` intact through `run`/`transaction`/`dry_run`, all of which keep return value 0 only | ✓ |
| Two return values `rows, header` | Idiomatic Lua multi-return | |
| Rows only, header is row 1 | Flat array, `rows[1]` is the header line | |

**User's choice:** One table `{header, rows}`

**Notes:** The decisive evidence was the host encoder, not ergonomics. `LuaRunner::run` encodes only
`result.get<sol::object>(0)` (`src/lua_runner.cpp:1806`), and `db:transaction`/`db:dry_run` collapse
the callback identically (`:302`, `:327`) — so multi-return silently drops the header at three
sites on the most natural line a script writes. House precedent was unanimous:
`get_vector_metadata_lua:1191` returns the same descriptor+parallel-array shape, and
`read_element_by_id_lua:1390` faced this exact fork with three named parts and merged rather than
multi-returned. Zero `as_returns`/`std::tuple`/`variadic_results` in 1813 lines. Rows-only was
killed by `append_json_table:145` being array-iff-keys-1..n — adding any scalar key later flips the
JSON from array to key-sorted object.

The refute pass corrected one claim in the recommendation's own rationale, which is recorded in
CONTEXT.md rather than hidden: the winning shape does **not** always fail loudly.
`ipairs(db:read_csv(p))` gives zero iterations silently, and `lua-api.ts:105` primes exactly that.
It still wins — one conditional silent mode against two, and against one guaranteed one.

---

## read_csv return shape — rider: row padding

| Option | Description | Selected |
|--------|-------------|----------|
| No padding, pin it now | Short row stays short; `row[j]` nil past its end | ✓ |
| Pad to header width | Every row the header's length, short rows filled with `""` | |

**User's choice:** No padding

**Notes:** Raised by a refutation lens as the one genuine Phase-2 breaking risk that is *not* about
return shape: csv-parser pads/trims by default, so shipping padded rows in Phase 1 would force a
breaking change when PARSE-07 lands. Costs one parser-config line now, nothing later. A later lens
then found the default is worse than "pads" — it is `IGNORE_ROW`, which *silently discards* ragged
rows outright (see the Options area).

---

## read_csv return shape — rider: empty file

| Option | Description | Selected |
|--------|-------------|----------|
| Throw | `Cannot read_csv: file 'x.csv' is empty` (Pattern 1) | ✓ |
| `header = nil, rows = {}` | Return normally with an empty result | |

**User's choice:** Throw

**Notes:** Keeps `if csv.header then` meaning exactly one thing rather than doubling as "the file
was empty" — the residual ambiguity the Phase-2 lens had flagged. LUA-08 already establishes this
family raises a named Pattern 1 error.

---

## Stream callback contract

*Research: 3 angles (existing Lua callbacks in the repo, cross-language streaming/early-termination
prior art, sol2 mechanics) → 1 synthesis → 3 refutation lenses. **Verdict: 3/3 refuted the code
shape, 0/3 refuted the five contract answers.***

| Option | Description | Selected |
|--------|-------------|----------|
| `on_row(row, index, header)` | Data first, index 1-based over data rows, header reachable during the stream | ✓ |
| `on_row(row)` | Matches the arity-1 shape of the only two existing callbacks | |
| `on_row(db, row, ...)` | Mirror `db:transaction`'s leading `db` | |

**User's choice:** Not put to the user — 0/3 lenses could refute it, so it was recorded as settled.

**Notes:** `db`-first was rejected on evidence: `fn(std::ref(self))` in `db:transaction`/`db:dry_run`
(`:290`, `:315`) is transaction-scoping ceremony, not a house rule. The header-as-third-argument
decision is the highest-value one in the area — a lens demonstrated that without it a model writes
the universal streaming idiom `if not header then header = row; return end`, which silently eats the
first data record and turns every column name into a cell value.

**What the 3/3 refutation actually hit:** the illustrative code, not the decisions. The proposed
test named a fixture (`LuaRunnerCsvTest`, `dir`, `write_file`, bare `runner`) that does not exist —
the real one is `LuaSandboxTest` with `sandbox`/`db_path()`; it invented a `NOLINT` pair in a region
of `lua_runner.cpp` that has none; it returned `size_t` where house counts are `int64_t`; and the
shared helper omitted both the empty-file throw and any wrapping of csv-parser's own open error,
which would have surfaced a raw parser error in violation of LUA-08 and made the two entry points
diverge on an empty file in violation of LUA-03. All folded into CONTEXT.md.

---

## Stream callback contract — early stop

*This is where two refutation lenses disagreed outright, so it went to the user.*

| Option | Description | Selected |
|--------|-------------|----------|
| Yes — `return false` stops | Costs one bool internally; kills the silent full-file read | ✓ |
| No — defer to a later phase | No requirement asks for it; one-way door the cheap way round | |

**User's choice:** Yes — `return false` stops

**Notes:** The case that won: with no early stop, "print the first 5 rows of huge.csv" yields a
script that prints exactly the right 5 lines *and* streams all 4 GB — correct output, zero errors,
the bounded-read guarantee silently gone, and invisible on a test fixture, so it ships and only
bites at the scale the feature exists for. The case against, now an accepted-and-documented hazard:
a callback ending `return row[1] ~= ""` returns a genuine Lua `false` and truncates the stream.
Mandatory implementation consequence recorded in CONTEXT.md — `sol::optional<bool>`, never
`get<bool>`, because a no-return callback must mean continue.

---

## Core reader surface and placement

*Research: 2 angles (repo structure/public-vs-internal precedent, csv-parser build cost) →
1 synthesis → 3 refutation lenses. **Verdict: 3/3 refuted the code shape, 0/3 refuted the
placement.***

| Option | Description | Selected |
|--------|-------------|----------|
| Internal `src/csv_read.{h,cpp}` | No public header, no C API, no FFI bindings; the "bind everywhere" rule never fires | ✓ |
| Public `include/quiver/csv_reader.h` with a documented binding exception | Mirrors the binary/expression subsystem's treatment | |

**User's choice:** Not put to the user — 0/3 lenses could refute the placement.

**Notes:** Root CLAUDE.md's "all public C++ methods should be bound to C API, then to every binding"
attaches to *public* headers, and REQUIREMENTS.md explicitly puts Julia/Dart/Python/JS exposure out
of scope. Staying internal means no exception is needed at all. One caveat recorded rather than
waved away: `csv_read.cpp` would be the first `.cpp` in `QUIVER_SOURCES` with no public header —
every internal helper today is header-only inline — so it is a new pattern and `src/CLAUDE.md` has
to say so.

**What the refutation hit:** the shape omitted the separator entirely (PARSE-08 is Phase 1, not
Phase 2 — all three lenses caught this independently); it wrote a `to_lua_row` helper duplicating
`to_lua_table`, which `src/CLAUDE.md` names as the *only* vector→table marshaler; it claimed
`transaction`/`read_scalars_by_id` are Lua-only when root CLAUDE.md documents them across all five
bindings; and it dropped the required `src/CLAUDE.md`, root `CLAUDE.md` and `CHANGELOG.md` updates.
A lens also surfaced the `CSV_NO_SIMD` finding — with SIMD on, csv-parser adds `/arch:AVX2` as a
**PUBLIC** compile option that propagates into `quiver` and would SIGILL on pre-AVX2 x86 for every
shipped wheel, npm native, Julia artifact and S3 binary.

---

## Options and memory window

*Research: 2 angles (house options/error conventions, csv-parser window internals + prompt-token
economics) → 1 synthesis → 3 refutation lenses. **Verdict: 3/3 refuted — two of them by running
`quiver_cli` against the real build.***

| Option | Description | Selected |
|--------|-------------|----------|
| One key, `separator`, trailing optional table, shared decoder, no window knob | Prompt-weight constraint says each knob has to earn its place | ✓ |

**User's choice:** Not put to the user as a whole — the evidence settled the shape; two sub-questions
below did go to the user.

**Notes — the most consequential finding of the whole discussion:** a lens ran
`db:export_csv("Configuration","","out.csv","NOT_A_TABLE")` against the real Debug build and it
**succeeded silently**, file written, defaults used. Same for a number and a boolean. Root cause:
`SOL_SAFE_FUNCTION=1` at `src/CMakeLists.txt:60-61` is a **dead define** — sol2 3.5.0 reads
`SOL_SAFE_FUNCTION_CALLS`/`SOL_SAFE_FUNCTION_OBJECTS` — and sol2's optional checker forwards with
`&no_panic`, so it never raises. Consequence for this feature:
`db:read_csv("f.csv", ";")` would silently parse with `,`, making `a;b;c` one column and filling the
database with garbage without a single error. Fix is local (`sol::object` + hand type-check, the
house precedent at `:1006`); the global fix is logged as a deferred issue.

Two further library defaults were caught the same way: `variable_columns` defaults to `IGNORE_ROW`,
which silently discards ragged rows (violating the padding decision above), and the header row is
*guessed* unless `header_row(0)` is pinned — which on a file with a title line above the header,
i.e. the Maranhão files this milestone exists for, silently eats the title row.

---

## Options and memory window — error naming

| Option | Description | Selected |
|--------|-------------|----------|
| `Cannot read_csv_stream:` — the method called | Root CLAUDE.md Pattern 1; the sandbox helper already says this regardless | ✓ |
| `Cannot read_csv:` — literal LUA-08 text | One greppable token for both forms | |

**User's choice:** Name the method called

**Notes:** Both syntheses independently flagged this as the one thing the evidence could not settle,
because two authoritative documents point opposite ways and neither is stale. The tiebreaker: the
sandbox helper will emit `Cannot read_csv_stream` for a path escape no matter what, so taking
LUA-08's wording literally would make the two halves of the same call disagree with each other.

---

## Options and memory window — unknown option key

| Option | Description | Selected |
|--------|-------------|----------|
| Throw — `Cannot read_csv: unknown option 'delim'` | Typo trap now; matters more once Phase 2 adds real keys | ✓ |
| Ignore, matching `parse_csv_options` | Matches existing code; forward-compatible across versions | |

**User's choice:** Throw

**Notes:** A deliberate divergence from `parse_csv_options` (`src/lua_runner.cpp:807`), which
ignores unknown keys today. The existing code pointed one way and the project's stated failure
philosophy (TEST-04 exists specifically to rule out silent fallbacks) pointed the other.

---

## Options and memory window — blank lines

| Option | Description | Selected |
|--------|-------------|----------|
| `KEEP_NON_EMPTY` — blank lines skipped | Ragged rows still survive; count stays meaningful | ✓ |
| `KEEP` — a blank line is an empty row | Maximally faithful to the file | |

**User's choice:** `KEEP_NON_EMPTY`

**Notes:** Raised by a lens that read `accept_row` (`csv_reader.cpp:106-108`) and found `KEEP`
accepts a 0-field row, so every blank line — including the trailing newline of any hand-edited file
— would fire the callback and inflate the returned count.

---

## Claude's Discretion

- Exact internal signature of the shared helper (free function vs `LuaRunner::Impl` static, template
  vs `std::function`), provided both entry points route through one loop and one `CSVFormat`
  construction.
- Whether the early-stop `bool` rides the internal callback's return type or a separate predicate.
- Test file naming and split.

## Deferred Ideas

- **`SOL_SAFE_FUNCTION=1` is a dead define** — affects five existing `sol::optional<sol::table>`
  option sites beyond this feature. Worked around locally in Phase 1; global fix needs its own
  change and tests. Raise as a separate issue.
- **Blank-line and header-guess audit of `import_csv`** — the same class of parser-default trap may
  have rapidcsv analogues on the existing import path. Out of scope (UNIFY-01/02/03 are v2).
- **CSV writing from Lua** (`db:write_csv`) — v2, WRITE-01.
- **Direct gtest coverage of the reader** without the Lua boundary — only if Phase 2's
  PARSE-02..07 matrix proves awkward through Lua.
