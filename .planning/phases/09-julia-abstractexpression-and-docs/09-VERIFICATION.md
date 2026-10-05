---
phase: 09-julia-abstractexpression-and-docs
verified: 2026-10-04T00:00:00Z
status: passed
score: 5/5 roadmap success criteria verified (plan must-haves 26/26)
behavior_unverified: 0
overrides_applied: 0
---

# Phase 9: Julia AbstractExpression and Docs Verification Report

**Phase Goal:** Julia mirrors the finished surface: `Binary.File` and `Expression` are subtypes of one `AbstractExpression` and a file accepts every expression operation, without the 97 forwarders. The AGENTS.md files and the CHANGELOG describe the finished milestone.
**Verified:** 2026-10-04
**Status:** passed
**Re-verification:** No (initial verification)

## Goal Achievement

### Observable Truths (ROADMAP success criteria)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | `abstract type AbstractExpression end` in `Quiver.jl` before `include("binary/Binary.jl")`; a test asserts both subtypes | VERIFIED | `bindings/julia/src/Quiver.jl:27` declares it, and line 29 is the Binary include. `Binary.jl:3` has `using ..Quiver: AbstractExpression, ...`. `file.jl:1` has `mutable struct File <: AbstractExpression`, and `expression.jl:1` has `mutable struct Expression <: AbstractExpression`. Testset "Binary.File is an AbstractExpression" (`test_expression.jl:2169-2172`) asserts both subtypes and `isabstracttype`. A live Julia session printed `true true true` (both subtypes, and `get_metadata === Binary.get_metadata`). |
| 2 | Operations defined once on `AbstractExpression`, a file converted through `quiver_expression_from_file`, the forwarders gone, no C API or `c_api.jl` diff | VERIFIED | `grep -n 'Binary.File' expression.jl` prints one line, 11 (`function Expression(file::Binary.File)`), down from 54 at base `4e35949`. Every operator, comparison, logical op, unary op, `ifelse`, `save`, `aggregate*` and `*_agents` is typed on `AbstractExpression` and converts through the private `_expression`. `git diff --stat 4e35949..HEAD` (excluding `.planning`) touches only the 3 AGENTS.md files, CHANGELOG, Quiver.jl, Binary.jl, file.jl, expression.jl and test_expression.jl, so there is no change to `c_api.jl`, `include/`, C/C++ sources or the other bindings. `Test.detect_ambiguities(Quiver; recursive=true)` returns 0 (run live). `hasmethod(Quiver.Expression, Tuple{Quiver.Expression})` is `false`, so there is no public identity constructor. |
| 3 | Julia tests run every expression operation on a raw `Binary.File`, and the suite passes | VERIFIED | Parity testset `test_expression.jl:2195-2264` covers 33 operations and 3 mixed cases. Each one byte-compares the `.qvr` and `.toml` output against the same operation on `Expression` operands: file-file and file-Real/Real-file arithmetic, `> < >= <=`, `gt/lt/gte/lte/eq/neq`, `& \| !`, unary `-`, `abs sqrt log exp`, `ifelse`, `aggregate`, `aggregate_agents` with and without a parameter, `select_agents`, `rename_agents`. `save` and `get_metadata` on a raw file have their own testsets (2169, 2266, 2369). The edge testsets cover save guards, the same file on both sides, a closed file, label order, saving twice, and an expression outliving its file across `GC.gc()`. test-all log: `Expression 287/287` and Julia `1675/1675`, "Testing Quiver tests passed". |
| 4 | Root AGENTS.md has the Design Decision and updated cross-layer rows. src/, src/c/ and bindings/julia/ AGENTS.md describe the type and the `src/lua_runner/` layout. The `caches an open BinaryFile` grep is empty | VERIFIED | `AGENTS.md:78` has `- **A binary file is an expression.**`. It names `abstract_expression.h`, `node()`, `get_metadata()`, the `SOL_BASE_CLASSES`/`SOL_DERIVED_CLASSES` traits, the Julia form, and the C API's lack of an abstract handle. These claims were spot-checked against `abstract_expression.h:28,32,48`, `src/lua_runner/internal.h:38-40` and `src/c/expression/expression.cpp:99-112`. The table rows at `AGENTS.md:844-848` cover Get metadata, Is an expression, Save, Aggregate and Select / rename agents. `src/AGENTS.md:330,1056,1069` describe the base and an accurate `ExpressionFile`, and line 47 gives the lua_runner layout. `src/c/AGENTS.md:45-57` has the bridge note and the `src/lua_runner/` citation. The `bindings/julia/AGENTS.md` AbstractExpression bullet is present. `grep -n 'caches an open' src/AGENTS.md` printed nothing (rc=1). |
| 5 | CHANGELOG `[0.13.0]` holds every BREAKING line with a caller action, plus both Added entries. No planning IDs, version 0.13.0, six suites and lua-api sync green | VERIFIED | The section has the C++ copy-init line ("write `Expression e(file);`"), C++ `metadata()` to `get_metadata()` ("Call `get_metadata()`"), Lua `e:metadata()` renamed ("Call `get_metadata()`"), and Lua strict arity ("Drop the extra argument."), as four separate BREAKING entries. `### Added` has exactly the Lua and Julia "a binary file is an expression" entries, in that order. The Changed/Added/Fixed order holds, and the compare link appears once. A planning-ID regex scan of the section found 0 hits. `uv run python scripts/assert_version.py`: "All project files at 0.13.0". The test-all log shows all six suites PASS and "All tests PASSED". `bun test test/lua-api-sync.test.ts`: 6 pass, 0 fail (run live). |

