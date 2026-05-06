---
phase: 01-c-core
plan: 02
subsystem: expr

tags: [cpp, expression, ast, save-engine, broadcasting, pimpl-rule, rule-of-zero, mutable-buffers, gtest, tdd]

# Dependency graph
requires:
  - phase: 01-c-core
    provides: "quiver::binary::first_dimensions / next_dimensions iterators; BinaryFile fast read/read_into/write overloads (D-13/D-14)"
provides:
  - "quiver::expr::Expression value type (Rule of Zero, wraps shared_ptr<Node>) — public C++ API for the lazy-expression subsystem"
  - "quiver::expr::Node virtual base + FileNode (lazy-open .qvr leaf, D-10) + ScalarNode (broadcast scalar leaf) + BinaryOpNode (validating internal node)"
  - "quiver::expr::BinaryOp { Add, Subtract, Multiply, Divide } enum"
  - "12 free-function operator overloads at namespace scope: operator+/-/*// for (Expression,Expression) / (Expression,double) / (double,Expression)"
  - "Expression::save(path) row-by-row engine with D-11 self-save check + single-buffer reuse (CORE-06)"
  - "BinaryOpNode eager validation chain (D-03): D-07 unit -> D-01/D-06 shape (n*n / 1*n / n*1) -> D-04 time-properties exact match -> D-09 initial_datetime equality -> D-08 labels-axis broadcast -> defense-in-depth BinaryMetadata::validate()"
  - "Pre-computed translation tables on BinaryOpNode (D-05) + size-1 broadcast clamping in compute_row"
  - "Mutable-buffer pattern for compute_row: BinaryOpNode owns lhs_dims_buf_ / rhs_dims_buf_ / lhs_buf_ / rhs_buf_ as `mutable` members; FileNode owns file_ / own_dims_buf_; reused across rows"
  - "ExpressionFixture GoogleTest fixture (programmatic .qvr generator + read_all_cells helper) — 14 baseline + extension test cases"
affects:
  - "01-03 (final test fill-out — adds ~20+ remaining cases from VALIDATION.md to tests/test_expression.cpp on top of the 14 already present)"

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "First public virtual hierarchy in the codebase (Node + 3 concrete subclasses) — every class decorated with QUIVER_API to prevent 'missing vtable' link errors on Windows shared builds"
    - "Mutable-member buffer reuse pattern: const compute_row uses mutable buffers logically owned by the node; documented as NOT thread-safe (project policy of single-threaded use)"
    - "Forward-declared cross-module type (BinaryFile in node.h) keeps include graphs slim — full include only in nodes.cpp where the body lives"
    - "Eager validation chain in constructor with cheapest-first ordering (string compare -> dim name lookup -> vector traversal -> single compare -> vector compare)"
    - "Free-function operator overloads on a public C++ value type — first instance in the codebase; three private anon-namespace dispatch helpers (make_binop / scalar_left / scalar_right) keep 12 overloads at one-line bodies"

key-files:
  created:
    - "include/quiver/expr/expression.h — public Expression value type + 12 operator overload decls (Rule of Zero, implicit BinaryFile -> Expression conversion is the only intentional implicit conversion in the public API per CORE-01)"
    - "include/quiver/expr/node.h — Node virtual base + FileNode + ScalarNode + BinaryOpNode + BinaryOp enum; mutable buffer members documented; FileNode::path() / BinaryOpNode::lhs() / rhs() public accessors used by Expression::save's D-11 walk"
    - "src/expr/expression.cpp — Expression methods (impl), save engine with D-11 self-save check + CORE-06 single-buffer row loop, anon-ns collect_file_paths walking the AST via dynamic_cast, anon-ns make_binop/scalar_left/scalar_right helpers + 12 operator definitions"
    - "src/expr/nodes.cpp — FileNode lazy-open + read_into trusted path (D-13); ScalarNode out.assign broadcast; BinaryOpNode 6-step validation, broadcast_meta_ build (D-02), parent_dimension_index recompute by name (Pitfall 6), translation tables, compute_row with size-1 clamp + label-axis broadcast (D-08)"
    - "tests/test_expression.cpp — ExpressionFixture (14 TEST_F cases): 5 baseline (CORE-01, CORE-05, D-11) + 4 arithmetic (CORE-02 / CORE-03 / CORE-04 / SamePathTwice exercising D-10) + 5 error rejections (D-01/D-06, D-07, D-04, D-09, D-08)"
  modified:
    - "src/CMakeLists.txt — appended expr/expression.cpp + expr/nodes.cpp to QUIVER_SOURCES"
    - "tests/CMakeLists.txt — appended test_expression.cpp to quiver_tests sources (after test_binary_time_properties.cpp)"

