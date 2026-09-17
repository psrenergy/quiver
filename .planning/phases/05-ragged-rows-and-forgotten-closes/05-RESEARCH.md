# Phase 5: Ragged rows and forgotten closes - Research

**Researched:** 2026-09-17
**Domain:** C++ RFC-4180 CSV writer width enforcement (FMT-07) + sol2/Lua garbage-collector-driven
resource flush (WRITE-06), inside the existing `db:write_csv` Lua surface shipped in Phase 4.
**Confidence:** HIGH (Q1's load-bearing GC claim was executed against this repo's own sol2/Lua
build this session, not merely read against documentation; Q2/Q3/Q4/Q6 are verified by reading the
actual shipped Phase 4 source; Q5 is a design recommendation flagged for escalation)

## Summary

Phase 5 is two narrow, independent additions on top of the already-shipped, already-reviewed
Phase 4 writer (`src/csv_write.h/.cpp`, `CsvWriter` in `src/lua_runner.cpp`): (1) make the `header`
option the row-width authority — pad a short row, throw on a long one — and (2) flush a writer the
script never closed, by calling `sol::state::collect_garbage()` once at the end of
`LuaRunner::run()`.

The phase's single highest-risk claim — that **one** `collect_garbage()` call actually runs the
`CsvWriter`'s destructor (and therefore `Writer`'s destructor, which calls
`out_.close()`, which flushes) — was not left as a research-time assumption. It was compiled and
executed against this repo's own `build/_deps/sol2-src` / `build/_deps/lua-build`, using the
project's real `SOL_SAFE_NUMERICS=1`/`SOL_SAFE_FUNCTION=1` flags and the exact
`unique_ptr`-returning-factory + `sol::no_constructor` ownership shape `CsvWriter` already uses.
Result: **one call is sufficient and deterministic** for every reachability shape tested (bare
local, nested-in-table, no binding at all, and the error/throw path). Full detail and the probe
source are below Q1.

**Primary recommendation:** Wrap `LuaRunner::run`'s `safe_script` call in one RAII guard that calls
`impl_->lua.collect_garbage()` exactly once in its destructor, so all three exit paths (throw,
empty-return, encoded-return) get the flush uniformly with zero duplicated calls. Enforce FMT-07's
width rule in the Lua layer (`CsvWriter`/`lua_runner.cpp`), not in `csv_write::Writer` — the ordinal
it must report already lives in `CsvWriter::next_row_index`, and `Writer` has no reason to grow a
new parameter for a rule its only caller can enforce before ever calling it.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| FMT-07 | With a `header`, the header's length is the row width — a shorter row pads with empty cells, a longer one throws naming the row ordinal and both counts. With no `header`, no width check is performed | Q2 (placement recommendation + illustrative shape), Q3 (padding/FMT-02 interaction traced against actual source) |
| WRITE-06 | A writer still open when `LuaRunner::run` returns is flushed to disk, so the file is complete even if the script never called `close()`. No warning is emitted | Q1 (executed probe proving one `collect_garbage()` call suffices), Q5 (RAII mechanism recommendation + error-path escalation) |
| TEST-10 | A short row against a multi-column header round-trips aligned; a longer row throws | Q3 (traces exact `append_record` behavior for the padded/rejected cases) |
| TEST-11 | A script that returns without calling `close()` leaves a complete, re-readable file, asserted after `run()` returns without destroying the `LuaRunner` | Q4 (concrete test shape, legality of a second `run()` call, RED-verification caveat) |
| DOC-06 | `src/CLAUDE.md`, root `CLAUDE.md` and `CHANGELOG.md` record the writer and its design decisions | Q6 (concrete file/line edit list, version-mismatch scope note, escalation on `lua-api.ts`) |
</phase_requirements>

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Row-width enforcement (FMT-07: pad short, throw on long) | Lua binding (`CsvWriter` in `src/lua_runner.cpp`) | — | The row ordinal (`next_row_index`) and the decoded `header` option already live only in the Lua layer; `csv_write::Writer` has no header-width state today and no reason to gain one (see Q2) |
| RFC-4180 emission (quoting, blank-line defense, terminator) | C++ core (`csv_write::Writer::write_row`/`append_record`) | — | Unchanged by this phase — the padded/validated vector is just another `std::vector<std::string>` by the time `Writer` sees it |
| Unclosed-writer flush (WRITE-06) | C++ core / sol2 runtime boundary (`LuaRunner::run`, `src/lua_runner.cpp`) | — | `sol::state` (and therefore every live `CsvWriter` userdata) is owned by `LuaRunner::Impl`; only `run()`'s own scope can trigger a deterministic collection before control returns to the C++ caller |
| Sandbox / path resolution | C++ core (`resolve_sandboxed_path`) | — | Unchanged; `write_csv` already calls it before options decode (LUA-10), and Phase 5 adds no new file path |
| Documentation of the writer's design decisions (DOC-06) | Docs (`src/CLAUDE.md`, root `CLAUDE.md`, `CHANGELOG.md`) | — | No code tier — pure record-keeping requirement |

## Q1 — Does one `collect_garbage()` call actually flush? [EXECUTED, HIGH confidence]

