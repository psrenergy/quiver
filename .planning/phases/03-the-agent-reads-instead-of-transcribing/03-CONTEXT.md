# Phase 3: The agent reads instead of transcribing - Context

**Gathered:** 2026-09-16
**Status:** Ready for planning
**Mode:** Smart discuss (autonomous) — two grey areas presented; the user chose the FULL example and
the recommended placement.

<domain>
## Phase Boundary

The shipped agent reference sends the model to the file instead of the clipboard, and the feature is
verified and documented well enough to release.

**In scope:** DOC-02 (the read-don't-transcribe instruction), DOC-03 (the worked dirty-file
example), DOC-04 (nearest `CLAUDE.md` files + changelog), TEST-05 (a Release run over the final
tree).

**Out of scope:** any change to the CSV reader's behaviour or option surface. Phases 1 and 2 closed
that; this phase is documentation plus a verification run. If a doc change seems to require a code
change, that is a signal the doc is describing something untrue — fix the doc.
</domain>

<decisions>
## Implementation Decisions

### The reference is already largely current — this is a narrow gap, not a rewrite

Phases 1 and 2 both updated `bindings/js/src/lua-api.ts` as they shipped. Its
`## CSV file reading` section (around line 616) already documents both entry points, the
`{header = , rows = }` shape, string-only cells, no row padding, both option keys, `header_row`'s
1-based rule and its `0` = no-header spelling, and the error catalogue. **Do not rewrite that
section.** Read it before editing; most of what a planner might assume is missing is already there.

What is genuinely absent is the two things this phase's requirements name: the instruction not to
transcribe (DOC-02) and a worked example over a dirty file (DOC-03).

### D-30: The read-don't-transcribe instruction goes at the no-`io` statement

`bindings/js/src/lua-api.ts` line ~93, the **Standard library** bullet, currently reads:

> Loaded standard libraries: base, string, table, math, coroutine, utf8. That is the
> pure-computation set — there is no `os`, `io`, `debug`, or `package`/`require`, and
> `dofile`/`loadfile` are removed (string-form `load` stays available).

That sentence is the *cause* of the behaviour this milestone exists to fix: a model reads "there is
no `io`" and correctly concludes it cannot open a file, then incorrectly concludes it must paste the
data into the script. The correction belongs against that sentence, not somewhere a model might not
reach — which is exactly what success criterion 1 asks for ("stated where the reference currently
tells it that it has no filesystem access").

Add the counterpoint immediately after that bullet: no `io`, **but** data files are read with
`db:read_csv` / `db:read_csv_stream`, and file contents must never be transcribed into the script.

Rejected: also repeating it at the head of the CSV section. The reference is system-prompt payload
and near-duplicate text costs tokens on every session for reinforcement the single well-placed
statement already provides. The CSV section already opens with "the only way to get file data into a
script, since `io` is deliberately absent", which points the same direction.

### D-31: The worked example is the FULL ~35-line form, over both real files

**User's explicit choice, against the recommended compact option.** The example covers both real
Maranhão shapes end to end:

- **Energia** — BOM + CRLF, a junk title row above the header and a units row below it
  (`header_row = 2`, then slice from `rows[2]`), apostrophe thousands separators stripped via
  `gsub`, `DD/MM/YYYY` split to `YYYY-MM`.
- **GD** — quoted fields containing commas, English month names mapped to numbers, clean decimals.

It must include the **`tonumber`/`gsub` parenthesis trap** explicitly, since criterion 2 names it:
`gsub` returns *two* values, so `tonumber(v:gsub("'",""))` passes the replacement count as
`tonumber`'s base argument and silently returns `nil`. The correct form is `tonumber((v:gsub(...)))`
— the extra parentheses truncate to one value. Comment it in the example; this is the single
highest-value line in the phase, because the failure is silent.

**Cost is accepted deliberately:** roughly +1000 tokens on every claw session, permanently, against
PROJECT.md's explicit prompt-weight constraint. The user weighed that and chose instructiveness.
Do not quietly trim it back toward the compact version — if it needs to shrink, that is a new
decision, not an implementation detail. Equally, do not let it grow past ~35 lines: it earns its
place by being a complete worked shape, not by touring the option surface.

Use the real fixtures' actual structure (`tests/fixtures/ma_energia_residencial.csv`,
`tests/fixtures/ma_gd_data.csv`) so the example is honest. Column indices are recorded in
`02-RESEARCH.md` and exercised by the two regression tests in `tests/test_lua_runner_read_csv.cpp`
(`EnergiaRegressionJunkRowAboveUnitsRowBelowHeader`,
`GdRegressionQuotedCommaAndEnglishMonthNames`) — lift the shape from the tests, which are known to
work, rather than writing fresh Lua that has never run.