key-decisions:
  - "Expression(const BinaryFile&) is non-explicit per CORE-01: 'Expression e = a;' is the canonical user-facing pattern. The other ctor (shared_ptr<Node>) is explicit. This is the only intentional implicit conversion in the public API."
  - "Public Node hierarchy (Claude's-discretion default 1 in 01-CONTEXT.md): every Node subclass is QUIVER_API-decorated. Forward-decl BinaryFile in quiver namespace inside node.h; full include only in nodes.cpp."
  - "ScalarNode adopts the sibling Expression's metadata at construction (passed in by the operator overload helper). compute_row writes the literal across the labels axis via out.assign(labels.size(), value_) — broadcasts uniformly."
  - "BinaryOpNode pre-computes per-output-dim translation tables (lhs_dim_translate_, rhs_dim_translate_, lhs_dim_sizes_, rhs_dim_sizes_) at construction, not on every compute_row call. Inner search to map operand dim index -> output dim index is O(out_dims) per operand per row — fine for typical 2-3 dims."
  - "Size-1 broadcast clamp in compute_row: if operand's size at a dim is 1, force coord = 1 (matches BinaryFile's validate_dimension_values_indexed contract `value < 1 || value > dim.size`). Documented edge case: this is correct for non-time dims and for outer time dims with initial_value=1; rare inner-time-dim configs where initial_value > 1 with size 1 are not exercised in v1 (Phase 1 tests don't construct them)."
  - "Recompute parent_dimension_index by NAME when building broadcast_meta_ (Pitfall 6): the source operand's parent index is translated to the new index via output_index_by_name lookup. Defense-in-depth broadcast_meta_.validate() runs at the end of the ctor."
  - "D-11 implementation walks the AST via collect_file_paths (recursive dynamic_cast on FileNode + BinaryOpNode), canonicalizes input + output paths via std::filesystem::weakly_canonical (same canonicalization the BinaryFile write registry uses, mitigating T-1-202), and throws BEFORE BinaryFile::open_file('w', ...) so an input collision cannot trigger fill_file_with_nulls (verified by SelfSaveCollisionThrows asserting input file size + first 64 bytes are unchanged after the throw)."
  - "Three private anon-namespace dispatch helpers in expression.cpp (make_binop / scalar_left / scalar_right) reduce the 12 operator definitions to one-line bodies, keeping operator semantics in one place."
  - "FileNode::path / BinaryOpNode::lhs / rhs are inline accessors in node.h (no separate cpp impl) per the project's value-type / Rule-of-Zero pattern."

patterns-established:
  - "Pattern A: First public virtual hierarchy in the codebase. Every Node subclass decorated with QUIVER_API. Future virtual public hierarchies (if any) should mirror this — apply QUIVER_API to base + every subclass to avoid missing-vtable link errors on Windows shared builds."
  - "Pattern B: Mutable-member buffer reuse for compute_row. const method, mutable buffer members, single-threaded contract documented inline. First use of `mutable` in the codebase; future hot-path methods that need const + buffer reuse should follow this idiom."
  - "Pattern C: Eager validation chain in BinaryOpNode constructor uses Pattern-1 throws (`Cannot apply binary operation: {reason}`) ordered cheapest-first. Establishes the project's preference for fail-fast at construction over save() time."
  - "Pattern D: Self-save / overwrite-protection via canonicalized path comparison BEFORE opening the writer. AST walk via dynamic_cast collects all input paths; uses the same std::filesystem::weakly_canonical the BinaryFile write registry uses."
  - "Pattern E: Anon-namespace dispatch helpers (make_binop / scalar_left / scalar_right) for free-function operator overload sets. Reduces N overload definitions to one-line bodies."