**Recommendation: yes, unconditionally.** One call to `sol::state::collect_garbage()`
(`lua_gc(L, LUA_GCCOLLECT)`) placed after `safe_script` returns is sufficient to run a
`CsvWriter`'s (and hence a `csv_write::Writer`'s) destructor synchronously, for every reachability
shape a real script can produce. This is not a documentation-derived claim — it was **compiled and
run this session**.

### What was executed

A standalone probe (`gc_probe.cpp`, compiled with `cl /std:c++20 /EHsc /MDd`, linked directly
against this repo's own `build/_deps/sol2-src/include` and
`build/_deps/lua-build/src/lua-5.4d.lib`, with `SOL_SAFE_NUMERICS=1`/`SOL_SAFE_FUNCTION=1` defined
to match `src/CMakeLists.txt:61-63`) mirrors `LuaRunner::run`'s real shape exactly: a `Probe`
usertype registered `sol::no_constructor`, returned from a global factory function as
`std::unique_ptr<Probe>` (byte-for-byte the same ownership pattern as `write_csv` at
`src/lua_runner.cpp:690-698`), with a real (non-trivial) destructor that prints and increments a
counter — standing in for `Writer::~Writer()`'s `out_.close()`.

Each case below gets its own freshly-constructed `sol::state` (a v1 of this probe that reused one
shared state across cases produced a confusing double-print; that was cross-contamination from a
**prior** case's leaked object being finalized when the shared variable was reassigned, not two
collections of the same live object — corrected by isolating every case in its own scope).

