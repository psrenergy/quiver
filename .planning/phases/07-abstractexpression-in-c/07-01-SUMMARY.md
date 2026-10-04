---
phase: 07-abstractexpression-in-c
plan: 01
subsystem: expression
status: complete
tags: [c++, expression, binary, refactor, breaking]
requires: []
provides:
  - "quiver::AbstractExpression (pure node(), virtual get_metadata(), non-virtual save/aggregate/aggregate_agents/select_agents/rename_agents)"
  - "quiver::AggregateOperation at namespace scope"
  - "BinaryFile and Expression (final) derive from AbstractExpression; operators take const AbstractExpression&"
affects: [08-typed-lua-parameters, 09-julia-and-docs]
tech-stack:
  added: []
  patterns:
    - "one node() local per operand, in declaration order (binary_op/unary_op helpers)"
    - "save() holds the root node in a local (a file's leaf is a temporary)"
key-files:
  created:
    - include/quiver/expression/abstract_expression.h
  modified:
    - include/quiver/expression/expression.h
    - include/quiver/expression/expression_node.h
    - include/quiver/binary/binary_file.h
    - src/expression/expression.cpp
    - src/expression/expression_file.cpp
    - src/c/expression/expression.cpp
    - src/lua_runner/expression.cpp
    - src/lua_runner/binary.cpp
    - tests/test_expression.cpp
    - CHANGELOG.md
    - src/AGENTS.md
decisions:
  - "get_metadata() is virtual but not pure on AbstractExpression (base body node()->metadata(), overridden only by BinaryFile), so node() stays the one pure virtual"
  - "ROADMAP criterion 3 tested as ifelse(a > 1.0, a, b): ifelse has no double overload in any layer and none was added"
  - "MSVC accepts the base-parameter ==/!= overloads with no C2666 and no warning; public signatures unchanged"
metrics:
  duration: 35min
  completed: 2026-10-04
  tasks: 3
  files: 12
actuals:
  tokens: 20375
  tasks: 3
  commits: 3
---

# Phase 7 Plan 01: AbstractExpression in C++ Summary

A `BinaryFile` is now a C++ expression: `AbstractExpression` (one pure `node()`) is the parameter type of every
operator, free function and method, `Expression` is final with an explicit freeze constructor, and a file's `node()`
is a fresh path-based leaf, so an expression never touches the caller's handle.

## Gate record

- BASE: `c20c0d0f31cb636dad31e60701b7844e1ae2bd25`
- At base (`GATE_AT_BASE=1`, before any source edit): `GATE PASS expr=116 tests=1454 lua=477 capi=543` (GOLDEN debug OK, EQ debug OK)
- After Task 1: `GATE PASS expr=117 tests=1455 lua=477 capi=543`
- Final (after Task 3): `GATE PASS expr=125 tests=1463 lua=477 capi=543`, with GOLDEN debug OK and EQ debug OK (`[true,true,false]`)
- N = 9 (TEST_F lines added to `tests/test_expression.cpp`). Windows Debug: ExpressionFixture 125, quiver_tests 1463,
  Lua* 477 in 12 suites, SandboxedPathTest 11, quiver_c_tests 543, ExpressionCApiFixture 73, LuaRunnerCApiTest 27; all pass.
- `git diff --numstat BASE -- src/c` is exactly `1 1 src/c/expression/expression.cpp`; `include/quiver/c`, `bindings`,
  root `AGENTS.md`, `src/c/AGENTS.md`, return_json and path_policy are untouched; no `tests/test_lua*` or `tests/test_c_api*` change.

## Commits

| Task | Commit | Message |
|------|--------|---------|
| 1 (tracer) | c36ba26 | refactor(07-01): make BinaryFile and Expression derive from AbstractExpression |
| 2 | ff10bf3 | test(07-01): pin AbstractExpression shape, ownership, metadata and operand order |
| 3 | 8eb2897 | docs(07-01): changelog and src/AGENTS.md for AbstractExpression |

## What was built

- `abstract_expression.h`: `AggregateOperation` at namespace scope (the header cannot include `expression_node.h`),
  `AbstractExpression` with public virtual dtor, protected default/copy/move, one pure `node()`, non-pure virtual
  `get_metadata()`, non-virtual `save`/`aggregate`/`aggregate_agents`/`select_agents`/`rename_agents`.
- `Expression final`: `explicit Expression(const AbstractExpression&)`, `explicit Expression(shared_ptr<ExpressionNode>)`,
  out-of-line `node()`. The 43 access-granting declarations are gone; the 43 free functions take `const AbstractExpression&`
  (58 occurrences in the header including the constructor).