requirements-completed:
  - CORE-01
  - CORE-02
  - CORE-03
  - CORE-04
  - CORE-05
  - CORE-06

# Metrics
duration: ~22m
completed: 2026-05-06
---

# Phase 01 Plan 02: Expression AST + save engine Summary

**`quiver::expr` subsystem in C++ core: Expression value type, Node virtual hierarchy (FileNode/ScalarNode/BinaryOpNode), 12 free-function operator overloads (`+ - * /` for Expr-Expr and double-Expr in both orders), and a row-by-row save engine with D-11 self-save check and single-buffer reuse — all six CORE requirements (CORE-01..06) covered by 14 passing GoogleTest cases.**

## Performance

- **Duration:** ~22m (start 2026-05-06T13:17:36Z, end 2026-05-06T13:39:19Z; ~6m of which was the initial CMake configure that fetched dependencies)
- **Started:** 2026-05-06T13:17:36Z
- **Completed:** 2026-05-06T13:39:19Z
- **Tasks:** 8 (Tasks 1-7 produced atomic commits; Task 8 was a verification gate)
- **Files modified:** 7 (5 created, 2 modified)

## Accomplishments

- Two new public headers (`include/quiver/expr/expression.h`, `include/quiver/expr/node.h`) defining the first public virtual hierarchy in the codebase plus the user-facing Expression value type with 12 free-function operator overloads.
- Two new implementation files (`src/expr/expression.cpp`, `src/expr/nodes.cpp`) totaling ~470 lines: the row-by-row save engine with eager D-11 self-save check, the FileNode lazy-open / ScalarNode broadcast / BinaryOpNode 6-step validation, broadcast metadata builder with parent_dimension_index recompute, pre-computed translation tables, and apply_op dispatch.
- New `tests/test_expression.cpp` with the `ExpressionFixture` GoogleTest fixture, programmatic `write_qvr` / `read_all_cells` helpers, and 14 TEST_F cases. Five baseline cases (CORE-01, CORE-05, D-11) + four arithmetic cases (CORE-02, CORE-03, CORE-04, SamePathTwice) + five error rejection cases (D-01/D-06, D-07, D-04, D-09, D-08).
- Full `quiver_tests.exe` suite green: 766 tests passed / 0 failed (752 pre-existing + 14 ExpressionFixture). No regression in the binary or database subsystems.
- CORE-06 single-buffer-reuse design preserved: `Expression::save` declares `std::vector<int64_t> dims` and `std::vector<double> row` exactly once outside the row loop. `BinaryOpNode` reuses mutable `lhs_dims_buf_ / rhs_dims_buf_ / lhs_buf_ / rhs_buf_` members. `FileNode::read_into` (D-13 trusted-caller fast path) writes into the caller-provided buffer without allocating.

## Task Commits

Tasks 1-7 each landed an atomic commit; Tasks 1-3 were RED stages of the TDD cycle (failing test / declarations only) and Tasks 4-6 were GREEN stages (implementations). Task 7 added an extension batch of tests on top of the green baseline. Task 8 was verification-only.

1. **Task 1: Wave-0 test scaffold + CMake registration** — `f8f050f` test(01-02): add failing tests for Expression scaffold
   - Created `tests/test_expression.cpp` with `ExpressionFixture` + 5 baseline TEST_F cases (CORE-01, CORE-05, D-11). Registered in `tests/CMakeLists.txt`. Build expectation (verified): `fatal error: quiver/expr/expression.h: No such file or directory` — RED.
2. **Task 2: node.h public header (declarations)** — `5527bf5` test(01-02): add Node, FileNode, ScalarNode, BinaryOpNode public header
   - First public virtual hierarchy in the codebase. All 4 classes decorated with `QUIVER_API`. `BinaryFile` forward-declared in `quiver` namespace (no transitive include). `enum class BinaryOp { Add, Subtract, Multiply, Divide }`. `mutable` buffer members on BinaryOpNode + FileNode documented as NOT thread-safe.