| Case | Script | `collect_garbage()` calls | Result |
|---|---|---|---|
| A | `local w = make_probe()` | 0 (today's shipped behavior) | Object survives past "run() returns"; destructor fires only when the whole `sol::state` is later torn down — **this is the bug WRITE-06 fixes** |
| B | `local w = make_probe()` | 1 | Destructor fires **before** "immediately after run() returns" prints — i.e., synchronously inside the single `collect_garbage()` call |
| C | `local w = make_probe()` | 2 | Fires on the *first* call; the second call finds nothing left to collect |
| D | `local t = { w = make_probe() }` (nested in a table, not a bare local) | 1 | Same result as B — no extra anchor from the nesting |
| F | `make_probe()` (bare statement, never bound to any name at all) | 1 | Same result as B |
| G | `local w = make_probe()` then `error('boom')` (mirrors `run()`'s throw path) | 1, called **before** the rethrow | Destructor fires; the exception is still caught correctly afterward |
| H | `local w = make_probe()` (1 collect), then a **second** `safe_script` call on the **same still-live `sol::state`** (`return 41 + 1`) | 1 (on the first call only) | First object destroyed as in B; second script runs and returns `42` normally — confirms a `collect_garbage()` call does not corrupt or reset the state for a subsequent `run()` |

Case H is the direct analogue of TEST-11's real shape (see Q4): one `LuaRunner`, `run()` called
twice, second call reads back what the first call's script left unclosed.

**What would falsify this:** if Case B had shown the object still alive after the single
`collect_garbage()` call (requiring a second call, or showing the destructor deferred to a later
cycle), the "one call" claim would be false and WRITE-06 would need either two calls or a different
mechanism (e.g., `lua_gc(L, LUA_GCCOLLECT)` called in a loop until `lua_gc(L, LUA_GCCOUNT)` stops
shrinking). That was not observed in any of the six reachability shapes tested. Re-run
`gc_probe.cpp` (reproducible from the transcript of this research session) against a different
Lua/sol2 vendor version before trusting this across a dependency bump.

### Why the "two collections" theoretical concern doesn't apply here

Lua 5.4's incremental collector *can* in general defer a finalizer to a later step when the
collector is paused mid-cycle by allocation-triggered single-step calls. `lua_gc(L, LUA_GCCOLLECT)`
is different: per the Lua 5.4 manual it "performs a full garbage-collection cycle," which includes
running pending finalizers before returning — this is why `sol::state::collect_garbage()`
(a thin forward to that call) is documented as the correct one-shot flush. The probe confirms this
matches actual behavior on this toolchain, for this exact ownership shape.

### Registry/anchor check

Case D (nested in a table) and Case F (never bound to anything) both collected on the first call —
ruling out the concern that `sol::protected_function_result` or the Lua registry keeps an extra
reference to a script-local userdata after `safe_script` returns. `LuaRunner::run` (verified by
reading `src/lua_runner.cpp:2165-2179`) does not retain `result` past the function body, and
nothing else in `Impl` holds a per-call reference to script locals — `sol::state lua` is the only
long-lived member touching Lua state (`src/lua_runner.cpp:236`).

## Q2 — Where does FMT-07 belong: `csv_write::Writer` or the Lua `CsvWriter` handle?

**Recommendation: the Lua layer (`LuaRunner::Impl::CsvWriter` / `csv_row_cells_from_lua` in
`src/lua_runner.cpp`), not `csv_write::Writer`.**

### Verified facts driving this

- `csv_write::Writer`'s constructor (`src/csv_write.cpp:91-127`, verified by reading) consumes
  `Options::header` only to emit the header record, then never stores it — `Writer` retains **no**
  member recording header width today (confirmed: the only members are `out_`, `separator_`,
  `original_path_`, `closed_` — `src/csv_write.h:66-70`).
- The 1-based row ordinal FMT-07's error must name lives **only** in
  `LuaRunner::Impl::CsvWriter::next_row_index` (`src/lua_runner.cpp:254-261`), not in `Writer`.
  `Writer::write_row`'s signature is `write_row(const std::vector<std::string>& cells, const
  std::string& operation)` — no row-index parameter (`src/csv_write.h:59`, verified).
- The header option is decoded once, in `write_csv_options_from_lua`
  (`src/lua_runner.cpp:371-...`), and the resulting `csv_write::Options` is handed to `Writer`'s
  constructor at the `write_csv` factory call site (`src/lua_runner.cpp:690-698`) — the same place
  that already constructs the `CsvWriter` wrapper.

### The two placements, laid out

**Option A — enforce inside `Writer`.** Would require: (1) `Writer` to retain
`options.header.size()` as a new member past construction (a real state change to an
already-shipped, already-reviewed class); (2) `Writer::write_row`'s signature to grow a
`row_index` parameter it does not otherwise need, purely so its error message can name the
ordinal — plumbing that must be threaded from `CsvWriter` anyway, since `Writer` itself has no
concept of "the Nth row" (each call is independent). No future second caller benefits from this,
because none is planned or permitted (REQUIREMENTS explicitly declines a second writer form).

**Option B — enforce in the Lua layer, before calling `Writer::write_row` at all (recommended).**
`CsvWriter` gains one new member — the header width captured once at construction from the
already-decoded `csv_write::Options::header.size()` — and `csv_row_cells_from_lua`'s output vector
is padded/validated against it immediately after the existing FMT-08 max-integer-key walk builds
it, before `self.writer.write_row(...)` is ever called. Consequences:
- **Zero interface change to `csv_write::Writer`** — the already-shipped, already-reviewed (04-REVIEW.md),
  45-tests-passing class stays exactly as-is; only `src/lua_runner.cpp` changes.
- The row ordinal and the header width are both already in scope at the exact point
  `csv_row_cells_from_lua`'s result is produced — no plumbing across a file/class boundary.
- D-36 ("`{operation}` is always the public Lua method the script called") is trivially satisfied:
  this code already threads `operation = "write_row"`.
- Symmetric with where FMT-05 (non-finite check) and FMT-08 (the max-integer-key walk) already
  live — both are Lua-table-semantics concerns handled in `lua_runner.cpp`, not pushed down into
  `Writer`.

### Message-string count

Exactly **one** new Pattern 1 message is added to `csv_write.cpp`'s pinned TEST-12 catalogue
comment block (the block explicitly covers "this header/cpp plus the write_row cell formatter it
feeds from `src/lua_runner.cpp`" — `src/csv_write.cpp:8-11`, verified), for the too-long-row case,
e.g. (exact wording is the planner's/executor's to pick, constrained only by "names the row
ordinal and both counts" and Pattern 1's `"Cannot {operation}: {reason}"` shape):

```
"Cannot write_row: row <N> has <cells> cells but header declares <header_width>"
```

Zero existing messages are reworded, and zero existing `Writer` methods change signature — the
padding/error decision happens entirely before `Writer::write_row` is invoked.

**Confidence: HIGH** — grounded in reading the actual shipped `Writer` class and `CsvWriter`
struct this session, not inference.

### Suggested shape (illustrative, not prescriptive of exact wording)

```cpp
// CsvWriter gains:
std::size_t header_width = 0;  // 0 = no header, no width enforcement (D-4-discretion:
                                // header={} already means "no header row", so this is unambiguous)

// write_csv factory, after decoding csv_options:
return std::make_unique<CsvWriter>(
    quiver::csv_write::Writer(resolved, path, "write_csv", csv_options),
    csv_options.header.size());

// in the write_row lambda, after csv_row_cells_from_lua produces `cells`:
if (self.header_width > 0) {
    if (cells.size() > self.header_width) {
        throw std::runtime_error("Cannot write_row: row " + std::to_string(row_index) +
                                  " has " + std::to_string(cells.size()) + " cells but header declares " +
                                  std::to_string(self.header_width));
    }
    cells.resize(self.header_width);  // pad with empty strings
}
```

## Q3 — The padding/FMT-02 interaction

**Verified, HIGH confidence.** Tracing `append_record` (`src/csv_write.cpp:57-87`, read in full
this session):

```cpp
const bool lone_empty_cell = cells.size() <= 1 && (cells.empty() || cells.front().empty());
```

- **`w:write_row{}` under an N≥2 header**: once FMT-07 pads the (currently zero-cell) row to N
  empty-string cells *before* `append_record` runs, `cells.size() == N >= 2`, so
  `lone_empty_cell` is **false**. The row is emitted as `N-1` bare separators (e.g. `,` for N=2) —
  a legitimate N-field row of empty cells, **not** the FMT-02 blank-line-defense spelling `""`.
  This is the correct output: `db:read_csv` (which has no NULL concept — every cell is a string)
  reads each padded cell back as `""` under its own header column, exactly matching ROADMAP
  criterion 1's "each value under the column the script meant."
- **`w:write_row{}` under a single-column (N=1) header**: padding still produces exactly 1 cell
  (empty), so `lone_empty_cell` is **true** and it *is* written as the quoted `""` blank-line
  defense — correctly, because with N=1 an empty row and a genuinely blank line are the same
  shape, and FMT-02's defense must still apply (this is the TEST-09 case, unaffected by this
  phase).
- **No interaction bug**: padding must simply happen *before* the vector reaches
  `append_record` (which Option B's placement guarantees, since padding happens in
  `lua_runner.cpp` prior to calling `Writer::write_row` at all) — `append_record` itself needs no
  change and correctly reacts to whatever final cell count it is given.

**Header record itself does not route through the width check** — it *is* the authority. Verified:
the header is written directly inside `Writer`'s constructor (`src/csv_write.cpp:119-126`), never
through `Writer::write_row`, so it can never touch `next_row_index` or any width-check logic
(which, per Q2, lives entirely in the Lua-layer `write_row` path, not the constructor). This
requires no change.

**Ordinal numbering**: the too-long-row error must use the same 1-based **first-data-row**
numbering FMT-05 already established — `CsvWriter::next_row_index` starts at `1`
(`src/lua_runner.cpp:261`, verified) and is only ever incremented after a row is *accepted*
(`src/lua_runner.cpp:717-719`); the header record never touches it (see above). FMT-07's error must
reuse `next_row_index` (or equivalently the not-yet-incremented `row_index` local already captured
at `src/lua_runner.cpp:717`) exactly as FMT-05's existing non-finite-number error does — a header
row is never counted as row 0 or row 1.

## Q4 — TEST-11's mechanics

**Verified, HIGH confidence for the shape; HIGH confidence with one execution caveat for the
"guaranteed red" claim (see below).**

### Shape

1. Build the `Database`/`LuaRunner` exactly as every existing test in
   `tests/test_lua_runner_write_csv.cpp` does (`LuaSandboxTest` fixture, `quiver::LuaRunner
   lua(db);`, verified: `tests/test_lua_runner.h:23-37`).
2. Call `lua.run(...)` **once** with a script that does `db:write_csv(...)`, `w:write_row(...)`,
   and returns **without** calling `w:close()`.
3. Call `lua.run(...)` **a second time**, on the **same, still-alive `lua`** object, with a script
   that calls `db:read_csv(path)` and returns the parsed rows (JSON-encoded by `LuaRunner::run`'s
   existing return-value encoder) — or asserts inline via Lua `assert(...)` the way every existing
   round-trip test in this suite already does (e.g. `tests/test_lua_runner_write_csv.cpp:110-121`,
   read this session).
4. Do **not** destroy `lua` between steps 2 and 3 — that is the entire point of the criterion
   ("asserted after `LuaRunner::run` returns, with the `LuaRunner` still alive and undestroyed").

**This two-`run()`-call shape has no precedent in the existing suite.** Every existing test in
`test_lua_runner_write_csv.cpp` (spot-checked several, including the pattern at lines 103-122 and
130-...) does the write, the explicit `w:close()`, **and** the `db:read_csv` round-trip all inside
one single `lua.run(...)` call/script. TEST-11 is a genuinely new test shape for this suite: two
separate `.run()` invocations against one live `LuaRunner`, because the entire behavior under test
(the flush happening *between* script executions, not inside one) cannot be observed any other
way.

### Legality of a second `run()` on the same runner

Case H of the Q1 probe (above) directly exercises this: one live `sol::state`, `safe_script` called
twice, with a `collect_garbage()` call after the first. The second script ran and returned
correctly (`42`), confirming a `collect_garbage()` call does not reset or corrupt the state for a
subsequent script execution. Nothing in `LuaRunner`'s implementation (`impl_->lua` is a single
long-lived member, `src/lua_runner.cpp:236`) prevents calling `.run()` multiple times; the existing
fixture imposes no such restriction either.

### Genuine-RED caveat

The ROADMAP claims "the file is zero bytes" without the fix. This is very likely true **for a small
fixture** (a header of 2-3 short names plus one or two short data rows — well under any
`std::filebuf`'s typical internal buffer, commonly a few KB), because nothing forces the
C++ runtime's stream buffer to flush to the OS before an explicit `flush()`/`close()`/destructor
call. **This was not independently executed as part of this research pass** (the Q1 probe used a
counter+printf destructor stand-in, not a real `std::ofstream`, and did not measure on-disk bytes).
Recommend the planner/executor treat this the same way this project always has for its
pre-implementation RED assertions (04-TOCHARS-PROBE.md, 04-SOL2-DISPATCH-PROBE.md, and the
project's own "9 genuinely failing tests" TDD precedent noted in STATE.md): **write TEST-11 first,
run it against the unmodified Phase-4 code, and confirm it actually fails** (ideally on an empty or
truncated file, not a coincidentally-passing one) before implementing the `collect_garbage()` fix.
Keep the fixture's row/header payload small and deterministic on purpose — a payload large enough
to spill past the stream's internal buffer would make the same test pass even without the fix,
silently defeating the RED. This is not a "hand it to a human" punt: it is the executor literally
running the test suite, which this project's own workflow already mandates at every TDD step.

## Q5 — Should the flush fire on `run()`'s error path too?

**Recommendation: yes, via one RAII guard around the whole function body — flagged for the
planner/user to confirm, since REQUIREMENTS' WRITE-06 wording is focused on the success case and
does not explicitly say either way.**

### The mechanism (verified cheapest-correct via the Q1 probe)

`LuaRunner::run` (`src/lua_runner.cpp:2165-2179`, read in full) has three exit points today: the
`throw` inside `if (!result.valid())`, the early `return {}` when `result.return_count() == 0`, and
the final `return out;` after JSON-encoding. Rather than duplicating a `collect_garbage()` call at
each of the three exits, a single RAII scope guard constructed **before** `safe_script` is called
fires its destructor on every one of those exits uniformly (normal return *and* exception
unwinding) with one line of new code:

```cpp
std::string LuaRunner::run(const std::string& script) {
    struct GcGuard {
        sol::state& lua;
        ~GcGuard() { lua.collect_garbage(); }
    } gc_guard{impl_->lua};

    auto result = impl_->lua.safe_script(script, sol::script_pass_on_error);
    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error(std::string("Failed to run Lua script: ") + err.what());
    }
    if (result.return_count() == 0) {
        return {};
    }
    std::string out;
    append_json(result.get<sol::object>(0), out, 0);
    return out;
}
```

Q1's Case G directly proves this ordering is safe: a `collect_garbage()` call executed immediately
before a `throw` (mirroring what happens during stack unwinding once `gc_guard` is destroyed)
correctly flushes the writer, and the exception is still caught and reported normally afterward. C++
destroys stack locals in reverse declaration order, so `gc_guard` (declared first) is destroyed
*after* `result` (declared second) during unwinding — meaning any reference `result` itself might
hold into the Lua stack is released before `collect_garbage()` runs, which is the safer order.

### Whether this is in scope

ROADMAP's WRITE-06 text ("A writer still open when `LuaRunner::run` returns is flushed to disk...")
and REQUIREMENTS' WRITE-06 row both describe the *success* case explicitly ("the script never
called `close()`"); neither explicitly excludes or includes a script that errors out mid-way with a
writer still open. The RAII approach costs **less** code than scoping the guard to only the
non-throwing paths (which would require either duplicating the call or wrapping only the tail of
the function in a nested scope), so "cheapest correct mechanism" argues for applying it uniformly
regardless of the answer to the scope question. **This is a real decision, not a technicality** —
flag it for the user/planner to confirm rather than assume, because a script that errors deliberately
mid-write might reasonably be expected to leave nothing usable behind, and a project reviewer could
read WRITE-06 either way.

## Q6 — DOC-06's concrete edit list

**Verified, HIGH confidence** — every location below was read this session.

| File | What changes | Verified location |
|---|---|---|
| `CLAUDE.md` (root) | The `db:read_csv` design-decision bullet ends "...writing (`db:write_csv`) is not exposed; a script's parsed rows go through the existing group writers." This sentence is **stale** since Phase 4 shipped `db:write_csv` and must be corrected/replaced with a statement of what the writer now does, plus the FMT-07/WRITE-06 decisions (header-as-width-authority pad/throw rule, the `collect_garbage()`-driven flush) | `CLAUDE.md:234-237` (read verbatim this session) |
| `CLAUDE.md` (root) | The cross-layer table has a "CSV file read" row (`db:read_csv()` / `db:read_csv_stream()`) but **no "CSV file write" row** for `db:write_csv()` / `w:write_row()` / `w:close()` | `CLAUDE.md:634` (only read-side row present, confirmed via grep) |
| `CHANGELOG.md` | The `## [0.10.7] — unreleased` section's `### Added` bullet says "...reading is the only direction, `db:write_csv` is not exposed" — **stale**, needs correction now that it is exposed (shipped Phase 4) plus a new `### Added` bullet for the writer itself (streaming-only, two options, truncate-at-open, hand-rolled/no-new-dependency, `to_chars` number formatting, `nil`/`""` indistinguishability) and Phase 5's own additions (header-as-width-authority, the unclosed-writer flush) | `CHANGELOG.md:8,32` (read verbatim this session) |
| `src/CLAUDE.md` | The existing `csv_write.h`/`csv_write.cpp` paragraph (which already documents D-34/D-37/D-40) needs one or two added sentences: where FMT-07's width enforcement lives (per Q2, in `lua_runner.cpp`'s `CsvWriter`, not `Writer`) and the `collect_garbage()`-at-`run()`'s-end guarantee (belongs near the existing `## LuaRunner` bullet list, close to where `run` returning JSON and the two return-value caps are already documented) | Existing paragraph confirmed present in the file already provided this session |