**Score:** 5/5 roadmap truths verified (0 present-but-behavior-unverified)

Plan must-haves: all 15 of 09-01's truths and all 11 of 09-02's truths hold, including the 2 `backstop` truths. The "parallel" truths are confirmed by evidence. Julia type declarations are top-level and immutable, and the subtype assertions pass in the suite run. AGENTS.md files have no runtime behaviour.

### Behavior-dependent truths

| Truth | Behavioral evidence |
|-------|---------------------|
| An expression from a raw file outlives the file, and temporaries never close the source | Testset "Expression from a raw Binary.File outlives it" (`test_expression.jl:2391`) calls `close!` and then `GC.gc()` before `save`, and runs `GC.gc()` over temporaries. It passed in the test-all Julia run (Expression 287/287). |
| A file stays open and readable after `save` from it, twice | "Saving a raw Binary.File twice" (2369) passed |
| A closed file is not an operand, and no output is written | "Closed Binary.File is not an operand" (2314) asserts `Null argument` and `!isfile`. It passed. |

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `bindings/julia/src/Quiver.jl` | abstract type before the Binary include; `import .Binary: get_metadata` before expression.jl | VERIFIED | lines 27-31 |
| `bindings/julia/src/binary/Binary.jl` | `using ..Quiver: AbstractExpression` | VERIFIED | line 3 |
| `bindings/julia/src/binary/file.jl` | `mutable struct File <: AbstractExpression`; `get_metadata(::File)` | VERIFIED | lines 1, 92 |
| `bindings/julia/src/expression.jl` | one conversion, `_expression`, operations on `AbstractExpression`, `GC.@preserve` | VERIFIED | 193 lines; 14 `GC.@preserve` sites; `GC.@preserve file check(C.quiver_expression_from_file(...))` at line 13 |
| `bindings/julia/test/test_expression.jl` | hierarchy, parity and edge testsets | VERIFIED | +251 lines; 0 base lines removed (`git diff -U0` shows no `-` lines) |
| `bindings/julia/AGENTS.md` | AbstractExpression bullet | VERIFIED | covers `_expression`, `GC.@preserve`, the one `get_metadata` generic, and the closed-file divergence |
| `AGENTS.md`, `src/AGENTS.md`, `src/c/AGENTS.md` | decision, rows, ExpressionFile fix, bridge note | VERIFIED | see truth 4 |
| `CHANGELOG.md` | Julia Added entry; section complete | VERIFIED | see truth 5 |

### Key Link Verification

| From | To | Via | Status |
|------|----|-----|--------|
| `Binary.jl` | `Quiver.jl` | `using ..Quiver: AbstractExpression` | WIRED |
| `Quiver.jl` | `expression.jl` | `import .Binary: get_metadata` before the include (`get_metadata === Binary.get_metadata` is true live) | WIRED |
| `expression.jl` | `quiver_expression_from_file` | `GC.@preserve file check(C.quiver_expression_from_file(file.ptr, out))` | WIRED |
| `AGENTS.md` | `abstract_expression.h` | named in the decision; its claims match the header | WIRED |
| `src/c/AGENTS.md` | `src/c/expression/expression.cpp` | the bridge note matches `new quiver_expression(quiver::Expression(file->binary_file))` | WIRED |

### Data-Flow Trace (Level 4)

