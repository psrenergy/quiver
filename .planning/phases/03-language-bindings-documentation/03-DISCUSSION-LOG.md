# Phase 3: Language bindings + documentation - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-05-19
**Phase:** 03-language-bindings-documentation
**Areas discussed:** Plan split structure, Test coverage breadth per binding, docs/time_series.md update scope, Test file placement (Julia + Lua)

**Discussion mode:** User invoked `/gsd-discuss-phase 3` with the standing "no clarifying questions — make the reasonable call and continue" directive in the session. The four gray areas were presented via `AskUserQuestion`; the user returned empty selections both times the question was offered, so Claude exercised reasonable-default authority and decided each area, with rationale logged here for traceability. The user can override any of these in subsequent phases by editing CONTEXT.md or re-discussing.

---

## Plan split structure

| Option | Description | Selected |
|--------|-------------|----------|
| 2 plans (bundle bindings, separate docs) | Match Phase 1/2 rhythm (2 plans each); group all 4 binding wrappers + tests into one plan, docs as the second | |
| 3 plans (auto-gen trio + Lua + docs) | Group Julia/Dart/Python (which share the auto-generator pattern); Lua separate (hand-edits LuaRunner); docs separate | |
| 5 plans (one per binding + docs) | One atomic plan per deliverable: Julia, Dart, Python, Lua, docs. Each plan owns its binding's wrapper + tests | ✓ |

**Claude's choice:** 5 plans (one per binding + docs).
**Rationale:** Each binding is an independent codebase with its own toolchain (Julia/Pkg, Dart/pub, Python/uv, Lua/CMake) and test harness. Failure modes do not overlap. Atomic per-binding commits give clean revert points and allow parallel execution if a future executor uses worktrees. Phase 1 and Phase 2 used 2 plans because they each had one impl file + one test file; Phase 3's scope is materially wider. The marginal git-history overhead of 5 commits vs 2 commits is negligible against the auditability win.

---

## Test coverage breadth per binding

| Option | Description | Selected |
|--------|-------------|----------|
| REQUIREMENTS minimums only | Each binding ships exactly what its success criterion names (e.g., Julia: insert + upsert + 1 error; Python: insert + upsert + dict unpacking). No multi-dim PK at the binding layer. | |
| REQUIREMENTS minimums + 1 multi-dim happy path | Add a single multi-dimension PK (`date_time + block`) happy-path case per binding using `tests/schemas/valid/multi_dim_time_series.sql`. Marshaling shape is the one place where multi-dim could surface a binding-specific bug. | ✓ |
| Mirror C++ scenario matrix at every binding | Replicate Phase 1's 9 scenarios at each binding (~36 new test cases total). Maximum rigor; significant duplication. | |

**Claude's choice:** REQUIREMENTS minimums + 1 multi-dim happy path per binding.
**Rationale:** Phase 1 (9 C++ scenarios) and Phase 2 (12 C API scenarios) already cover the deep semantic and error matrix at the layers where bugs live. Each binding wrapper is a thin marshaler whose only novel surface is the language-specific row representation (kwargs vs Map vs Lua table). Multi-dim is the one place where row→map translation could go wrong differently from update (single map vs vector-of-maps), so one happy-path test per binding is worth the ~5 lines of new code. Mirroring the full C++ matrix at every binding is duplicate work with negative ROI.

---

## docs/time_series.md update scope

| Option | Description | Selected |
|--------|-------------|----------|
| Narrow DOC-11 fix only | Rewrite only the `add_time_series_row!` example in §"Inserting data". Leave the rest of the file (which contains other stale references) untouched. | ✓ |
| Broader cleanup | Also fix references to non-existent `read_time_series_table`, `update_time_series_row!`, `delete_time_series!` — bring the entire file in sync with the shipped API. | |

**Claude's choice:** Narrow DOC-11 fix only.
**Rationale:** DOC-11's acceptance criterion is precise — "the `add_time_series_row!` example matches the actually-shipped Julia signature." Broader staleness is a separate problem (whole-doc rewrite) that deserves its own phase or a `/gsd-docs-update` pass. Per CLAUDE.md "Don't add features, refactor, or introduce abstractions beyond what the task requires" — scope discipline matters even for docs. The other stale references are captured in `<deferred>` in CONTEXT.md so they do not get lost.

---

## Test file placement (Julia + Lua)

| Option | Description | Selected |
|--------|-------------|----------|
| Match existing convention | Julia tests → `bindings/julia/test/test_database_time_series.jl` (existing file with related testsets); Lua tests → `tests/test_lua_runner.cpp` (existing GTest harness with `LuaRunnerTest.UpdateTimeSeriesGroup` as direct precedent). | ✓ |
| Verbatim spec filenames | Create new `bindings/julia/test/test_time_series.jl` and new files under `tests/lua/` to match REQUIREMENTS.md filenames literally. | |

**Claude's choice:** Match existing convention.
**Rationale:** REQUIREMENTS.md filenames were approximate at write-time. The actual project convention names Julia binding test files `test_database_*.jl` (binding-side tests are namespaced under `Database`) and Lua tests live inside the C++ GTest harness because that's how `LuaRunner` is exercised — REQUIREMENTS.md acknowledges this with "or equivalent existing Lua test harness." Creating new files solely to satisfy a literal filename split tests across two files in each binding, hurting discoverability. The spec's intent is "tests exist and pass," not "tests live at a specific path."

---

## Claude's Discretion

The following sub-decisions were left to Claude per the "no clarifying questions" directive and are noted here without alternatives:

- **Python error-path test inclusion** — Default: skip. Python's success criterion (PY-11) explicitly names "insert + upsert + dict unpacking" and does not require an error path. The C API layer's error relay is already proven by Phase 2 tests.
- **Lua static helper naming** — Default: reuse `lua_table_to_value_map` directly inside `add_time_series_row_lua`. No new helper.
- **Dart `.dart_tool/` cleanup** — Default: incremental rebuild first; fall back to full clean per CLAUDE.md note only if DLL drift is observed during test runs.
- **Test ordering within each binding's new block** — Default: match C++ test file's order (insert → upsert → multi-dim → error) for review consistency.
- **Whether `add_time_series_row!` is re-exported from `Quiver.jl`** — Default: yes; verify it appears in any public-symbol export list as part of Plan 03-01's verification step.

## Deferred Ideas

Captured in CONTEXT.md `<deferred>`:

- Broader `docs/time_series.md` cleanup (DOC-11 stays narrow; the rest of the file's staleness deserves a dedicated docs-update phase)
- Shared `marshal_time_series_columns` helper between add and update per binding (Phase 2 declined the equivalent refactor at the C API layer — Phase 3 keeps the same posture)
- `add_time_series_rows` plural batched variant (CORE-15, v2)
- Multi-element add across multiple `id`s (out of scope per REQUIREMENTS.md)
- Streaming row append (CORE-16, v2)
- High-level `read_time_series_row` wrappers in Dart and Python (Julia has one; Dart and Python expose only the raw FFI symbol — its own future single-issue phase)
- Julia test file rename to literal `test_time_series.jl` — not worth churning git history