### D-32: TEST-05 is already discharged; this phase re-runs it as a release gate

`REQUIREMENTS.md` already marks TEST-05 `[x]` — phase 2's plan 02-04 ran it (291/291 `LuaRunner*`
in both Debug and Release), and phase 2's verifier independently re-ran it in a fresh tree
(293/293). This phase's changes are documentation-only and cannot break a C++ test.

So the Release run here is a **final release gate over the finished tree**, not a rediscovery. Run
it once at the end, after the doc changes land. Use an explicit configure —
`cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DQUIVER_BUILD_TESTS=ON
-DQUIVER_BUILD_C_API=ON` — never the plain `release` preset, which sets `QUIVER_BUILD_TESTS=OFF`
and would report success while testing nothing. Remove the build tree afterwards; it is not
gitignored.
</decisions>

<code_context>
## Existing Code Insights

- **`bindings/js/src/lua-api.ts`** — 755 lines, ~38 KB, exported as `LUA_DB_API_REFERENCE` and
  interpolated into an LLM system prompt downstream. The two edit sites are the Standard library
  bullet (~line 93) and the CSV file reading section (~line 616). It is a TypeScript template
  literal: backticks and `${` inside the prose **must** stay escaped (`\``), or the file will not
  parse — the existing text shows the convention throughout.
- **`bindings/js/test/lua-api-sync.test.ts`** — parses `src/lua_runner.cpp` and asserts every bound
  `db:`/`quiver.*` name and the exact `open_libraries` list appear in the reference. It matches
  **names only** — it does not parse option keys or prose, so it will not catch a wrong or stale
  example. It will, however, fail if an edit accidentally drops a documented binding name.
- **Root `CLAUDE.md`** — the Lua surface description and the cross-layer tables. `db:read_csv` /
  `db:read_csv_stream` are Lua-only (no C++/C API/other-binding counterpart), which is already a
  documented design decision; check whether the cross-layer table needs a row or an explicit
  "Lua only" note.
- **`src/CLAUDE.md`** — already documents `csv_read.{h,cpp}`, `make_format`'s pinned settings and
  call-order constraint, and `resolve_sandboxed_path`'s wrapping. Extend only where phase 3 changes
  something; do not duplicate phase 2's entries.
- **`tests/CLAUDE.md`** — already documents `tests/fixtures/` and the release-preset trap (phase 2,
  plan 02-04).
- **`CHANGELOG.md`** — `## [0.10.4] — unreleased` already carries entries for `db:read_csv` /
  `db:read_csv_stream`, the `header_row` option, and the sandbox path-resolution fix. The phase 3
  entry is about the **agent reference** change, not the reader. Do not restate the reader's
  features.
</code_context>

<specifics>
## Specific Ideas

- **The example must be correct Lua, not plausible Lua.** It ships to a model that will imitate it.
  Lift the transformations from the two passing regression tests rather than composing new ones.
  Worth actually running the finished example once against the committed fixtures — a wrong example
  in a system prompt teaches the wrong thing on every session forever.
- Version bump: this milestone's user-visible surface is new Lua bindings and a new option. Per
  root `CLAUDE.md`, a `0.x` **minor** bump signals breaking changes and a **patch** bump does not —
  this is purely additive, so `0.10.4` (already the unreleased section) is the right target. All
  five manifests must agree; `scripts/assert_version.py` checks and can bump them.
- One correction inherited from phase 2, worth not repeating: an earlier draft claimed
  `lua-api-sync.test.ts` fails the build on an undocumented option key. It does not — it matches
  bound names only. Do not plan work around a gate that will not fire.
</specifics>

<deferred>
## Deferred Ideas

- **Trimming the worked example back toward the compact form** — explicitly a new decision (D-31),
  not an implementation detail, because the user chose the full form knowing its per-session cost.
- **CSV writing** (`db:write_csv`) — v2, per REQUIREMENTS.md. Reading is the only direction.
- **Relocating `LUA_DB_API_REFERENCE` out of `bindings/js/`** — reviewed and rejected in the root
  `CLAUDE.md`'s "Do Not Fix" list. Leave it where it is.
</deferred>