3. **Task 3: expression.h public header (declarations)** — `fca0cf1` test(01-02): add Expression public header + 12 operator overload decls
   - `Expression(const BinaryFile&)` intentionally non-explicit (CORE-01 implicit conversion). 12 free-function operator overloads at namespace scope, all `QUIVER_API`. `Expression(std::shared_ptr<Node>)` is explicit. Build expectation (verified): linker fails with `undefined reference to quiver::expr::Expression::Expression`, etc. — RED progresses to link-stage.
4. **Task 4: nodes.cpp implementation** — `23eaa52` feat(01-02): implement FileNode, ScalarNode, BinaryOpNode
   - FileNode lazy-opens via `BinaryFile::open_file(path_, 'r')` and uses `read_into` (D-13). ScalarNode `out.assign(labels.size(), value_)`. BinaryOpNode constructor: D-07 unit -> D-01/D-06 shape -> D-04 time props -> D-09 initial_datetime -> D-08 labels broadcast -> build broadcast_meta_ (D-02) with parent_dimension_index recompute by name (Pitfall 6) -> defense-in-depth `validate()`. Pre-computes translation tables and per-dim sizes for compute_row clamp. compute_row applies size-1 broadcast clamp + recursive operand dispatch + D-08 label-axis broadcast.
5. **Task 5: expression.cpp implementation + accessors on node.h** — `8c78e33` feat(01-02): implement Expression + save engine + 12 operator overloads
   - `Expression::save` walks the AST via `collect_file_paths` (recursive `dynamic_cast`), canonicalizes input + output paths via `std::filesystem::weakly_canonical`, throws Pattern-1 BEFORE opening the writer (D-11). Row loop declares `dims` and `row` ONCE outside `for(;;)` (CORE-06). 12 operator definitions backed by 3 anon-namespace dispatch helpers. Added `FileNode::path()`, `BinaryOpNode::lhs()` / `rhs()` inline accessors to `node.h`.
6. **Task 6: CMake wiring + 5 baseline tests green** — `f54d54e` feat(01-02): wire expr sources into CMake; baseline tests green
   - Appended `expr/expression.cpp` and `expr/nodes.cpp` to `QUIVER_SOURCES`. Reconfigured cmake. `quiver_tests.exe --gtest_filter=ExpressionFixture.{IdentityFile,SaveProducesReadableFile,SaveOpenedTwiceProducesSameOutput,SelfSaveCollisionThrows,SelfSaveCollisionThrowsWithCanonicalizedPath}` reports 5 passed, 0 failed.
7. **Task 7: 9 representative arithmetic + error tests** — `fb8dd1f` test(01-02): add 9 representative tests for CORE-02/03/04 + D-error rejections
   - AddTwoFiles + Chained (CORE-02 / CORE-04). ScalarBroadcastAddRight (CORE-03). SamePathTwice (CORE-04 + D-10). 5 error-rejection tests: MismatchedShapesThrows (D-01/D-06), UnitMismatchThrows (D-07), TimePropertiesMismatchThrows (D-04), InitialDatetimeMismatchThrows (D-09), LabelMismatchThrows (D-08). 14 ExpressionFixture cases total; full `--gtest_filter=ExpressionFixture.*` reports 14 passed, 0 failed.
8. **Task 8: full suite + grep audit** — verification-only, no commit.
   - `./build/bin/quiver_tests.exe` (no filter) reports 766 / 0 failed.
   - `grep -n "std::vector<double>" src/expr/expression.cpp` → 2 hits (line 76 a comment, line 78 the single `std::vector<double> row;` declaration outside the loop body).
   - `grep -n "std::vector<double>" src/expr/nodes.cpp` → 3 hits, all of them the `std::vector<double>&` parameter type in compute_row signatures (no allocations).
   - `grep -n "mutable std::vector" include/quiver/expr/node.h` → 5 hits: 4 on BinaryOpNode (`lhs_dims_buf_`, `rhs_dims_buf_`, `lhs_buf_`, `rhs_buf_`) + 1 on FileNode (`own_dims_buf_`). Matches the CORE-06 single-buffer-reuse design.

