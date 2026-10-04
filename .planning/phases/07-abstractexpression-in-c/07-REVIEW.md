---
phase: 07-abstractexpression-in-c
reviewed: 2026-10-04T00:00:00Z
depth: standard
files_reviewed: 12
files_reviewed_list:
  - CHANGELOG.md
  - include/quiver/binary/binary_file.h
  - include/quiver/expression/abstract_expression.h
  - include/quiver/expression/expression.h
  - include/quiver/expression/expression_node.h
  - src/AGENTS.md
  - src/c/expression/expression.cpp
  - src/expression/expression.cpp
  - src/expression/expression_file.cpp
  - src/lua_runner/binary.cpp
  - src/lua_runner/expression.cpp
  - tests/test_expression.cpp
findings:
  critical: 0
  warning: 1
  info: 5
  total: 6
status: issues_found
---

# Phase 7: Code Review Report

**Reviewed:** 2026-10-04
**Depth:** standard
**Files Reviewed:** 12
**Status:** issues_found

## Summary

I reviewed the diff `c20c0d0..HEAD`. In it, `BinaryFile` and `Expression` now derive from `quiver::AbstractExpression`, the operators take `const AbstractExpression&`, `Expression::metadata()` becomes `get_metadata()`, and `AggregateOperation` moves to namespace scope.

The core refactor is correct:
- `binary_op` / `unary_op` call `node()` once per operand, left to right.
- `save()` holds the root in a local, so a file's temporary leaf stays alive.
- The scalar overloads take the node's metadata, so an unopened file works as an operand.
- The protected copy and move members on the base prevent slicing.
- `Expression(const Expression&)` still wins over the explicit `Expression(const AbstractExpression&)`.
- `==` / `!=` with base-class parameters resolve to the non-rewritten candidate whether or not the compiler implements P2468.
- A writer handle used as an operand fails loudly through `write_registry`. It is never read silently while unflushed.
- The C API and Lua edits are the mechanical rename only.

No BLOCKER was found. There is one real hazard in the new public base class: `get_metadata()` returns a reference that can dangle. The rest are documentation and coverage gaps.

## Narrative Findings (AI reviewer)

## Warnings

### WR-01: `AbstractExpression::get_metadata()` default body returns a reference into a temporary for any subclass that does not retain its node

**File:** `include/quiver/expression/abstract_expression.h:30-32`, `src/expression/expression.cpp:18-20`
**Issue:** The base body is `return node()->metadata();`. `node()` returns a `shared_ptr` by value, so the returned reference is valid only if something else keeps that node alive.
- `Expression` keeps it alive in `node_`.
- `BinaryFile` builds a fresh leaf on every call (`expression_file.cpp:30-32`). It is safe only because it overrides `get_metadata()`.

`AbstractExpression` is an exported, non-final base with a protected constructor, so it is built to be subclassed. The test suite does exactly that with `CountingExpression`. Any subclass shaped like `BinaryFile` (one that returns a freshly built node) that does not override `get_metadata()` gets a dangling `const BinaryMetadata&`. That is silent undefined behaviour, and nothing in the code catches it. A header comment is the only guard.

**Fix:** Remove the trap rather than document it. Make `get_metadata()` pure, and give the one implementation that can safely return a reference to `Expression`, which owns `node_`:
```cpp
// abstract_expression.h
virtual const BinaryMetadata& get_metadata() const = 0;

// expression.h (Expression)
const BinaryMetadata& get_metadata() const override;

// expression.cpp
const BinaryMetadata& Expression::get_metadata() const {
    return node_->metadata();
}
```
This conflicts with the ROADMAP wording "exactly one pure virtual `node()`". If that wording is binding, keep the base body but at minimum mark `Expression::get_metadata` as the only safe inheritor, and note the requirement on `AbstractExpression` in `src/AGENTS.md`. Moving the body is the cheaper and safer choice.

## Info

### IN-01: `get_metadata()` on a file and the operations on that file can disagree (deliberate per ROADMAP criterion 4)

