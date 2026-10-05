---
phase: 09-julia-abstractexpression-and-docs
fixed_at: 2026-10-04T00:00:00Z
review_path: .planning/phases/09-julia-abstractexpression-and-docs/09-REVIEW.md
iteration: 1
findings_in_scope: 3
fixed: 3
skipped: 0
status: all_fixed
---

# Phase 09: Code Review Fix Report

**Fixed at:** 2026-10-04
**Source review:** .planning/phases/09-julia-abstractexpression-and-docs/09-REVIEW.md
**Iteration:** 1

**Summary:**
- Findings in scope: 3 (fix_scope: all)
- Fixed: 3
- Skipped: 0

## Fixed Issues

### WR-01: The write-mode save guard test passes for any failure, not the guard

**Files modified:** `bindings/julia/test/test_expression.jl`
**Commit:** 1402f01
**Applied fix:** Added assertions only; the existing `@test_throws` line stays. The test now
captures the error from `Quiver.save(w, path_out)` and checks that it is a `DatabaseException`
whose message contains `already open for writing`. That is the core's text from
`src/binary/binary_file.cpp:68`: `Cannot open_file: file is already open for writing: <path>`. It
also checks that neither `path_out.qvr` nor `path_out.toml` was created.

### IN-01: The Julia AGENTS.md overstates GC.@preserve coverage of the file

**Files modified:** `bindings/julia/AGENTS.md`
**Commit:** 3b09ea2
**Applied fix:** The sentence now matches the code. The file is preserved across
`quiver_expression_from_file` (in `Expression(::Binary.File)`), and every converted handle is
preserved across its own operation's ccall. Changed the wording only. I did not add the optional
`GC.@preserve file` wrapping to the `file.jl` ccalls: that was outside the requested scope, and
callers keep the file reachable.

### IN-02: `get_metadata(file)` and the file used as an operand can disagree for an unopened `Binary.File(path)`

**Files modified:** `bindings/julia/AGENTS.md`
**Commit:** 3bc1bb5
**Applied fix:** Added one clause to the AbstractExpression bullet. An unopened
`Binary.File(path)` reports its handle's empty metadata, while an expression built from it reads the
file's metadata from disk. I checked this against
`BinaryFile::BinaryFile(const std::string&)`, which builds the handle with `BinaryMetadata{}`. The
paragraph's lines were also reflowed to the file's width.

## Verification

- All edits and commits were made in an isolated worktree
  (`.claude/worktrees/rf-09-*` on a temp branch). The branch was then fast-forwarded into `rs/lua-2`,
  and the worktree and temp branch were removed.
- The Julia expression tests ran **in the worktree**, with `QUIVER_LIB_DIR` pointing at the main
  checkout's `build/bin` and a copy of the main `Manifest.toml`:
  `Pkg.test(test_args=["test_expression.jl"])` gave 290/290 passing.
  The worktree is gone now. To reproduce in the main checkout, run
  `bindings/julia/test/test.bat test_expression.jl`.
- JuliaFormatter (`bindings/julia/format/format.jl`) ran in the worktree and changed nothing in the
  committed diff.
- The two AGENTS.md wording fixes were checked by re-reading the file. They have no syntax check.

---

_Fixed: 2026-10-04_
_Fixer: Claude (gsd-code-fixer)_
_Iteration: 1_