**Version-mismatch note (not in scope, do not touch):** `CHANGELOG.md`'s unreleased section is
already `0.10.7` while all five version manifests are at `0.10.6`. Phase 4's D-33 deliberately left
this alone ("no version bump happens in this milestone either"), and STATE.md carries the same
note as a standing, accepted, pre-existing mismatch. **Recommendation: Phase 5 should not touch
version numbers either**, even though it is editing `CHANGELOG.md` anyway for unrelated reasons —
this is a carried decision, not an open question, and resolving it is the "Bump Version" workflow's
job at actual release time, per root `CLAUDE.md` § Versioning.

**Escalation candidate — is `bindings/js/src/lua-api.ts` (DOC-05's file) also in scope?**
DOC-06's REQUIREMENTS text names only `src/CLAUDE.md`, root `CLAUDE.md`, and `CHANGELOG.md` — not
the agent-facing `LUA_DB_API_REFERENCE`. But FMT-07 (pad-short/throw-long) and WRITE-06 (flush
without `close()`) are both **behavior a script author calling `w:write_row`/omitting `w:close()`
needs to know** — comparable in kind to WRITE-08's truncate-at-open note, which DOC-05 *does*
already carry. `lua-api-sync.test.ts`'s build gate will not force this (it only checks that bound
`db:`/`quiver.*` **names** are documented, not that behavioral nuances are current — no new names
are added in Phase 5). **Flag this for the planner/user to decide**: either (a) REQUIREMENTS.md's
DOC-06 scope is complete as written and `lua-api.ts` is deliberately left as-is (in which case the
reference will describe a subtly wrong contract — a short row silently accepted-and-padded, not
mentioned at all today), or (b) DOC-06's scope should be read to implicitly include it, consistent
with the project's demonstrated diligence elsewhere. This is a scope question, not a technical one.