**Plan metadata:** Will be added by the orchestrator after this SUMMARY commits.

_Note: TDD tasks have multiple commits (test → feat). Task 1 + Task 4 + Task 6 form the FileNode/ScalarNode/BinaryOpNode TDD cycle (test scaffold f8f050f → declarations 5527bf5 fca0cf1 → impls 23eaa52 8c78e33 → wire f54d54e). Task 7 adds extension tests on top of green._

## Files Created/Modified

**Created:**
- `include/quiver/expr/expression.h` — Expression value type (Rule of Zero, no Pimpl) wrapping `std::shared_ptr<Node>`. Non-explicit `Expression(const BinaryFile&)` per CORE-01. Explicit `Expression(std::shared_ptr<Node>)` for internal use. Methods: `metadata()`, `save(path)`, `node()`. 12 free-function operator overloads at namespace scope.
- `include/quiver/expr/node.h` — `Node` virtual base with pure-virtual `metadata()` + `compute_row(dims, out)`. `FileNode` (mutable file_ for lazy open + own_dims_buf_), `ScalarNode` (value_ + broadcast_meta_), `BinaryOpNode` (op_, lhs_, rhs_, broadcast_meta_, translation tables, mutable buffers). `enum class BinaryOp { Add, Subtract, Multiply, Divide }`. `BinaryFile` forward-declared in `quiver` namespace.
- `src/expr/expression.cpp` — Anon-namespace `collect_file_paths` (recursive dynamic_cast walking FileNode/BinaryOpNode). Anon-namespace `make_binop / scalar_left / scalar_right` dispatch helpers. `Expression::save` body: D-11 self-save check → open writer → row loop with single `std::vector<int64_t>` dims + `std::vector<double>` row buffers (CORE-06).
- `src/expr/nodes.cpp` — Anon-namespace `find_dim_index / any_time_dim / apply_op` helpers. FileNode lazy open. ScalarNode broadcast. BinaryOpNode constructor: 6-step validation chain, broadcast_meta_ build per D-02 with parent_dimension_index recompute by name, defense-in-depth `validate()`, pre-compute translation tables and per-dim sizes. BinaryOpNode::compute_row: D-05 dim translation + size-1 clamp, recursive operand dispatch, D-08 label-axis broadcast in apply_op loop.
- `tests/test_expression.cpp` — `ExpressionFixture` extending `::testing::Test` with `path_a / path_b / path_c / path_out / path_out2`, programmatic `write_qvr(path, meta, fill)` (uses Plan 01 iterators + D-14 fast write), `read_all_cells(path)` helper. 14 TEST_F cases.

**Modified:**
- `src/CMakeLists.txt` — appended `expr/expression.cpp` and `expr/nodes.cpp` to `QUIVER_SOURCES` (after the binary group).
- `tests/CMakeLists.txt` — appended `test_expression.cpp` to `quiver_tests` source list (after `test_binary_time_properties.cpp`).

## Decisions Made