Not applicable: the phase changes a library binding and docs, and nothing renders dynamic data. The data path of every file operand is file → `quiver_expression_from_file` → C expression, and the parity testset's byte-compares exercise it end to end.

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| Type hierarchy and the one generic | `julia +1.12.5 --project=bindings/julia -e '... <: ..., get_metadata === Binary.get_metadata'` | `true true true` | PASS |
| No method ambiguities | `Test.detect_ambiguities(Quiver; recursive=true)` | `amb=0` | PASS |
| No public identity constructor | `hasmethod(Quiver.Expression, Tuple{Quiver.Expression})` | `false` | PASS |
| lua-api sync | `bun test test/lua-api-sync.test.ts` | 6 pass, 0 fail | PASS |
| Version | `uv run python scripts/assert_version.py` | All five at 0.13.0 | PASS |
| Six suites | orchestrator's `scripts/test-all.bat` log (grepped) | C++ 1476, C API 543, Julia 1675, Dart 444, JS PASS, Python 350; All tests PASSED | PASS |

### Probe Execution

None: the phase declares no `scripts/*/tests/probe-*.sh` probes. `build/julia-check/gate.sh` is a gitignored executor harness, not a declared probe. Its checks were re-derived independently above.

### Prohibitions (judgment tier)

| Prohibition | Evidence | Disposition |
|-------------|----------|-------------|
| No C API, `c_api.jl`, C++ core, Lua binder or other-binding change, and no old surface kept (no forwarder, no Union alias, no public identity constructor, no second `get_metadata`, no Julia-crafted error text) | The diff stat lists only Julia binding files and docs. One `Binary.File` line remains. `hasmethod` identity is false. `get_metadata === Binary.get_metadata`. `expression.jl` has no `error(`/`throw(` calls: every failure goes through `check`. | Verified against the code, not flagged |
| No existing test line edited or deleted; the parity testset was green before the forwarders were deleted | `git diff -U0 4e35949..HEAD -- bindings/julia/test` shows 0 removed lines. Commit order: `66b5eb9` (parity test) precedes `3c643ff` (refactor). | Verified against the code, not flagged |

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|-------------|-------------|--------|----------|
| JUL-01 | 09-01 | abstract type declared before the Binary include; both subtypes | SATISFIED | truth 1 |
| JUL-02 | 09-01 | operations once on AbstractExpression via `quiver_expression_from_file`; 97 forwarders deleted; no C API change; suite passes | SATISFIED | truth 2 (54 lines to 1; the review counts the forwarders as 97) |
| JUL-03 | 09-01 | every operation, `save` and `get_metadata` included, works on a file, with tests | SATISFIED | truth 3 |
| DOC-01 | 09-01, 09-02 | root, src/, src/c/ and bindings/julia/ AGENTS.md describe the type and the lua_runner layout; ExpressionFile line fixed | SATISFIED | truth 4 |
| DOC-02 | 09-01, 09-02 | CHANGELOG complete; six suites green; version 0.13.0 | SATISFIED | truth 5 |

No orphaned requirements: REQUIREMENTS.md maps exactly JUL-01..03 and DOC-01..02 to Phase 9, and the plans claim all five.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| (all added lines) | - | TBD/FIXME/XXX/TODO/placeholder | none found | - |
| `src/AGENTS.md` | 1030, 1040 | `Expression::save` in pre-existing text, but save is now `AbstractExpression::save` (`src/expression/expression.cpp:42`) | Info | Stale qualifier in lines this phase did not edit. It is not a must-have, and the meaning stays clear. |
| `bindings/julia/test/test_expression.jl` | 2281 | `@test_throws Quiver.DatabaseException Quiver.save(w, path_out)` does not pin the "already open for writing" text and does not check that no output file was written (review WR-01) | Warning (advisory) | The plan's truth asks only for "raises DatabaseException", so the must-have holds. The assertion is weaker than its sibling collision case. |

### Human Verification Required

None.

### Gaps Summary

There are no gaps. The phase goal holds in the codebase:

- `Binary.File` and `Expression` share `AbstractExpression`.
- `expression.jl` names the file type once, in the conversion constructor, and every operation works on a raw file. Byte-identical parity tests prove it.
- The C API and `c_api.jl` are untouched.
- The four AGENTS.md files and the CHANGELOG describe the finished milestone.
- All six suites and the lua-api sync test are green at 0.13.0.

Advisory follow-ups, not blocking:
- Tighten WR-01's assertion.
- Update the two stale `Expression::save` mentions in `src/AGENTS.md` to `AbstractExpression::save`.

---

_Verified: 2026-10-04_
_Verifier: Claude (gsd-verifier)_