**File:** `include/quiver/binary/binary_file.h:46`, `tests/test_expression.cpp:3046`
**Issue:** Through a `const AbstractExpression&`, `get_metadata()` on a `BinaryFile` returns the handle's in-memory metadata. That is empty for a `BinaryFile(path)` that was never opened. Every operation on the same object goes through `node()` and reads the TOML from disk instead.

Generic code such as `x.select_agents(x.get_metadata().labels)` therefore behaves differently for an unopened file than `Expression(x)` would. The same split appears if the TOML on disk is rewritten after the handle was opened. This is the specified behaviour and is pinned by `GetMetadataReturnsHandleOrNodeMetadata`, so it is not flagged as a bug.
**Fix:** None required now. Record the divergence in the `AbstractExpression` notes in `src/AGENTS.md` so Phase 8 does not assume that `x.get_metadata()` describes what `x`'s expressions compute.

### IN-02: A writer handle used as an operand fails with an error naming `open_file`, not `save`

**File:** `src/expression/expression.cpp:57-69` (save), `src/binary/binary_file.cpp:67-69`
**Issue:** Take `w`, opened `'w'`. Both `w.save(out)` and `(w * 2.0).save(out)` build a leaf without error. `save` then throws `Cannot open_file: file is already open for writing: <path>`. That breaks the convention that the Pattern 1 `{operation}` is the method the user actually called. `Expression(w)` behaved the same way before this phase, but `w.save(out)` now reads like "write this handle out", which makes the confusion more likely. No test covers a writer handle as an operand.
**Fix:** Add one test that pins the current error, or catch the case in `save` and rethrow it as `Cannot save: input file '<path>' is open for writing`.

### IN-03: The CHANGELOG BREAKING entry names only one form of the lost implicit conversion

**File:** `CHANGELOG.md:63-69`
**Issue:** The entry says only that `Expression e = file;` no longer compiles. Every other copy-initialization breaks the same way:
- passing a `BinaryFile` to a user function that takes `const Expression&`
- `return file;` from a function that returns `Expression`
- `std::vector<Expression>{file_a, file_b}`

The entry also leaves out that `Expression` is now `final`, so it can no longer be derived from.
**Fix:** Widen the sentence. For example: "a `BinaryFile` no longer converts implicitly to `Expression` (assignment, argument passing, return, brace lists); write `Expression(file)` or take `const AbstractExpression&`. `Expression` is now `final`."

### IN-04: `src/AGENTS.md` still names `Expression::save`, and the example keeps a wrapper that is no longer needed

**File:** `src/AGENTS.md:1009`, `src/AGENTS.md:1020`
**Issue:** Line 1009 lists `Expression::save` as a hot-path consumer. That function is now `AbstractExpression::save`. The canonical example at line 1020 still writes `sqrt(Expression(a))`, which shows the wrapper this phase made unnecessary.
**Fix:** Change line 1009 to `AbstractExpression::save`, and change the example to `abs((a + b) * 2.0 - sqrt(a))`.

### IN-05: The no-wrapper file tests check most operators only for "does not throw"

**File:** `tests/test_expression.cpp:2945-2960`
**Issue:** `FileMethodsNeedNoWrapper` checks values only for `+`, `-` (a - a), `==`, `aggregate`, `aggregate_agents`, `select_agents` and `rename_agents`. The other 16 file-operand forms are `EXPECT_NO_THROW({ auto e = ...; })`, which only shows that a node was built. A wrong `ExpressionBinary::Operation` in one of those overloads would pass. The risk is low, because every overload goes through the same `binary_op` / `unary_op` that the `Expression` tests cover with values.
**Fix:** Optional. Save one or two of them (for example `a / 2.0` and `a >= b`) and compare the cells, as the `+` / `==` cases already do.

---

_Reviewed: 2026-10-04_
_Reviewer: Claude (gsd-code-review)_
_Depth: standard_
