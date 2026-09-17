# Phase 5: Ragged rows and forgotten closes - Context

**Gathered:** 2026-09-17
**Status:** Ready for planning
**Mode:** Smart discuss (autonomous) — three grey areas proposed as tables, all accepted as
recommended. Three of the twelve questions had already been answered by the user directly during
the preceding `/gsd-plan-phase 5` run (marked **user-decided** below); the rest were proposed from
`05-RESEARCH.md` and accepted.

<domain>
## Phase Boundary

The two ways a Phase-4 file is wrong even though every row was accepted: a row that does not match
the declared `header`, and a script that returns without calling `w:close()`. Plus the milestone
documentation record.

**In scope:** FMT-07, WRITE-06, TEST-10, TEST-11, DOC-06.

**Out of scope — declined for the whole milestone, do not reintroduce as an implementation
detail:** the unclosed-writer *warning* (WRITE-06 is the flush **alone**), a destructive-overwrite
guard, a whole-file `write_csv` form, atomic write-then-rename, and every option beyond
`separator` / `header`. Each is argued down individually in `.planning/REQUIREMENTS.md`
§ Out of Scope.

**Out of scope — untouched by decision:** the `CHANGELOG.md` `0.10.7`-vs-manifests-`0.10.6`
mismatch (Phase 4's D-33, carried).

</domain>

<decisions>
## Implementation Decisions

### Ragged-row enforcement (FMT-07)

- **D-41: The width check lives in the Lua layer**, in the `write_row` usertype lambda in
  `src/lua_runner.cpp` (:705-720), spliced between `csv_row_cells_from_lua` and
  `Writer::write_row`. `csv_write::Writer`'s public interface is **untouched** by this phase. The
  row ordinal already lives on the Lua side (`CsvWriter::next_row_index`, :261) and FMT-05's
  existing non-finite message already uses it; putting the check in `Writer` would force it to
  start retaining the header *and* grow a duplicate row counter, for no caller that exists.
  — **Reversibility:** reversible — the check is a contiguous block in one lambda.

- **D-42: `CsvWriter` gains a `std::size_t header_width`**, set from `csv_options.header.size()`
  at construction. `0` means no header and therefore no enforcement — unambiguous, because
  Phase 4 already settled that `header = {}` means "no header row" (04-CONTEXT discretion item).
  Not the full header vector: only the count is ever read.

- **D-43: The ordinal in the too-long error is `next_row_index`** — 1-based, counting the first
  **data** row as 1, with the header record never counted. This matches FMT-05's existing message
  exactly. The header is written inside `Writer`'s constructor (`src/csv_write.cpp:119-126`) and
  never passes through `write_row`, so it cannot touch the counter. A rejected row leaves the
  ordinal unchanged, per Phase 4's existing rule.

- **D-44: Padding happens before the cell vector reaches `append_record`** — i.e. entirely in
  `lua_runner.cpp`, so `src/csv_write.cpp` needs no functional change. Consequence, verified by
  tracing `append_record`'s `lone_empty_cell` predicate (`src/csv_write.cpp:57-87`): under an N≥2
  header a zero-cell row pads to N empty cells and is emitted as N-1 bare separators — a
  legitimate N-field row, **not** FMT-02's quoted `""`. Under a 1-column header it pads to exactly
  one empty cell and **does** still hit FMT-02's blank-line defence. Both are correct; the TEST-09
  fixture is unaffected.

- **D-45: The exact wording of the too-long-row Pattern 1 message is Claude's discretion**
  (user delegated). Pin it deliberately against the neighbouring FMT-05 message and add it to the
  pinned catalogue comment block at the top of `src/csv_write.cpp` in the same change — that block
  is the one place TEST-12's message strings are enumerated. It must name the row ordinal and both
  counts (ROADMAP criterion 2).
  — **Reversibility:** costly — TEST-10 matches on the message.

### The unclosed-writer flush (WRITE-06 / TEST-11)

- **D-46: One RAII scope guard, declared before `safe_script`, wrapping the whole body of
  `LuaRunner::run`** (`src/lua_runner.cpp:2165-2179`). Its destructor calls
  `impl_->lua.collect_garbage()`. Not three separate calls at the three exits. **user-decided.**

- **D-47: The flush covers `run()`'s error path too.** `run` throws before reaching any
  end-of-function cleanup, and the guard fires during unwinding as well as on both normal returns.
  A script that errors mid-write leaves a readable partial file rather than a zero-byte one.
  **user-decided.** Ordering is deliberate and safe: C++ destroys stack locals in reverse
  declaration order, so the guard (declared first) runs *after* `result` (declared second) is
  destroyed — the Lua stack reference is released before `collect_garbage()` runs.

- **D-48: One `collect_garbage()` call, not two.** The researcher compiled and ran a standalone
  probe against this repo's own vendored `sol2-src`/`lua-build`: one call synchronously runs a
  `unique_ptr`-owned usertype's destructor for every reachability shape tested — bare local in the
  main chunk, nested in a table, no binding at all, the error path, and a second `run()` on the
  same live state. This is **executed evidence, not standards-text reading** — the distinction
  that the `std::to_chars` blocker in STATE.md exists to enforce.

- **D-49: TEST-11 must be observed RED before the fix, with the actual byte count recorded.**
  The ROADMAP asserts the unflushed file is "zero bytes"; research rates that MEDIUM — a
  `std::ofstream` may already have flushed part of its buffer, which would make a small fixture
  red but a large one accidentally green. Keep the fixture small, run the test before the fix
  lands, and write the number actually observed into the plan summary rather than repeating the
  claim.

### The documentation record (DOC-06)

- **D-50: `bindings/js/src/lua-api.ts` is in scope**, in addition to DOC-06's three literally named
  files. **user-decided.** FMT-07's pad-short/throw-long rule and WRITE-06's
  flush-without-`close()` are both behaviour a script author needs, and `LUA_DB_API_REFERENCE`
  documents neither today — it would otherwise describe a subtly wrong contract in every `claw`
  session. Note this is **not** forced by the build gate: `bindings/js/test/lua-api-sync.test.ts`
  checks only that bound `db:` *names* are documented, and Phase 5 binds no new name. Match the
  voice of that section's existing truncate-at-open paragraph (`bindings/js/src/lua-api.ts:724-728`)
  and keep the addition to clauses, not a second worked example — D-39's per-session token argument
  still holds.

- **D-51: The `CHANGELOG.md` work folds into the existing `## [0.10.7] — unreleased` section, and
  the five version manifests are not touched.** Two edits there: correct the stale sentence in the
  `### Added` bullet that still says "reading is the only direction, `db:write_csv` is not exposed"
  (false since Phase 4 shipped), and add the writer's own entry covering both phases —
  streaming-only, the two options, truncate-at-open, hand-rolled with no new dependency,
  `to_chars` number formatting, `nil`/`""` indistinguishability, header-as-width-authority, and
  the unclosed-writer flush. The `0.10.7`-vs-`0.10.6` mismatch is Phase 4's carried D-33 and stays;
  bumping is the Bump Version workflow's job at release, per root `CLAUDE.md` § Versioning.

- **D-52: Root `CLAUDE.md` gets both edits** — correct the stale
  "writing (`db:write_csv`) is not exposed" clause at `:235`, **and** add the missing "CSV file
  write" row to the cross-layer table beside the existing "CSV file read" row at `:634`
  (`N/A` in every column but Lua, matching its neighbour).

- **D-53: The writer extends the existing `db:read_csv` Design Decisions bullet** rather than
  getting its own. That bullet already frames the pair as one decision and is the exact place
  carrying the stale clause D-52 corrects.

### Claude's Discretion

- D-45's exact message text (user delegated explicitly).
- Task and wave decomposition, and whether TEST-10 and TEST-11 share a plan.
- Whether the WRITE-06 probe is preserved as a phase artifact the way Phase 4 preserved
  `04-tochars-probe.cpp` / `04-sol2-dispatch-probe.cpp`. It is **not** to be added to the test
  suite; TEST-11 is the shipped guarantee.

</decisions>

<code_context>
## Existing Code Insights

Full line-anchored excerpts in `05-PATTERNS.md`; the load-bearing facts:

### Reusable Assets
- **`CsvWriter::next_row_index`** (`src/lua_runner.cpp:261`) — the 1-based next-data-row ordinal,
  already threaded into FMT-05's message. D-43 reuses it; do not add a second counter.
- **The `write_row` usertype lambda** (`src/lua_runner.cpp:705-720`) — already holds the
  closed-writer guard and the FMT-08 integer-key pass. The FMT-07 block goes here, and the
  existing FMT-05/FMT-08 checks in the same lambda are its exact analog.
- **`append_record`** (`src/csv_write.cpp:57-87`) — its `lone_empty_cell` predicate is what makes
  D-44's before/after ordering matter. It needs **no change**.
- **The pinned TEST-12 message catalogue** (comment block at the top of `src/csv_write.cpp`) —
  the one place every Pattern 1 string for this feature is enumerated. D-45's new message is added
  there in the same change. This is the only edit `src/csv_write.cpp` receives: the mapper
  confirmed no functional code in that file changes.

### Established Patterns
- **Pattern 1 everywhere** — `"Cannot {operation}: {reason}"`, `{operation}` being the public Lua
  method the script called (D-36, Phase 4). FMT-07's message says `write_row`.
- **RAII guards** — `TransactionGuard` is the nearest structural precedent for D-46, but it guards
  a SQL transaction, not GC. The mapper flags it as a spirit-only analog, not a copy source: no
  prior code in this repo drives Lua GC from a scope guard.
- **Round-trip-through-`db:read_csv`** — every correctness assertion. This project has named the
  weak-test trap three times (`export_csv` has 118 export-side string-search tests that never
  re-import). A grep over the output file proves the emitter emitted, not that the file reads.

### Integration Points
- `src/lua_runner.cpp` only, for both code changes. No C API, no FFI, no new dependency, no public
  `include/quiver/` header — `db:write_csv` rides inside the already-bound generic
  `LuaRunner::run` path.
- `tests/test_lua_runner_write_csv.cpp` + the `tests/test_lua_runner.h` fixture.
- **No analog for TEST-11's shape:** no existing test in the suite calls `.run()` twice on one
  live `LuaRunner`. Write the sequencing fresh from neighbouring tests' fixture conventions; the
  second `run()` is what reads the file back through `db:read_csv` without destroying the runner.

</code_context>

<specifics>
## Specific Ideas

- **ROADMAP criterion 4 is a trap if read carelessly:** the assertion must happen with the
  `LuaRunner` still alive and undestroyed. Destroying it would pass on `Writer`'s destructor alone
  — which already closes the stream — and prove nothing about the flush.
- **FMT-07 and TEST-10 are inseparable, as are WRITE-06 and TEST-11.** A padded short row is only
  visible as a misalignment on read-back, and `db:read_csv`'s pinned `KEEP_NON_EMPTY` policy never
  pads; an unpadded ragged row comes back silently misaligned against its own header.
- **Release build must be exercised separately.** `SOL_SAFE_GETTER` is off in Release and has
  hidden Lua marshalling bugs before.
- The spec-less edge probe surfaced 7 applicable edges across the 5 requirements (FMT-07
  empty/encoding, WRITE-06 concurrency, TEST-10 boundary/precision, TEST-11 concurrency, DOC-06
  unclassified), all `unresolved`. The planner resolves each into `must_haves` — none may be
  silently dropped.

</specifics>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

- `.planning/phases/05-ragged-rows-and-forgotten-closes/05-RESEARCH.md` — Q1's executed probe
  result, the Q2 placement argument, the Q3 `append_record` trace, Q4's RED caveat, and Q6's
  file/line edit list.
- `.planning/phases/05-ragged-rows-and-forgotten-closes/05-PATTERNS.md` — line-anchored excerpts
  per file, plus the two explicit "no analog" call-outs.
- `.planning/REQUIREMENTS.md` — the 29 v1.1 requirements; § Out of Scope argues down every
  declined option individually.
- `.planning/ROADMAP.md` § Phase 5 — the five success criteria and the two inseparability notes.
- `.planning/phases/04-a-lua-script-writes-a-csv-file/04-CONTEXT.md` — D-34 through D-40, all
  still binding.
- `.planning/STATE.md` § Accumulated Context and § Blockers/Concerns.
- Root `CLAUDE.md` § C++ Error Message Patterns (the three Pattern shapes) and § Design Decisions.

</canonical_refs>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope.

</deferred>
