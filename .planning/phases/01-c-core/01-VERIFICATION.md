---
phase: 01-c-core
verified: 2026-05-06T00:00:00Z
status: human_needed
score: 7/7 must-haves verified (with 1 D-04 gap flagged for human decision)
overrides_applied: 0
human_verification:
  - test: "Confirm that D-04 mirror-case gap (CR-01) is acceptable for Phase 1 sign-off"
    expected: "Either (a) accept the gap because every test in TEST-01 passes today and every CORE-* truth is observably met, deferring CR-01 to a follow-up plan, OR (b) treat CR-01 as a CORE-correctness gap that must be closed inside Phase 1 because it allows silent wrong outputs when an input metadata combination has a time dim on rhs but a non-time same-name dim on lhs"
    why_human: "CR-01 is a broadcast-validation correctness bug discovered by gsd-code-reviewer. It does not break any currently exercised truth (no test triggers the inverse direction; D-04 is verified by TimePropertiesMismatchThrows in the lhs-time direction). Whether it constitutes a Phase 1 gap or a Phase 2 cleanup depends on whether the project owner judges 'silent wrong output on a documented decision rule' as in-scope for the phase goal."
human_verification_advisory:
  - finding: "CR-02: open_file('w', ...) leaks canonical path into write_registry on partial-construction failure"
    impact: "Quality bug, not a goal-failure. No Phase 1 must-have or test exercises this path."
  - finding: "CR-03: New BinaryOpNode runtime_error messages use 'Cannot apply binary operation: …' which is not a C++ method name"
    impact: "Cosmetic violation of CLAUDE.md error-pattern rule. Bindings (deferred milestone) would surface unchanged. No truth fails today."
---

# Phase 01: c-core Verification Report

**Phase Goal:** Deliver a working `quiver::expr` subsystem in the C++ core — `Expression` value type, virtual `Node` AST (FileNode / ScalarNode / BinaryOpNode), four binary operators, scalar broadcast, and a row-by-row `save(path)` engine that materializes any expression tree into a new `.qvr` file.
**Verified:** 2026-05-06
**Status:** human_needed
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths

| #  | Truth                                                                                                        | Status     | Evidence                                                                                                                                                                                                                              |
| -- | ------------------------------------------------------------------------------------------------------------ | ---------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 1  | User can construct an Expression from a BinaryFile via implicit conversion (CORE-01)                         | VERIFIED   | `include/quiver/expr/expression.h:23` declares `Expression(const BinaryFile& file)` non-explicit. `IdentityFile` test (test_expression.cpp:102) constructs `Expression e = a;` and `[ OK ]`s.                                          |
| 2  | Two Expressions compose with operator+/-/*// (CORE-02)                                                       | VERIFIED   | 12 free-function operator overloads in `expression.h:52-66`, defined in `expression.cpp:111-149`. Tests AddTwoFiles, SubtractTwoFiles, MultiplyTwoFiles, DivideTwoFiles all pass.                                                       |
| 3  | Scalar literals broadcast on either side: `a + 2.0` and `2.0 + a` (CORE-03)                                  | VERIFIED   | `scalar_left` / `scalar_right` helpers in `expression.cpp:99-107`. All 8 ScalarBroadcast{Add,Subtract,Multiply,Divide}{Left,Right} tests pass.                                                                                          |
| 4  | Operators chain into trees of arbitrary depth: `((a + b) - c) / 2.0` (CORE-04)                                | VERIFIED   | `Chained` test (test_expression.cpp:227) builds `((a + b) * 2.0) - c` tree and verifies every cell. `SamePathTwice` (270) verifies D-10 no-caching across same-path FileNodes.                                                          |
| 5  | `expression.save(path)` materializes the lazy tree to a new .qvr/.toml file (CORE-05)                        | VERIFIED   | `Expression::save` in `expression.cpp:58-87`. Tests SaveProducesReadableFile, SaveOpenedTwiceProducesSameOutput pass; metadata round-trips through the TOML sidecar.                                                                    |
| 6  | save() inner row-loop reuses ONE std::vector<double> buffer across iterations (CORE-06)                       | VERIFIED   | Grep audit: `expression.cpp:78` declares `std::vector<double> row;` ONCE outside the `for(;;)` loop. `node.h:123-126` declares 4 `mutable std::vector` buffer members on BinaryOpNode + 1 on FileNode (own_dims_buf_). LargeGridCompletes runtime backstop completes in 244ms (5s budget). |
| 7  | C++ test suite verifies arithmetic correctness end-to-end (TEST-01)                                          | VERIFIED   | 29 ExpressionFixture cases (5 baseline + 4 arithmetic + 5 error rejection from Plan 02; 3 op + 7 scalar + 4 broadcast + 1 large-grid from Plan 03). `quiver_tests.exe --gtest_filter="ExpressionFixture.*"` reports 29/0 passed. Full `quiver_tests.exe` reports 781/0 passed across 34 suites. |

**Score:** 7/7 truths verified (CORE-01..06 + TEST-01).

### Required Artifacts

| Artifact                                | Expected                                                                                  | Status   | Details                                                                                                                                                                                                                                                                                                          |
| --------------------------------------- | ----------------------------------------------------------------------------------------- | -------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `include/quiver/binary/iteration.h`     | Free-function row iterators in `quiver::binary` namespace                                 | VERIFIED | Exists (29 lines). Declares `first_dimensions(BinaryMetadata)` and `next_dimensions(BinaryMetadata, vector<int64_t>) -> optional`. Both `QUIVER_API`-decorated.                                                                                                                                                  |
| `src/binary/iteration.cpp`              | Implementation of first_dimensions / next_dimensions + lifted helper                      | VERIFIED | Exists (143 lines). Anonymous-namespace helper `dimension_sizes_at_values(meta, values)`. Both free functions implemented.                                                                                                                                                                                       |
| `include/quiver/binary/binary_file.h`   | 3 new public method decls: `read(vector<int64_t>)`, `read_into`, `write(data, vec<int64_t>)` | VERIFIED | All 3 declarations present (lines 42-50). Trusted-caller docstring on `read_into` (lines 44-47). 2 indexed helper decls (`calculate_file_position_indexed`, `validate_dimension_values_indexed`).                                                                                                                |
| `src/binary/binary_file.cpp`            | Implementations of new overloads + helpers + delegate refactor                            | VERIFIED | All implementations present at the documented line numbers. `BinaryFile::next_dimensions` (line 381) is now a 6-line delegate to `quiver::binary::next_dimensions` translating `nullopt` → `first_dimensions(meta)`.                                                                                              |
| `include/quiver/expr/expression.h`      | Expression value type + 12 operator overload decls                                        | VERIFIED | Exists (70 lines). Class with non-explicit `Expression(const BinaryFile&)`, explicit `Expression(shared_ptr<Node>)`, `metadata()`, `save(path)`, `node()`. 12 free-function operator overloads.                                                                                                                  |
| `include/quiver/expr/node.h`            | Node virtual base + FileNode + ScalarNode + BinaryOpNode + BinaryOp enum                  | VERIFIED | Exists (131 lines). `Node` virtual base with pure-virtual `metadata()` + `compute_row()`. 3 concrete subclasses + `enum class BinaryOp { Add, Subtract, Multiply, Divide }`. `BinaryFile` forward-declared.                                                                                                       |
| `src/expr/expression.cpp`               | Expression impl + save engine with D-11 + 12 operator overloads                            | VERIFIED | Exists (152 lines). `collect_file_paths` walks AST via dynamic_cast (lines 27-38). `save()` performs eager D-11 self-save check via `weakly_canonical` (lines 65-70) BEFORE opening writer. Row loop allocates `dims` and `row` exactly once outside the loop.                                                  |
| `src/expr/nodes.cpp`                    | FileNode + ScalarNode + BinaryOpNode definitions; BinaryOpNode 6-step validation chain     | VERIFIED | Exists (323 lines). FileNode lazy-opens via `read_into` (D-13 trusted path). ScalarNode broadcasts via `out.assign`. BinaryOpNode: D-07 unit (109-112), D-01/D-06 shape (117-130), D-04 time props (133-151) — see WARNING below, D-09 initial_datetime (154-158), D-08 labels (161-179), D-02 dim union (181-244), translation tables (246-259). |
| `tests/test_expression.cpp`             | GoogleTest suite; ~29 cases mapping 1:1 to every CORE-* and D-* decision                  | VERIFIED | Exists (854 lines). 29 `TEST_F(ExpressionFixture, ...)` cases enumerated; verified passing via `--gtest_filter="ExpressionFixture.*"` (29/0).                                                                                                                                                                  |
| `tests/test_iteration.cpp`              | 5 GoogleTest cases for free-function iterators                                            | VERIFIED | Exists (86 lines). 5 `TEST(IterationTest, ...)` cases covering FirstDimensionsOnNonTime, FirstDimensionsOnTime, NextDimensionsTraversesNonTime, NextDimensionsReturnsNullOptOnEnd, NextDimensionsTraversesTimeMetadata.                                                                                          |
| `tests/test_binary_file.cpp`            | Extension with 9 D-13/D-14 fast-overload cases                                            | VERIFIED | 613 lines total. 9 new TEST_F cases under `// Fast Overloads (D-13/D-14)` divider verified passing via filter (FastReadOverload* / ReadInto* / FastWriteOverload* = 9/0 passed).                                                                                                                                |
| `src/CMakeLists.txt`                    | `binary/iteration.cpp`, `expr/expression.cpp`, `expr/nodes.cpp` in QUIVER_SOURCES         | VERIFIED | Lines 24, 29, 30 confirmed.                                                                                                                                                                                                                                                                                       |
| `tests/CMakeLists.txt`                  | `test_iteration.cpp` and `test_expression.cpp` in quiver_tests source list                | VERIFIED | Lines 7, 9 confirmed.                                                                                                                                                                                                                                                                                            |

### Key Link Verification

| From                                       | To                                                                       | Via                                                            | Status   | Details                                                                                                                                                                  |
| ------------------------------------------ | ------------------------------------------------------------------------ | -------------------------------------------------------------- | -------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `Expression::save(path)`                   | `quiver::binary::first_dimensions / next_dimensions / BinaryFile::write` | row-loop in save engine                                        | WIRED    | `expression.cpp:77` calls `first_dimensions`; `:82` calls `next_dimensions`; `:81` calls `writer.write(row, dims)` (D-14 fast write).                                    |
| `FileNode::compute_row`                    | `BinaryFile::read_into`                                                  | trusted-caller fast path                                        | WIRED    | `nodes.cpp:37` calls `file_->read_into(dims, out, /*allow_nulls=*/true)`. Lazy-open at line 30 (D-10).                                                                   |
| `operator+(const Expression&, double)`     | `ScalarNode + BinaryOpNode`                                              | `scalar_right` helper                                           | WIRED    | `expression.cpp:104-107` constructs `ScalarNode(rhs, lhs.metadata())` then `BinaryOpNode(op, lhs.node(), scalar_node)`. ScalarNode adopts sibling metadata for broadcast. |
| `Expression::save(path)` D-11 check        | `weakly_canonical` over output + every FileNode path                     | eager pre-check throws BEFORE `open_file('w', ...)`              | WIRED    | `expression.cpp:63-70`: `collect_file_paths` walks tree, canonicalizes, throws BEFORE `BinaryFile::open_file(path, 'w', meta)` at line 74. SelfSaveCollisionThrows confirms input file is byte-unchanged after the throw. |
| `BinaryFile::next_dimensions` (protected)  | `quiver::binary::next_dimensions` (free function)                        | delegate, returns `first_dimensions(meta)` when free returns nullopt | WIRED    | `binary_file.cpp:381-392`: 6-line delegate translating `nullopt` to `first_dimensions(meta)` so csv_converter loop semantics preserved. CSVConverter regression tests still green. |
| `csv_converter.cpp:85, 130`                | `BinaryFile::next_dimensions` delegate                                   | `if (current_dimensions == initial_dimensions) break;`           | WIRED    | Implicit verification: full quiver_tests.exe (781/0 passed) includes 37 CSVConverterFixture regression tests.                                                              |

### Data-Flow Trace (Level 4)

| Artifact                          | Data Variable                       | Source                                                              | Produces Real Data | Status     |
| --------------------------------- | ----------------------------------- | ------------------------------------------------------------------- | ------------------ | ---------- |
| `Expression::save`                | `row` (vector<double>)              | `node_->compute_row(dims, row)` populates per iteration             | Yes                | FLOWING    |
| `BinaryOpNode::compute_row`       | `lhs_buf_` / `rhs_buf_`             | `lhs_->compute_row(lhs_dims_buf_, lhs_buf_)` and rhs equivalent     | Yes                | FLOWING    |
| `FileNode::compute_row`           | `out` (vector<double>)              | `file_->read_into(dims, out, true)` reads from on-disk .qvr binary  | Yes                | FLOWING    |
| `ScalarNode::compute_row`         | `out` (vector<double>)              | `out.assign(broadcast_meta_.labels.size(), value_)`                 | Yes (uniform broadcast) | FLOWING |
| `BinaryOpNode::broadcast_meta_`   | dimensions / labels / unit / etc.   | Built in ctor from lhs_meta + rhs_meta (D-02 dim union)             | Yes                | FLOWING    |
| `Expression::metadata()`          | BinaryMetadata                      | Delegates to `node_->metadata()` which returns subtree metadata     | Yes                | FLOWING    |

### Behavioral Spot-Checks

| Behavior                                                                       | Command                                                                                                                                                                                                | Result                                                            | Status |
| ------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | ----------------------------------------------------------------- | ------ |
| ExpressionFixture suite (TEST-01 contract: arithmetic correctness end-to-end) | `./build/bin/quiver_tests.exe --gtest_filter="ExpressionFixture.*"`                                                                                                                                    | 29 tests run, 29 passed, 0 failed                                 | PASS   |
| IterationTest suite (D-12 free-function iterators)                            | `./build/bin/quiver_tests.exe --gtest_filter="IterationTest.*"`                                                                                                                                        | 5 tests run, 5 passed, 0 failed                                   | PASS   |
| Fast-overload regression suite (D-13/D-14)                                    | `./build/bin/quiver_tests.exe --gtest_filter="BinaryTempFileFixture.FastReadOverload*:BinaryTempFileFixture.ReadInto*:BinaryTempFileFixture.FastWriteOverload*"`                                       | 9 tests run, 9 passed, 0 failed                                   | PASS   |
| Full quiver_tests suite (no regressions in pre-existing 752+ tests)            | `./build/bin/quiver_tests.exe`                                                                                                                                                                         | 781 tests run across 34 suites in 9.7s, 0 failed                  | PASS   |
| LargeGridCompletes wall-clock smoke (CORE-06 backstop)                         | `./build/bin/quiver_tests.exe --gtest_filter="ExpressionFixture.LargeGridCompletes"`                                                                                                                   | 1 test, passed in 244 ms (budget 5000 ms)                         | PASS   |
| Self-save D-11 (input byte-unchanged after rejection)                          | `./build/bin/quiver_tests.exe --gtest_filter="ExpressionFixture.SelfSaveCollisionThrows*"`                                                                                                             | 2 tests, passed (29 ms + 41 ms)                                   | PASS   |

### Requirements Coverage

| Requirement | Source Plan(s)              | Description                                                                                          | Status     | Evidence                                                                                                                            |
| ----------- | --------------------------- | ---------------------------------------------------------------------------------------------------- | ---------- | ----------------------------------------------------------------------------------------------------------------------------------- |
| CORE-01     | 01-02                       | Construct Expression from BinaryFile via implicit conversion                                          | SATISFIED  | `Expression(const BinaryFile&)` non-explicit (expression.h:23). IdentityFile test passes.                                            |
| CORE-02     | 01-02, 01-03                | Compose two Expressions with `+ - * /`                                                               | SATISFIED  | 4-case op matrix: AddTwoFiles, SubtractTwoFiles, MultiplyTwoFiles, DivideTwoFiles — all pass.                                        |
| CORE-03     | 01-02, 01-03                | Scalar literals broadcast on either side                                                             | SATISFIED  | 8 ScalarBroadcast{op}{Left,Right} tests pass.                                                                                        |
| CORE-04     | 01-02, 01-03                | Chain operators into trees of arbitrary depth                                                         | SATISFIED  | Chained test (`((a + b) * 2.0) - c`) and SamePathTwice pass.                                                                        |
| CORE-05     | 01-02, 01-03                | `expression.save(path)` materializes lazy tree to .qvr                                                | SATISFIED  | SaveProducesReadableFile, SaveOpenedTwiceProducesSameOutput, SelfSaveCollisionThrows{,WithCanonicalizedPath} all pass.              |
| CORE-06     | 01-01, 01-02                | Materialization iterates row-by-row reusing per-row buffers                                          | SATISFIED  | Grep audit confirms single-buffer declarations outside loops; LargeGridCompletes provides 244ms backstop. `mutable std::vector` members declared once. |
| TEST-01     | 01-03                       | C++ test suite verifies arithmetic correctness end-to-end                                             | SATISFIED  | 29 ExpressionFixture cases all pass; full quiver_tests.exe = 781/0.                                                                  |

**ORPHANED requirements:** None — every requirement ID in REQUIREMENTS.md mapped to Phase 1 is claimed by at least one plan and verified.

### Anti-Patterns Found

| File                              | Line(s)             | Pattern                                          | Severity | Impact                                                                                                                                                                                          |
| --------------------------------- | ------------------- | ------------------------------------------------ | -------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `src/expr/nodes.cpp`              | 132-151             | D-04 only fires when time dim is on lhs (CR-01)  | WARNING  | A symmetric loop over rhs is missing. When lhs has dim X as non-time and rhs has X as time, validation silently passes and the time semantics are dropped. NO TEST currently triggers this; no observable failure today. |
| `src/binary/binary_file.cpp`      | 81-100              | `write_registry` leak on partial-construction (CR-02) | WARNING  | If `to_toml()`, fstream ctor, or `fill_file_with_nulls` throws after registry insert, the canonical path remains in the static registry indefinitely. No test currently exercises this path.    |
| `src/expr/nodes.cpp`              | 110, 127, 141, 148, 157, 168, 177 | New runtime_error messages don't match the 3 CLAUDE.md patterns (CR-03) | WARNING | "Cannot apply binary operation: …" is not a C++ method name. Cosmetic violation; bindings (deferred) would surface unchanged. |
| `src/binary/iteration.cpp`        | 17-91 vs `binary_file.cpp:394-464` | `dimension_sizes_at_values` duplicated verbatim (WR-03) | INFO     | 70-line copy-paste flagged in code review. Will diverge under future calendar-rule changes. Not a goal-failure issue.                                                                          |
| `src/binary/binary_file.cpp`      | 263-267             | `validate_file_is_open()` is dead code (WR-02)   | INFO     | Declared, defined, never called. CLAUDE.md "delete unused code". Pre-existing or new — minor cleanup. Not a goal-failure issue.                                                                |
| `src/binary/iteration.cpp`        | 104-140             | `next_dimensions` rebuilds `first_dimensions(meta)` per call (WR-09) | INFO | Performance note in hot path. Doesn't affect correctness; LargeGridCompletes still well under budget (244ms). |
| `src/expr/nodes.cpp`              | 283-310             | O(n²) inner search in `compute_row` (WR-06)      | INFO     | `lhs_dim_translate_` stores the inverse mapping; could pre-compute forward mapping to make compute_row a single pass. Performance-only.                                                          |

### Human Verification Required

Two finds were promoted from gsd-code-reviewer's BLOCKER list to "human decision" because they are correctness-adjacent but do not fail any currently exercised truth or test in this phase.

#### 1. CR-01 (D-04 mirror-case gap)

**Test:** Inspect `src/expr/nodes.cpp:132-151`. Confirm whether the D-04 validation chain catches the case where `lhs` has dim X as a non-time dim and `rhs` has X as a time dim.

**Expected behavior (per D-04):** "When both sides have a same-name time dimension, TimeProperties must match exactly." The implication of the locked decision is that any inconsistent time-ness on a same-name dim should reject — both directions.

**Actual behavior:** Only the lhs-time / rhs-non-time branch fires (line 140). The mirror case (lhs-non-time / rhs-time) is never caught: line 134 skips non-time lhs dims, so the entire match against rhs is bypassed. The `TimePropertiesMismatchThrows` test only covers the lhs-time direction. Resulting expression metadata silently uses the lhs (non-time) dim, demoting the rhs time semantics.

**Why human:** No truth fails today (all 7 CORE/TEST truths verified, all 781 tests green). The bug only surfaces when the inputs themselves have asymmetric metadata — a configuration the current test suite does not exercise. The phase owner must decide:

- (a) **Accept and defer:** Phase 1 closes; CR-01 becomes a follow-up plan. Justification: every observable behavior in the success criteria works today; the gap only manifests on inputs the test suite has not yet been extended to cover.
- (b) **Treat as a Phase 1 gap:** Add a Phase 1 follow-up plan that (i) adds the symmetric loop, (ii) adds a regression test for the mirror direction, before closing the phase. Justification: a documented decision (D-04) is incompletely enforced and could ship a "correct" expression that materializes wrong outputs.

If (b), the suggested fix is the snippet from `01-REVIEW.md` (CR-01 section): an additional 9-line loop over rhs that mirrors the existing lhs-driven check, plus a 1-test addition mirroring `TimePropertiesMismatchThrows` with rhs-as-time-dim.

#### 2. CR-02 (write_registry leak — quality bug, advisory)

Per the verifier's task brief, CR-02 is flagged for awareness but does NOT require human decision because (i) it does not fail any Phase 1 truth, (ii) it is a defensive-resilience issue (the registry self-corrects on fresh process restart), and (iii) no test exercises the partial-construction failure path. Treat as a follow-up plan candidate.

#### 3. CR-03 (error message patterns — cosmetic, advisory)

Per the verifier's task brief, CR-03 does NOT fail any Phase 1 truth. Bindings are deferred. The phrase "Cannot apply binary operation" is human-readable and unambiguous; the deviation from CLAUDE.md's three patterns is structural rather than informational. Treat as a follow-up plan candidate.

### Gaps Summary

**No gaps that fail any Phase 1 must-have truth or test.** All 7 truths VERIFIED; every CORE-* and TEST-01 requirement satisfied; every artifact exists, is substantive, is wired, and produces real data; every key link wired.

The phase delivers everything the Phase 1 goal demanded: `Expression e = a` constructs, `(a + b) * 2.0` composes and saves, `((a + b) - c) / 2.0` materializes correctly, and the inner loop reuses a single buffer (verifiable in code review and runtime via LargeGridCompletes).

The single human-decision item (CR-01) is a partially-enforced D-04 invariant whose impact is observable only on input configurations that the test suite does not exercise. The phase owner decides whether D-04's full enforcement is in-scope for Phase 1 or rolled into a follow-up plan.

---

_Verified: 2026-05-06_
_Verifier: Claude (gsd-verifier)_