1. **Implicit conversion only for `Expression(const BinaryFile&)`** — every other public constructor in the codebase is `explicit`; this one is intentionally not, per CORE-01's `Expression e = a;` pattern. Documented inline.
2. **Forward-decl BinaryFile in node.h, not full include.** FileNode's ctor takes `const BinaryFile&` by reference; the body lives in nodes.cpp. Forward-declaration is sufficient and keeps the public Node header free of `binary_file.h`'s transitive includes.
3. **Three anon-namespace dispatch helpers in expression.cpp** (`make_binop`, `scalar_left`, `scalar_right`) collapse 12 operator overload bodies to one line each. Keeps operator semantics in one place.
4. **Pre-computed translation tables on BinaryOpNode.** Per-row search to map operand dim index → output dim index would otherwise be O(out_dims) per row per operand. Pre-computing at construction makes compute_row faster; the tables are small (size = number of output dims, typically 2-4).
5. **dynamic_cast for AST walking in `collect_file_paths`.** One virtual dispatch per node, dwarfed by the file I/O cost of save(). Acceptable for v1.
6. **Inline accessors in node.h** (FileNode::path / BinaryOpNode::lhs / rhs). Trivial getters; no .cpp impl needed; visible to expression.cpp's collect_file_paths via the public surface.
7. **Cleanup helper in ExpressionFixture.** Consolidated SetUp + TearDown leftover-removal into a private `cleanup()` method shared by both, ensuring test isolation across reruns.
8. **`mutable` member-buffer pattern is the first use in the codebase.** Documented inline in node.h next to each mutable member with the NOT-thread-safe note. Establishes the pattern for future const + buffer-reuse hot paths.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Plan example test data used unsupported "weekly" inner frequency**
- **Found during:** Task 7 (running ExpressionFixture.* after appending the 9 extension tests)
- **Issue:** The plan's `<action>` block for `TimePropertiesMismatchThrows` constructed `md_b` with `frequencies: ["monthly", "weekly"]`. Running the test produced `"WEEKLY frequency not implemented. This function should only be used for inner time dimensions."` — thrown from `compute_time_dimension_initial_values` in `src/binary/binary_metadata.cpp:43-45`. The error fired during `BinaryMetadata::from_element(md_b)` in the test setup itself, never reaching the `operator+` call that the test was intended to exercise. The plan's example data was a known-broken combination relative to the existing engine's frequency support matrix (only Daily and Hourly are valid as inner dims; Weekly and Yearly throw).
- **Fix:** Replaced the test data with a frequency-disjoint construction that exercises the SAME D-04 code path through a different branch: lhs treats `block` as a monthly time dim (`time_dimensions=["block"]`, `frequencies=["monthly"]`); rhs treats `block` as a regular non-time dim (no `time_dimensions`). Same dim name + same size on both sides → D-01 passes; lhs `block` is time, rhs `block` is not → BinaryOpNode's `"is a time dimension on lhs but not on rhs"` check (a D-04 branch) fires. This validates that D-04 rejects inconsistent treatment of a same-name dim across operands without depending on engine-side frequency matrix.
- **Files modified:** `tests/test_expression.cpp` (TimePropertiesMismatchThrows body only).
- **Verification:** `quiver_tests.exe --gtest_filter=ExpressionFixture.TimePropertiesMismatchThrows` reports OK (33 ms). Full `--gtest_filter=ExpressionFixture.*` reports 14/14 passed.
- **Committed in:** `fb8dd1f` (Task 7 commit).

---

**Total deviations:** 1 auto-fixed (1 bug fix in plan-supplied test data).
**Impact on plan:** Replacement test still validates D-04 — just through a different in-validation-chain branch. The original plan intent ("triggering a TimeProperties mismatch") is preserved; coverage of the frequency-difference path within D-04 is deferred. Plan 03 (per VALIDATION.md) can add a hand-constructed-metadata test exercising that branch directly when it lands the remaining 20+ enumerated cases.

## Issues Encountered

