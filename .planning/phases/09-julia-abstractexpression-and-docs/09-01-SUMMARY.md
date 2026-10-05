---
phase: 09-julia-abstractexpression-and-docs
plan: 01
subsystem: julia-binding
status: complete
tags: [julia, expression, binary, dispatch, ffi]
requires: []
provides:
  - "Quiver.AbstractExpression with Binary.File and Expression as subtypes"
  - "every Julia expression operation defined once on AbstractExpression"
  - "Quiver.save(file, path) and Quiver.get_metadata(file) on a raw Binary.File"
affects: [09-02]
tech-stack:
  added: []
  patterns:
    - "parent-module abstract type subtyped from a submodule via using ..Quiver"
    - "private _expression conversion, GC.@preserve on converted locals"
    - "one get_metadata generic owned by Binary, imported into Quiver before expression.jl"
key-files:
  created: []
  modified:
    - bindings/julia/src/Quiver.jl
    - bindings/julia/src/binary/Binary.jl
    - bindings/julia/src/binary/file.jl
    - bindings/julia/src/expression.jl
    - bindings/julia/test/test_expression.jl
    - bindings/julia/AGENTS.md
    - CHANGELOG.md
decisions:
  - "get_metadata is one generic owned by Binary; Quiver imports it before include(\"expression.jl\"), so a file answers with its handle's metadata and Quiver.get_metadata === Quiver.Binary.get_metadata"
  - "Operand conversion is the private _expression helper, not a public Expression(::Expression) identity constructor"
  - "The 97 Binary.File forwarders are deleted; Expression(file::Binary.File) is the one line in expression.jl naming the file type"
metrics:
  duration: "18 min"
  completed: 2026-10-05
  tasks: 3
  files: 7
actuals:
  tokens: 6400
  tasks: 3
  commits: 4
---

# Phase 9 Plan 1: Julia AbstractExpression Summary

`Binary.File` and `Expression` are now subtypes of `Quiver.AbstractExpression`. Every expression operation is
defined once on it and converts a file through the existing `quiver_expression_from_file`, with `GC.@preserve`
on every converted handle. The 97 `Binary.File` forwarders are gone, and `get_metadata` is one generic owned by
`Binary`. No C API, `c_api.jl` or C++ change.

## Gate record

- BASE: `4e359499e85c5f191e6ea47d738575819a5ca497`
- Gate on the unchanged tree: `JULIA GATE PASS expression=186 ambiguities=0 binary_file_lines=54` (matches the plan's baseline).
- After Task 1 (tracer, `90457fc`): `JULIA GATE PASS expression=195 ambiguities=0 binary_file_lines=54`
- After Task 2 Part A (parity, `66b5eb9`, old forwarders still present): `JULIA GATE PASS expression=287 ambiguities=0 binary_file_lines=54`. The parity testset and the edge testsets were green against the old forwarders before the refactor.
- After Task 2 Part B (refactor, `3c643ff`): `END=1` gives `JULIA GATE PASS expression=287 ambiguities=0 binary_file_lines=1`
- After Task 3 (docs, `fa3a656`): `END=1` gives `JULIA GATE PASS expression=287 ambiguities=0 binary_file_lines=1`
- Expression testset: 186 → 287 (+101). Full Julia suite (`Pkg.test()`): **1675 pass** (Phase 8 baseline 1574, +101), "Testing Quiver tests passed".
- `bindings/julia/src/expression.jl`: 266 → 193 lines, 14 `GC.@preserve` sites, `grep -c 'Binary.File'` = 1 (`function Expression(file::Binary.File)`).
- `Test.detect_ambiguities(Quiver; recursive = true)`: 0 at every gate run.

## Mutation check

I deleted `import .Binary: get_metadata` from `Quiver.jl` and ran `test_expression.jl`. The run failed.
"Binary.File is an AbstractExpression" had 1 failure (the `===` identity) and 1 error (`MethodError: no method
matching get_metadata(::Quiver.Binary.File)`), and "Save guards" and "Saving twice" errored the same way
(Expression 278 pass / 1 fail / 3 error). I restored the file with `git checkout -- bindings/julia/src/Quiver.jl`,
and `git diff --quiet -- bindings/julia/src/Quiver.jl` exits 0. `bindings/julia/src` as a whole still carried the
not-yet-committed refactor of `expression.jl` at that moment, which was committed next.

## Commits

| Task | Commit | Message |
|------|--------|---------|
| 1 | `90457fc` | feat(09-01): make Binary.File and Expression subtypes of AbstractExpression in Julia |
| 2A | `66b5eb9` | test(09-01): pin every expression operation on a raw Binary.File in Julia |
| 2B | `3c643ff` | refactor(09-01): define Julia expression operations once on AbstractExpression |
| 3 | `fa3a656` | docs(09-01): Julia AGENTS.md and CHANGELOG for AbstractExpression |

## Deviations from Plan

1. **[Style] The `&`/`|` loop stays separate from the `+ - * /` loop.** The plan asked for one `@eval` loop
   over `+ - * / & |`. I kept two loops so the `&`/`|`/`!` explanatory comment stays above the logical
   operators. It now ends "Generated for the same three operand shapes". The method set is identical.
2. **[Formatter] One parity-test line was rewrapped.** `format.jl` rewrapped the
   `aggregate_agents(..., PERCENTILE, 0.5)` closure, a line added in `66b5eb9`, so the refactor commit
   includes that one test-file change. No BASE test line was removed or changed (gate check (b) is 0). The
   formatter reported "Some files have not been formatted!", which it prints after reformatting. It touched
   only the two files in scope.
3. **[Harness] Gate pipefail fix.** In gate step (b), `grep '^-'` with no match exited 1 and, under
   `pipefail`, killed the script silently. Each grep is now wrapped in `{ grep ... || true; }`. The fix is in
   the gitignored harness only.
4. **Tracer feedback gate.** Auto mode is off (`auto_advance: false`). I did not stop for a
   `checkpoint:human-verify` after the tracer. The project config sets `human_verify_mode: end-of-phase`,
   the plan is `autonomous: true`, and the user's standing preference is to verify adversarially at gates
   rather than ask. So I re-ran the tracer's `<verify>` (the gate) end to end, it passed, and expansion
   continued.

Otherwise the plan ran as written. The bodies are the RESEARCH "Code Examples" code, with both explanatory
comments restored.

## Flagged Assumptions (restated)

- **FA-1 (Lua parity, closed file):** in Lua a closed file is still an expression, read by path. In Julia,
  `Binary.close!` deletes the C handle, so a closed `Binary.File` raises `Null argument` (pinned by "Closed
  Binary.File is not an operand"). Closing this gap needs a C API change, which is out of scope.
- **FA-2 (get_metadata on an unopened file):** `Quiver.get_metadata(Binary.File(path))` returns the handle's
  empty metadata, while `Quiver.get_metadata(Quiver.Expression(file))` reads the `.toml`. This is the C++
  virtual's semantics, unchanged, and not pinned here.
- **FA-3:** the other `.ptr` ccalls in `src/binary/file.jl` (read, write, get_metadata, ...) still have no
  `GC.@preserve`. Only the struct line changed there.

## Known Stubs

None.

## Self-Check: PASSED

- FOUND: bindings/julia/src/Quiver.jl, bindings/julia/src/binary/Binary.jl, bindings/julia/src/binary/file.jl,
  bindings/julia/src/expression.jl, bindings/julia/test/test_expression.jl, bindings/julia/AGENTS.md, CHANGELOG.md
- FOUND commits: 90457fc, 66b5eb9, 3c643ff, fa3a656
