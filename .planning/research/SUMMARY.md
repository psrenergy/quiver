# Project Research Summary

**Project:** Quiver — milestone v1.1, CSV writing for the Lua runner (`db:write_csv`)
**Domain:** C++20 library feature — a streaming CSV writer bound into an embedded, sandboxed Lua scripting surface
**Researched:** 2026-09-16
**Confidence:** HIGH, with one unresolved architectural contradiction (see below) and one MEDIUM-confidence numeric claim

> **Self-heal note (#222):** the synthesizer returned this document inline instead of writing it. The
> orchestrator persisted it. Content is the synthesizer's, restructured onto the template and with its
> fabricated work-day estimates removed — no researcher produced effort estimates, so they were not
> grounded in anything.

## Executive Summary

Build the writer by hand. Roughly 60–90 lines of RFC-4180 emission over `std::ofstream`, using the
`std::to_chars` numeric formatting already present in `src/lua_runner.cpp`. **No new dependency, no
`cmake/Dependencies.cmake` change, no `FetchContent_Declare`.** The already-vendored
`vincentlaucsb/csv-parser` was evaluated for reuse and rejected on two independently sufficient
grounds, both verified by reading `build/_deps/csv_parser-src/include/internal/csv_writer.hpp`
directly: its `DelimWriter<OutputStream, Delim, Quote>` takes the delimiter and quote character as
**compile-time template parameters** (line 254), which cannot serve Quiver's runtime `separator`
option; and its floating-point `to_string` truncates to a hardcoded **`DECIMAL_PLACES = 5`**
(line 25), the same bug class this project already fixed once. Its quoting/escaping *is* correct
RFC-4180 (lines 555–590), but that is about twenty lines — too little to justify pulling three more
headers into the build. No csv-parser release exists past the pinned 5.3.0 that fixes either issue.

The feature itself is narrow and its shape is already decided. What carries real risk is not the
CSV emission — that is well-understood — but three things around it: **the writer handle's ownership
and lifetime** (an unresolved contradiction between two researchers, detailed below), **the ways a
write path destroys data that v1.0's read path could not**, and **a recurring numeric-formatting bug
class this project has now hit three separate times**.

The cheapest meaningful mitigation is that the correctness test must be a **round trip through the
already-verified `db:read_csv`**, not a string search over the output file. This project has twice
named its own weak-test trap — `export_csv` has 118 tests that are export-side string searches and
never re-import — and a writer suite built the same way would make it three.

---

## OPEN DECISION — writer handle ownership and lifetime

**This is unresolved and must be settled before the implementing phase is planned. Neither position
is wrong; they answer different readings of "flushed and closed at script end."**

### Position A — `unique_ptr`, sol2's default `__gc` (STACK.md)

Copy `db:open_file` exactly. It returns `std::unique_ptr<BinaryFile>` with `sol::no_constructor`
and no explicit `__gc` (`src/lua_runner.cpp:431-441`, `:565-579`); sol2's default garbage-collect
metamethod runs the C++ destructor, which covers both a forgotten `w:close()` and `lua_close`
teardown.

- **For:** simplest, zero new machinery, and it is the pattern already in this exact file.
- **Against:** no warning can be emitted. Logging from a `__gc` context during teardown is unsafe,
  so a forgotten `close()` closes silently — which is not the decided behaviour.

### Position B — `shared_ptr` + `weak_ptr` registry, swept at `run()`'s exit (ARCHITECTURE.md, agreed by PITFALLS.md)

`std::shared_ptr<Writer>` handed to Lua, with a `std::vector<std::weak_ptr<Writer>>` registry on
`LuaRunner::Impl`. At the end of `LuaRunner::run()` — where `db` is still valid and exceptions are
safely catchable — walk the registry, flush/close anything still open, and log the warning there.
The destructor stays a silent, non-throwing safety net underneath.

- **For:** Lua 5.4's reference manual guarantees finalization only at `lua_close`. `__gc` firing at a
  mid-script error is **not** guaranteed. In this codebase `lua_close` means "whenever `LuaRunner` is
  destroyed," which is later than "script end." Position A therefore cannot deliver the decided
  behaviour on the error path — the case that matters most.
- **For:** PITFALLS.md reached the same conclusion independently — a throwing `__gc` is undefined
  behaviour, so the sweep belongs at `run()`'s boundary regardless.
- **Against:** more machinery, and it needs a logger that does not currently exist (below).

### Consequence either way

**`LuaRunner` has no logger.** `Database`'s per-instance `spdlog` logger is private with no public
accessor — `include/quiver/database.h` has none. "Log a warning" therefore requires a new
`Database::log_warning(...)` forwarder. This is a required new touch point, not incidental plumbing,
and it only arises under Position B.

---

## Key Findings

### Recommended Stack

Nothing is added. The writer is hand-rolled against the standard library and the codebase's own
existing helpers.

**Core technologies:**
- `std::ofstream` — file I/O and buffering. No manual batch buffer is needed; `DelimWriter`'s
  internal 64 KB `batch_buffer_` exists only because it supports non-file streams Quiver does not use.
- `std::to_chars` — numeric formatting, shortest round-trip. `append_number` at
  `src/lua_runner.cpp:129` is the existing implementation to reuse, not re-derive.
- sol2 3.5.0 usertype — the handle binding, mirroring `BinaryFile` at `src/lua_runner.cpp:431-441`.
- **Rejected:** `csv::DelimWriter` (compile-time delimiter at `csv_writer.hpp:254`; 5-decimal float
  truncation at `:25`). **Rejected:** any new FetchContent dependency.

`/bigobj` is confirmed scoped to `lua_runner.cpp` alone (`src/CMakeLists.txt:68`) and is irrelevant
here either way, since nothing template-heavy is being introduced.

### Expected Features

Surveyed against Python's `csv`, Rust's `csv`, Go's `encoding/csv`, papaparse, and Julia's `CSV.jl`.

**Must have (table stakes):**
- Minimal quoting — quote only on separator, quote char, CR or LF. **Unanimous** across all five;
  no surveyed writer defaults to quote-everything.
- Doubled internal quotes (RFC 4180).
- A record terminator after **every** record including the last. RFC 4180 makes the final break
  optional; every real writer emits it anyway.
- Header written once, on construction.
- Strict row width — mismatch is an error.

**Should have:**
- Idempotent `close()`; a Pattern 1 error on `write_row` after `close`.

**Defer / reject (each argued down individually against the permanent `LUA_DB_API_REFERENCE`
prompt-payload cost):** `append`, a `line_ending` toggle, quote-char and quoting-mode options,
dialect presets, an encoding option, a BOM toggle, `is_open()`, a row-count getter, and atomic
write-then-rename. The last one is not merely low-value — it **actively conflicts** with the already
decided "partial file survives, with a warning" behaviour.

**Line terminator:** no consensus exists (Python and papaparse default CRLF; Go and Rust default LF,
explicitly diverging from RFC 4180). That absence of consensus is precisely why it is safe to expose
no knob. Pick **LF** — Quiver's own reader already accepts both.

**BOM:** opt-in or absent everywhere except Excel's own export path. Never emit one, no knob.

### Architecture Approach

`src/csv_write.h` / `.cpp` mirroring `src/csv_read.h` / `.cpp`. Under the hand-rolled recommendation
the split is stylistic rather than forced — the Pimpl rationale behind `csv_read::Reader` was to keep
csv-parser's headers out of the sol2 translation unit, and a zero-dependency writer has no such
headers to hide.

**Data flow for one `write_row`:** Lua table → sol2 type dispatch (`nil` → empty, boolean → `"1"`/
`"0"`, int64 → `to_chars` integer overload, double → `to_chars`, string → verbatim, anything else →
throw) → RFC-4180 quoting → `std::ofstream`.

**Type dispatch is load-bearing:** copy the `lua_table_to_value_map` / `table_to_element` pattern at
`src/lua_runner.cpp:1085-1150` verbatim, adding a `nil` branch first. Do **not** use
`lua_cell_as<T>` — that is for homogeneous arrays.

**`resolve_sandboxed_path` needs no change.** `std::filesystem::weakly_canonical` is already
existence-agnostic by design, so it is correct for a target that does not yet exist. Write-specific
preconditions (target is a directory, open-for-write failed) belong in `Writer`'s constructor — the
same layering `csv_read::Reader` already uses.

**No C API and no binding work.** `db:write_csv` rides entirely inside the already-bound generic
`LuaRunner::run()` / `quiver_lua_runner_run` path — confirmed by grepping all four FFI bindings.
Only `quiver_tests` gets real coverage.

**Build order (strict):** `csv_write` files → the `lua_runner` binding (plus `Database::log_warning`
under Position B) → `bindings/js/src/lua-api.ts` in the same commit, because the sync test is a hard
build gate → tests → docs.

### Critical Pitfalls

1. **Destructive overwrite — no guard exists anywhere in this codebase.** `BinaryFile`'s own `'w'`
   mode has no overwrite protection either, and reusing that posture is far more dangerous here
   because the milestone's entire purpose is writing into the *same case folder* as the live `.db`,
   `.qvr`, and migration files. `db:write_csv("study.db")` truncates the open SQLite database, with
   platform-divergent symptoms: silent corruption on POSIX, a loud lock error on Windows.
   **Prevention:** refuse an exact `db.path()` match in the binding.
2. **`std::to_chars` silently emits literal `inf` / `nan`** for a non-finite double (verified against
   cppreference's "as if by `printf`" Effects clause). This is the **third** occurrence of this
   project's recurring numeric bug class, and the first in this direction: no error, no truncation,
   just an unparseable token in a data cell. **Prevention:** an `std::isfinite` guard that throws.
3. **The int64-near-2^53 trap.** An int64 must reach `std::to_chars`'s **integer** overload directly.
   Routed through `double` first, `9007199254740993` silently becomes `9007199254740992`.
4. **The `lua-api-sync.test.ts` gate has a verified blind spot.** Its usertype-method coverage runs
   over a **hardcoded** `["BinaryFile", "BinaryMetadata", "Expression"]` array (line 75). A new
   `Writer` usertype's methods are parsed into the test's internal map but **never asserted** against
   the documentation unless that array is extended by hand. The "hard gate" would not catch an
   undocumented `w:write_row` or `w:close`.
5. **The weak-test trap, for the third time.** `export_csv`'s 118 tests are export-side string
   searches that never re-import. A writer suite built the same way repeats it. **Prevention:** round
   trip through `db:read_csv`.

Also identified: whole-float identity loss (does `2014.0` write as `2014`, and does that break a
downstream reader's type inference?), Lua division semantics, destructor GC unsafety, and the rule
that the writer must not hold a `Database&`.

---

## Implications for Roadmap

Suggested structure, mirroring v1.0's own mechanics → correctness → agent-facing-shape progression.

### Phase 1: Writer mechanics and the sandbox
**Rationale:** nothing else can be tested until a file can be written safely. The ownership decision
is foundational and blocks the lifecycle phase.
**Delivers:** `src/csv_write.h` / `.cpp` (quoting, header, strict width, file I/O); the
`db:write_csv` binding with the destructive-overwrite guard; **the settled ownership decision**.
**Avoids:** pitfalls 1 (destructive overwrite), 5 (sandbox), 7 (no `Database&` in the writer).

### Phase 2: Numeric and quoting correctness
**Rationale:** this is where the recurring bug class lives, and it is independently testable once a
file can be produced.
**Delivers:** numeric type dispatch; the `std::isfinite` guard; first-row-width logic; a round-trip
test suite through `db:read_csv`.
**First action:** the two-line empirical spot-check of `std::to_chars` on `0.0/0.0` and `1.0/0.0`.
The research verified this against standards text but could not execute a binary to confirm it.
**Avoids:** pitfalls 2, 3, 4, 9, 10.

### Phase 3: Lifecycle, documentation, hardening
**Rationale:** the lifecycle work depends on Phase 1's ownership decision; the documentation gate
must land with the binding it documents.
**Delivers:** the registry and `run()` sweep under Position B, plus `Database::log_warning`;
idempotent `close()` and the post-close guard; `bindings/js/src/lua-api.ts`, **the
`lua-api-sync.test.ts` hardcoded array extension**, the CLAUDE.md files, and CHANGELOG.
**Avoids:** pitfalls 6 (GC) and 8 (the sync-test blind spot).

### Phase Ordering Rationale

- The ownership decision gates Phase 3's registry work, so it must be settled in Phase 1.
- The numeric pitfalls are concentrated and independently testable, so they earn their own phase
  rather than being smeared across the other two.
- The documentation gate must land in the same phase as the binding — `lua-api-sync.test.ts` is a
  hard build gate, and v1.0 already learned this.

### Research Flags

Needing deeper work during planning:
- **Phase 1 — ownership:** walk both positions against the Lua 5.4 reference manual §4.8.
- **Phase 2 — non-finite `to_chars`:** compile and execute the spot-check immediately; this is the
  one MEDIUM-confidence claim in the whole body of research.
- **Phase 2 — no-header width:** document the nil-hole ambiguity with concrete test cases.
- **Phase 3 — logging:** settle the `Database::log_warning` API shape (Position B only).

Standard patterns, low research risk:
- **Phase 1 — RFC-4180 emission:** `csv_writer.hpp:555-590` is a correct reference implementation to
  read, even though it is not being linked.
- **Phase 1 — sandbox:** mirrors existing precedent exactly.
- **Phase 2 — type dispatch:** reuse `src/lua_runner.cpp:1085-1150`.

---

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | Verified line-by-line against the vendored source, not a summary. Decision firm. |
| Features | HIGH | Every per-library claim checked against that library's own docs. One open question (no-header width) resolved by precedent rather than proof. |
| Architecture | HIGH on everything except **the ownership contradiction, which is unresolved** |
| Numeric / non-finite | MEDIUM | Standards-text-verified; could not be executed in the research sandbox. Spot-check required. |
| Round-trip | HIGH | Test suite needs eight specific cases. |
| Pitfalls | HIGH for repo-sourced claims | Pitfalls 1 and 2 are the most likely to be missed; both need an explicit test. |

**Overall confidence:** HIGH, conditional on the ownership decision being made deliberately rather
than defaulted into.

### Gaps to Address

- **Handle ownership — Position A or B.** Settle in Phase 1 planning. Data-loss risk is equivalent
  either way (RAII holds in both); what differs sharply is diagnostic strength on the error path.
- **No-header row width.** Recommendation: the first row fixes it, strict thereafter (Rust
  `csv::Writer`'s behaviour — the closest architectural analog). Named failure mode: a first row with
  a trailing `nil` can silently lock in the wrong width through Lua's `#t`-with-holes ambiguity, and
  the resulting error is then misattributed to a later row.
- **Destructive-overwrite scope.** Exact `db.path()` match only, or extended to `.qvr`, `.toml`, and
  migration files? Explicitly deferred to the plan.
- **Parent directories.** Auto-create, matching `export_csv`, or throw a Pattern 1 error? Undecided.
- **Header-only vs Pimpl'd `.cpp`.** File-organization call; the Pimpl rationale does not apply to a
  zero-dependency writer.
- **Usertype name.** `Writer` or `CsvWriter`. Affects the `lua-api-sync.test.ts` array entry.
- **`nil` vs `""` is structurally indistinguishable after any CSV round trip.** Not fixable —
  document it.

## Sources

### Primary (HIGH confidence)
- `build/_deps/csv_parser-src/include/internal/csv_writer.hpp` — lines 25, 254, 555-590, read directly
- `src/lua_runner.cpp` — lines 129, 431-441, 565-579, 950-973, 1085-1150
- `src/csv_read.h` / `src/csv_read.cpp` — the v1.0 reader pattern
- `src/CMakeLists.txt:68` — `/bigobj` scoping
- `bindings/js/test/lua-api-sync.test.ts:75` — the hardcoded usertype array
- `.planning/v1.0-MILESTONE-AUDIT.md` and `.planning/milestones/v1.0-phases/` — prior-milestone traps
- Lua 5.4 reference manual §4.8 — `lua_close` and `__gc` finalization guarantees
- cppreference `std::to_chars` — the printf-equivalence Effects clause
- GitHub releases, vincentlaucsb/csv-parser — no release past 5.3.0

### Secondary (MEDIUM confidence)
- Python `csv`, Rust `csv`, Go `encoding/csv`, papaparse, Julia `CSV.jl` documentation — writer
  defaults for quoting, line terminators, and BOM
- RFC 4180

### Tertiary (needs validation)
- Non-finite `std::to_chars` output — standards-verified, never executed. Spot-check in Phase 2.
- Julia `CSV.jl`'s write-side line-ending default — not directly confirmed in any source found.

---
*Research completed: 2026-09-16*
*Ready for roadmap: yes — with the ownership decision explicitly flagged as open*