## Standard Stack

No new external dependency. This phase reuses everything Phase 4 already vendored (sol2 3.5.0,
Lua 5.4.8) and adds no library. `sol::state::collect_garbage()` is part of sol2 3.5.0's existing
public API (already linked and in use throughout `lua_runner.cpp`); no version bump or new
`FetchContent` entry is needed.

## Package Legitimacy Audit

Not applicable — no new external package is installed by this phase.

## Architecture Patterns

### Data flow for a ragged row (FMT-07)

```
Lua script: w:write_row({1, 2})            [table has 2 entries; header declared 3 names]
        |
        v
CsvWriter::write_row lambda (src/lua_runner.cpp)
        |
        +--> csv_row_cells_from_lua(row, "write_row", row_index)   [FMT-08: max-integer-key walk]
        |         -> cells = ["1", "2"]          (size 2)
        |
        +--> NEW: compare cells.size() against self.header_width (3)
        |         cells.size() < header_width  -> cells.resize(header_width)   (pad: ["1","2",""])
        |         cells.size() > header_width  -> throw "Cannot write_row: row <N> has <M> cells
        |                                          but header declares <W>"     (rows already
        |                                          written to disk are untouched -- Writer never
        |                                          sees the rejected row)
        |
        v
self.writer.write_row(cells, "write_row")   [csv_write::Writer -- UNCHANGED, just formats
                                              whatever vector<string> it is given]
        |
        v
append_record(cells, separator, out)        [FMT-01/02/03 -- unchanged; a padded N>=2 row is
                                              never the "lone empty cell" shape, so it is written
                                              as N-1 bare separators, not the blank-line defense]
        |
        v
out_ << out                                 [written to disk]
```