- Operators collapse onto `binary_op` (expr/expr, expr/double, double/expr) and `unary_op`; `ifelse` reads c, t, e in order.
  A scalar takes `l->metadata()` (the node's), never `get_metadata()`.
- `BinaryFile : public AbstractExpression`; `node()` (in `expression_file.cpp`) returns `make_shared<ExpressionFile>(get_file_path())`;
  `get_metadata()` and `~BinaryFile()` marked `override`; `binary_file.cpp` untouched.
- Accessor call sites: C API line 248 and the Lua `metadata` lambda call `get_metadata()` (Lua name unchanged);
  `binary.cpp` comment reworded only (include kept, sol2 automagic unchanged, EQ probe green).
- Tests: `FileOperandsNeedNoWrapper` (tracer), `AbstractExpressionShape` (11 static_asserts), `FileMethodsNeedNoWrapper`,
  `FileStaysOpenAndReadableAfterSave`, `FailedSaveLeavesFileOpen`, `ExpressionOutlivesItsFile`, `UnopenedFileIsAnOperand`,
  `GetMetadataReturnsHandleOrNodeMetadata`, `NodeCalledOncePerOperandLeftToRight` (test-local `CountingExpression`);
  26 `.metadata()` -> `.get_metadata()` renames.
- CHANGELOG `[0.13.0]` Changed: two BREAKING C++ entries. `src/AGENTS.md`: file map, Expression bullet, comparison note,
  aggregation-enum lines, binary.cpp include rationale. The "Caches an open" line is left for the docs phase.

## Mutation checks (pins verified to bite)

- `save` using `node()->collect_input_files` instead of the `root` local: `FileOperandsNeedNoWrapper` dies with an access
  violation (0xc0000005) in MSVC Debug, so the dangling-leaf pin is real.
- `binary_op` evaluating `rhs.node()` before `lhs.node()`: `NodeCalledOncePerOperandLeftToRight` fails with
  `{"y","x","y"}` vs `{"x","y"}`. Both mutants reverted; the file matches the commit.

## Deviations from Plan

### Recorded design deviations (decided at plan time, restated as required)

1. **`get_metadata()` is virtual but not pure** on `AbstractExpression` (base body `node()->metadata()`, overridden only by
   `BinaryFile`). ROADMAP criterion 1's wording would allow reading it as a second pure virtual; a pure one would break
   "exactly one pure virtual", and a non-virtual one would let a call through `const AbstractExpression&` on a file read
   a temporary leaf. `GetMetadataReturnsHandleOrNodeMetadata` pins the address identity.
2. **ROADMAP criterion 3 amended**: `ifelse(file > 1.0, file, 0.0)` does not compile (no `double` overload of `ifelse` in
   C++, the C API, Lua or Julia) and none was added. The test uses `ifelse(a > 1.0, a, b)` on two raw files;
   `file_a + file_b` and `2.0 + file` are tested as written.

### Execution deviations

- **Tracer feedback gate**: config has `auto_advance: false`, which would make the tracer stop for a human-verify
  checkpoint. Per the user's standing instruction to verify approve/UAT gates adversarially instead of asking, the
  tracer's `<verify>` (the full gate) was re-run end to end and the two mutation checks above were added; execution then
  continued to the expansion tasks.
- Test helpers `expect_cells` and `expect_save_collision` were added to the test file's anonymous namespace (the file
  had no message-check idiom to reuse); the node() call log is named `calls`, not `log`, so it cannot shadow `quiver::log`.

Otherwise the plan executed as written.

## Flagged assumptions

- **FA-1** (unchanged, for the verifier): a `BinaryFile`'s `get_metadata()` returns the handle's in-memory metadata,
  which can differ from the `.toml` that `node()` reads (an unopened handle reports empty labels while `f * 2.0`
  carries the `.toml`'s). Pinned by `UnopenedFileIsAnOperand` and `GetMetadataReturnsHandleOrNodeMetadata`.
- **FA-2** resolved: MSVC (`/std:c++20 /permissive-`, Debug) built the base-parameter `==`/`!=` overloads with no
  C2666 and no warning. Nothing needed.

## Known Stubs

None.

## Self-Check: PASSED

- Files: `include/quiver/expression/abstract_expression.h` exists; all modified files present.
- Commits: c36ba26, ff10bf3, 8eb2897 present in `git log`.