1. **Initial Write/Edit calls landed in the main repo, not the worktree.** The first iteration of Task 1 used Windows-style absolute paths starting with `C:\Development\DatabaseTmp\quiver\...` (without the `.claude\worktrees\agent-a6ac63406daaf318c\` prefix). The Write tool resolved those to the main repo paths, creating `tests/test_expression.cpp` and modifying `tests/CMakeLists.txt` outside the worktree branch. Detected immediately via `git status` in the worktree (clean) vs the main repo (modified).
   - **Resolution:** `cd C:\Development\DatabaseTmp\quiver && git checkout -- tests/CMakeLists.txt && rm tests/test_expression.cpp` to restore the main repo. Re-issued Write/Edit calls with the full worktree path. All subsequent file ops landed in the worktree correctly.
   - **No data lost:** the main repo was on a different branch (`rs/operations`); the stray test file was untracked, the CMakeLists.txt edit was reverted before any commit landed there. (Same root cause as Wave 1's Issue #1; resolution identical.)

2. **Stale ninja build files after `tests/CMakeLists.txt` edit.** First `cmake --build build --config Debug --target quiver_tests` after Task 1 reported `ninja: no work to do` because the build directory's ninja files were generated at initial-configure time (before the tests/CMakeLists.txt edit). Required a `cmake -B build` reconfigure. The second build correctly compiled `test_expression.cpp` and produced the expected RED `quiver/expr/expression.h: No such file or directory` linker error. Subsequent edits triggered correct re-detection automatically. (Same root cause as Wave 1's Issue #2.)

3. **Plan's `TimePropertiesMismatchThrows` test data was not engine-supported (Rule 1 bug).** See Deviations section. Did not block progress; fix took ~5 minutes including investigation in `compute_time_dimension_initial_values`.

## User Setup Required

None — no external service configuration required. The `quiver::expr` subsystem is pure C++ and depends only on existing in-repo deps (sqlite3, spdlog, sol2, lua, tomlplusplus, rapidcsv, GoogleTest — all of which are already wired via FetchContent).

## Next Phase Readiness

- The `quiver::expr` subsystem is ready for Plan 03 (test fill-out):
  - `tests/test_expression.cpp` is structured for additive extension. Plan 03 can append the remaining ~20 cases enumerated in `01-VALIDATION.md` (e.g., `BroadcastSizeOneDim`, `BroadcastLabelsAxis`, `UnionDimsAcrossOperands`, full scalar broadcast matrix `ScalarBroadcast{Add,Subtract,Multiply,Divide}{Left,Right}`, `LargeGridCompletes`, etc.) without rewriting the fixture.
  - Public API surface is locked: `quiver::expr::Expression`, `Node`, `FileNode`, `ScalarNode`, `BinaryOpNode`, `BinaryOp`, 12 operator overloads. Plan 03 only adds tests, no API changes.
- No blockers for Plan 03. The CORE-06 design contract is preserved by code-review and grep audit (Task 8); Plan 03's `LargeGridCompletes` smoke test will provide an additional runtime backstop for it.
- Phase 2 (C API) work was deferred at the project level (REQUIREMENTS.md). Plan 03 closes Phase 1.

## Threat Flags

None — this plan only added new in-process API surface (no new file paths, no new network endpoints, no new auth paths, no new schema). The threat model's T-1-201 / T-1-202 (self-save corruption) are mitigated by D-11 — verified by `SelfSaveCollisionThrows` (input file size + first 64 bytes unchanged after the throw) and `SelfSaveCollisionThrowsWithCanonicalizedPath` (non-canonical path normalizes to the same canonical form). T-1-203 (NaN/inf on division-by-zero) and T-1-204 (mutable buffer concurrency) are documented `accept` decisions per the plan threat register; no test required for either.

## Self-Check: PASSED

All declared files exist on disk and all per-task commits are present in `git log`:

- `include/quiver/expr/expression.h` ✓
- `include/quiver/expr/node.h` ✓
- `src/expr/expression.cpp` ✓
- `src/expr/nodes.cpp` ✓
- `tests/test_expression.cpp` ✓
- `src/CMakeLists.txt` ✓ (modified)
- `tests/CMakeLists.txt` ✓ (modified)

Commits found:
- `f8f050f` ✓ test(01-02): add failing tests for Expression scaffold
- `5527bf5` ✓ test(01-02): add Node, FileNode, ScalarNode, BinaryOpNode public header
- `fca0cf1` ✓ test(01-02): add Expression public header + 12 operator overload decls
- `23eaa52` ✓ feat(01-02): implement FileNode, ScalarNode, BinaryOpNode
- `8c78e33` ✓ feat(01-02): implement Expression + save engine + 12 operator overloads
- `f54d54e` ✓ feat(01-02): wire expr sources into CMake; baseline tests green
- `fb8dd1f` ✓ test(01-02): add 9 representative tests for CORE-02/03/04 + D-error rejections

---
*Phase: 01-c-core*
*Completed: 2026-05-06*