### Data flow for the unclosed-writer flush (WRITE-06)

```
Lua script: local w = db:write_csv(path, {...}); w:write_row({...})   [no w:close() call]
        |
        v
LuaRunner::run(script)
        |
        +--> GcGuard constructed (holds a reference to impl_->lua)
        |
        +--> impl_->lua.safe_script(script, ...)   [script returns; `w` goes out of scope as the
        |                                            main chunk's only local -- now unreachable]
        |
        +--> (any of: throw / early return{} / JSON-encode-and-return)
        |
        v
GcGuard::~GcGuard() fires (RAII, on every exit path)
        |
        +--> impl_->lua.collect_garbage()          [lua_gc(L, LUA_GCCOLLECT) -- a FULL cycle,
        |                                            which runs pending finalizers before
        |                                            returning -- verified this session]
        |
        v
CsvWriter's sol2-generated __gc metamethod runs synchronously
        |
        v
CsvWriter's destructor runs -> quiver::csv_write::Writer's destructor runs
        |
        v
Writer::~Writer(): "if (!closed_ && out_.is_open()) { out_.close(); }"   [flushes + closes]
        |
        v
File on disk is now complete and re-readable via a SECOND lua.run("... db:read_csv(path) ...")
```

### Anti-Patterns to Avoid

- **Do not add a `row_index` parameter to `csv_write::Writer::write_row`** to support FMT-07's
  error message. The ordinal already lives in `CsvWriter`; plumbing it into `Writer` duplicates
  state for no caller that needs it (see Q2).
- **Do not duplicate `collect_garbage()` calls at each of `run()`'s three exit points.** A single
  RAII guard is fewer lines and covers all of them uniformly (see Q5).
- **Do not reach for a `weak_ptr` registry or a new `Database::log_warning`** to "detect" the
  unclosed writer for diagnostic purposes — that whole mechanism was evaluated and declined in
  Phase 4 (root REQUIREMENTS § Out of Scope: "A warning when a writer is left unclosed"). WRITE-06
  is the flush **alone**.

## Don't Hand-Roll

Nothing new to hand-roll or avoid hand-rolling in this phase — both additions are small, local
changes to code this project already owns (`csv_write::Writer`'s caller, and `LuaRunner::run`'s own
body). No parsing, no serialization format, no new external-facing surface.

## Common Pitfalls

