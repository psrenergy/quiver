---
phase: 09-julia-abstractexpression-and-docs
reviewed: 2026-10-04T00:00:00Z
depth: standard
files_reviewed: 10
files_reviewed_list:
  - AGENTS.md
  - CHANGELOG.md
  - bindings/julia/AGENTS.md
  - bindings/julia/src/Quiver.jl
  - bindings/julia/src/binary/Binary.jl
  - bindings/julia/src/binary/file.jl
  - bindings/julia/src/expression.jl
  - bindings/julia/test/test_expression.jl
  - src/AGENTS.md
  - src/c/AGENTS.md
findings:
  critical: 0
  warning: 1
  info: 2
  total: 3
status: issues_found
---

# Phase 09: Code Review Report

**Reviewed:** 2026-10-04
**Depth:** standard
**Files Reviewed:** 10
**Status:** issues_found

## Summary

Reviewed `git diff 4e35949..HEAD` for the Julia `AbstractExpression` refactor and its docs. The
refactor itself holds up:

- Every removed `Binary.File` forwarder (97 of them, and the count in `bindings/julia/AGENTS.md` is
  correct) is covered by one of the `AbstractExpression` methods, with the same three operand
  shapes, so dispatch does not change for any existing call.
- `rename_agents` on a file used to accept any `AbstractDict` and then fail with a `MethodError` in
  the method it forwarded to. It now fails at its own signature. The behaviour is the same.
- `import .Binary: get_metadata` sits before `include("expression.jl")`, so the `Expression` method
  extends Binary's function. `Quiver.get_metadata(::File)` used to raise `MethodError` and now
  works, so nothing breaks.
- `GC.@preserve` now covers every converted handle across its ccall.
- `save(file, file_path)` and `save` on a file open for writing are both refused by the C++ core
  (`AbstractExpression::save`: a collision check, and `open('r')` against `write_registry`). Neither
  path touches the caller's handle, because `ExpressionFile` owns a private unopened `BinaryFile`.

I checked the doc claims against the code: `abstract_expression.h`, the `final` `Expression`, the
protected copy/move, `ExpressionFile` built from a path, the `SOL_BASE_CLASSES`/`SOL_DERIVED_CLASSES`
traits, the Lua operand error text, `ifelse` having no `double` overload in Lua, and
`quiver_expression_from_file` copying the path. All of them hold.

I found no correctness bug in the source. There is one weak test assertion and two documentation
and consistency notes.

## Warnings

### WR-01: The write-mode save guard test passes for any failure, not the guard

**File:** `bindings/julia/test/test_expression.jl:2283`
**Issue:** `@test_throws Quiver.DatabaseException Quiver.save(w, path_out)` accepts any
`DatabaseException`. The test is meant to pin the core's "file is already open for writing" refusal
(`BinaryFile::open`, `src/binary/binary_file.cpp:68`). It would still pass if `save` failed for an
unrelated reason, for example if `Expression(w)` failed to read the `.toml` while `w` is open, or
if some future guard fired first. Two lines earlier, the sibling collision case pins its message
with `occursin("collides with input file", ...)`; this case does not. The test also never checks
that no output file was created, which is the property that matters when a save is refused.
**Fix:**
```julia
err = try
    Quiver.save(w, path_out)
    nothing
catch e
    e
end
@test err isa Quiver.DatabaseException && occursin("already open for writing", err.msg)
@test !isfile(path_out * ".qvr")
```

## Info

### IN-01: The Julia AGENTS.md overstates GC.@preserve coverage of the file

**File:** `bindings/julia/AGENTS.md:160-161`, `bindings/julia/src/binary/file.jl:92-104`
**Issue:** The new paragraph says "The converted handles and the file are still passed to
`GC.@preserve` across every ccall." That is not what the code does:

- The file is preserved only across the `quiver_expression_from_file` call, inside
  `Expression(::Binary.File)` (`expression.jl:13`). `_binop`, `_unop`, `save` and the other
  operations preserve only the converted `Expression`. That is correct, because the C expression
  owns a copy of the path.
- The file's own ccalls do not preserve the file at all: `get_metadata(::File)`, which is now the
  shared generic the docs present as the uniform metadata call, plus `read`, `write!` and
  `get_file_path`.

This is not a live bug, since callers keep the file reachable. But the sentence will mislead the
next person who checks preserve coverage.
**Fix:** Change the sentence to: "Every converted handle is passed to `GC.@preserve` across its
ccall, and the file across `quiver_expression_from_file`." Optionally, wrap the four `file.jl`
ccalls in `GC.@preserve file` for consistency with `expression.jl`.

### IN-02: `get_metadata(file)` and the file used as an operand can disagree for an unopened `Binary.File(path)`

**File:** `bindings/julia/src/binary/file.jl:18-22,92-96`, `bindings/julia/src/expression.jl:125`
**Issue:** `Binary.File(path)` (`quiver_binary_file_create`, used in `example2.jl` and
`test_binary_file.jl`) builds an unopened handle whose in-memory metadata is an empty
`BinaryMetadata{}`. That handle is still a valid operand, because `ExpressionFile` reads the
`.toml` from disk. So `get_metadata(f)` returns empty labels while `get_metadata(f + 0.0)` returns
the real ones.

The new docs and CHANGELOG present `get_metadata` as one generic that "works on a file (its
handle's metadata) or an expression". The C++ override is a deliberate design decision, so this is
not a defect to fix in the binding. It is a gap in the documentation.
**Fix:** Add one clause to the `bindings/julia/AGENTS.md` AbstractExpression bullet: "an unopened
`Binary.File(path)` reports empty metadata but is still a valid operand, read by path."

---

_Reviewed: 2026-10-04_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