### Pitfall 1: Trusting the "one collect_garbage() call" claim without executing it

**What goes wrong:** Assuming Lua 5.4's incremental collector behaves like a fully-deferred
generational collector and requires two or more `collect_garbage()` calls (one to mark, one to
finalize), leading to an over-engineered fix (a loop calling `collect_garbage()` until a counter
stabilizes) or, worse, shipping a fix that "usually" works and occasionally leaves TEST-11 flaky.
**Why it happens:** `lua_gc(L, LUA_GCCOLLECT)`'s exact finalizer-timing guarantee is easy to get
wrong by reasoning from generational-GC intuition instead of reading (or executing against) Lua
5.4's specific incremental-collector semantics.
**How to avoid:** This research already executed the probe (Q1) confirming one call suffices for
every reachability shape tested. Re-run it if the sol2/Lua vendor version changes.
**Warning signs:** TEST-11 passing on some runs and not others in CI would be the symptom; it was
not observed in six repeated executions this session.

### Pitfall 2: Writing TEST-11's fixture too large

**What goes wrong:** A test fixture whose header + rows exceed the C++ runtime's internal stream
buffer will show *some* bytes on disk even without the WRITE-06 fix (because the buffer spilled on
its own), making the RED phase accidentally pass and hiding a broken fix.
**Why it happens:** `std::ofstream`/`std::filebuf` buffers internally; a buffer flush is triggered
by size, not just by an explicit `flush()`/`close()`.
**How to avoid:** Keep TEST-11's script tiny — a 2-3 column header, one or two short data rows —
and confirm the test is RED against the unmodified Phase-4 code before adding the fix (see Q4).
**Warning signs:** The test passes before the fix is implemented.

### Pitfall 3: Padding after (rather than before) the FMT-02 blank-line check

**What goes wrong:** If a short row is padded to N cells *after* `append_record`'s
`lone_empty_cell` determination has already run once on the original (unpadded, zero-cell) vector,
the wrong branch could be taken.
**Why it happens:** `append_record` (`src/csv_write.cpp:57-87`) is a pure function of whatever
vector it receives — there is no way for it to know a row was "originally shorter." Padding
**must** happen entirely before `Writer::write_row` is called at all (Option B in Q2 already
guarantees this ordering; a hypothetical Option A that padded inside `Writer` after already having
called into `append_record` would need to reorder carefully).
**How to avoid:** Pad in `lua_runner.cpp`, immediately after `csv_row_cells_from_lua` returns and
before `self.writer.write_row(...)` is called — never inside `Writer`/`append_record`.
**Warning signs:** TEST-10's short-row fixture reading back as a quoted `""` blank line instead of
an N-field row of empty cells.

## Code Examples

See the "Suggested shape" code block under Q2 (row-width enforcement) and the `GcGuard` code block
under Q5 (unclosed-writer flush) — both are illustrative sketches grounded in the exact,
already-read source this session, not independently executed as production code (only the GC
*mechanism* itself was executed, via the standalone probe, not this exact class).

## State of the Art

No external state-of-the-art shift applies — this is a two-item internal fix to code shipped four
days ago in the same milestone. N/A.

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | Exact wording of the new FMT-07 Pattern 1 message (`"Cannot write_row: row <N> has <M> cells but header declares <W>"`) | Q2 | Low — TEST-12-style tests assert on this project's own newly-chosen wording, so the planner/executor picks the exact phrasing and the test asserts it; no external contract depends on the specific words, only that it is Pattern 1 and names the ordinal + both counts |
| A2 | TEST-11's fixture will actually produce a **zero-byte** file pre-fix (rather than merely truncated/incomplete) | Q4 | Low-medium — if the OS/runtime flushes partially, the test might need to assert "incomplete" rather than "zero bytes"; either way it is RED pre-fix as long as the fixture stays small, but the exact on-disk byte count was not measured this session |
| A3 | The flush should also fire on `run()`'s error/throw path (Q5's recommendation) | Q5 | Medium — if the project intends WRITE-06 to cover only the success path, applying the RAII guard unconditionally is a (harmless but) unrequested behavior expansion; flagged for explicit confirmation |
| A4 | `bindings/js/src/lua-api.ts` should also be updated for FMT-07/WRITE-06 even though DOC-06's requirement text doesn't name it | Q6 | Medium — if left unupdated, the agent-facing reference will describe a subtly incomplete contract for `w:write_row`/omitted `w:close()`; if updated without confirming scope, it is unrequested extra work outside the stated requirement |

## Open Questions

1. **Exact phrasing of the FMT-07 too-long-row message.**
   - What we know: it must be Pattern 1, must name the operation (`write_row`), the row ordinal,
     and both counts (actual cell count and header width).
   - What's unclear: the precise sentence — several equally-valid phrasings exist (see A1).
   - Recommendation: the planner picks one and pins it in `csv_write.cpp`'s TEST-12 catalogue
     comment block in the same change that adds the test asserting it.

2. **Should `bindings/js/src/lua-api.ts` be touched by this phase?**
   - See A4 / the Q6 escalation note. Recommend surfacing this to the user before planning, since
     it changes the phase's file-touch footprint beyond REQUIREMENTS' literal DOC-06 text.

## Decisions the planner must escalate rather than assume

1. **Does WRITE-06's flush apply to `run()`'s error/throw path, or only the success path?**
   (Q5) — REQUIREMENTS' wording is silent; the cheapest-correct implementation (one RAII guard)
   naturally covers both, but that is an implementation-convenience argument, not a requirements
   answer. Confirm with the user/reviewer before assuming uniform coverage is wanted.

2. **Is `bindings/js/src/lua-api.ts` (DOC-05's file) in scope for this phase's documentation
   work, even though DOC-06's REQUIREMENTS text names only `src/CLAUDE.md`, root `CLAUDE.md`, and
   `CHANGELOG.md`?** (Q6) — FMT-07/WRITE-06 change behavior a script author needs to know, but the
   phase's own requirement list doesn't ask for the agent-facing reference to be touched.

3. **Exact wording of the new Pattern 1 message for a too-long row** (Q2/Open Question 1) — not
   a blocking decision, but the planner should pick and pin it deliberately rather than let it
   drift from an ad-hoc executor choice.

## Security Domain

`security_enforcement` is on (`.planning/config.json`). This phase's surface is narrow: no new
file path, no new network/auth boundary, no new external input format. The sandbox
(`resolve_sandboxed_path`) and the untrusted-script threat model it defends against are unchanged
by this phase.

### Applicable ASVS Categories

| ASVS Category | Applies | Standard Control |
|---------------|---------|-------------------|
| V2 Authentication | No | N/A — no auth surface in this feature |
| V3 Session Management | No | N/A |
| V4 Access Control | No | N/A — sandbox containment is Phase 4's `resolve_sandboxed_path`, unchanged |
| V5 Input Validation | Yes | FMT-07's row-width check is itself an input-validation rule (reject a too-long row rather than silently misaligning data); reuses the existing Pattern 1 error channel, no new validation library needed |
| V6 Cryptography | No | N/A |

### Known Threat Patterns for this stack

| Pattern | STRIDE | Standard Mitigation |
|---------|--------|----------------------|
| Untrusted-script resource exhaustion via an oversized integer table key (already-known CR-01 from Phase 4's code review, still open/deferred, not this phase's job to fix) | Denial of Service | A `kMaxReturnDepth`/`kMaxReturnBytes`-style ceiling on `max_index`, same pattern already used for the JSON return encoder. FMT-07's padding (`cells.resize(header_width)`) does **not** introduce a new instance of this — `header_width` comes from the already-decoded `header` option, which is itself already subject to CR-01's existing (still-open) gap, not a new one this phase creates |
| A script leaving a file descriptor open indefinitely (the pre-Phase-5 bug WRITE-06 fixes) | Denial of Service (resource leak) | The `collect_garbage()`-driven flush this phase adds; no new resource-tracking machinery needed since ownership was already `unique_ptr` + sol2 GC (LUA-11) |

## Sources

### Primary (HIGH confidence — read or executed this session)

- `src/csv_write.h`, `src/csv_write.cpp` — full read, this session
- `src/lua_runner.cpp` (lines 1-260, 254-384, 680-730, 2140-2179) — full read, this session
- `src/CLAUDE.md`, root `CLAUDE.md`, `CHANGELOG.md` — full/targeted read, this session
- `tests/test_lua_runner.h`, `tests/test_lua_runner_write_csv.cpp` (lines 95-140) — read, this session
- `.planning/phases/04-a-lua-script-writes-a-csv-file/{04-CONTEXT.md,04-VERIFICATION.md,04-REVIEW.md,04-SOL2-DISPATCH-PROBE.md,04-TOCHARS-PROBE.md}` — full read, this session
- `.planning/{REQUIREMENTS.md,STATE.md,ROADMAP.md}` — full read, this session
- `gc_probe.cpp` — written, compiled (`cl /std:c++20 /EHsc /MDd`), and executed this session
  against this repo's own `build/_deps/sol2-src`/`build/_deps/lua-build/src/lua-5.4d.lib`; six
  reachability cases plus a same-state double-`run()` case, all confirming the one-call claim

### Secondary (MEDIUM confidence)

- Lua 5.4 reference manual's description of `lua_gc(L, LUA_GCCOLLECT)` as performing a full
  collection cycle (not independently re-fetched this session; consistent with the executed
  probe's observed behavior, so treated as corroborating rather than sole evidence)

### Tertiary (LOW confidence)

None — every substantive claim in this document is either a direct source read or an executed
probe.

## Metadata

**Confidence breakdown:**
- Q1 (GC flush mechanism): HIGH — executed, not merely read
- Q2 (enforcement placement): HIGH — verified against actual shipped source
- Q3 (padding/FMT-02 interaction): HIGH — traced against actual `append_record` source
- Q4 (TEST-11 shape): HIGH for the shape; MEDIUM for the exact "zero bytes" byte-count claim (not
  independently measured this session — flagged as a RED-first verification step for the executor)
- Q5 (error-path flush): design recommendation, escalated rather than asserted
- Q6 (DOC-06 edit list): HIGH — every file/line read this session

**Research date:** 2026-09-17
**Valid until:** Should remain valid through this phase's execution (no external dependency drift
risk); re-verify the GC probe if the sol2/Lua vendor version changes before this phase lands.
