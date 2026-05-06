# Phase 2: Tech Debt - Context

**Gathered:** 2026-05-06
**Status:** Ready for planning

<domain>
## Phase Boundary

Phase 2 closes the 12 review findings carried forward from Phase 1 (3 BLOCKER + 9 WARNING) tracked in `.planning/v1.0-MILESTONE-AUDIT.md`. Operates on `src/expr/*.cpp`, `src/binary/*.cpp`, and the matching headers under `include/quiver/`. No new requirements, no new public API — pure cleanup against an audit-defined punch list.

In scope:
- **CR-01** D-04 mirror-case correctness gap (closed via symmetric reject + name-based parent comparison; bundled with WR-01)
- **CR-02** `BinaryFile::open_file('w', ...)` write-registry leak on partial-construction failure
- **CR-03** BinaryOpNode `runtime_error` messages conformance to CLAUDE.md three-pattern rule
- **WR-01** parent-dimension comparison by raw operand index (bundled into CR-01 fix)
- **WR-02** dead `validate_file_is_open()` removal
- **WR-03** `dimension_sizes_at_values` duplication between `binary_file.cpp` and `iteration.cpp`
- **WR-04** unused `BinaryFile::dimension_sizes_at_values` and protected `next_dimensions`
- **WR-05** `validate_dimension_values` / `_indexed` calendar-aware-logic dedup
- **WR-06** `BinaryOpNode::compute_row` O(n²) inner search → pre-computed reverse translation table
- **WR-07** `apply_op` exhaustive-switch fall-through risk
- **WR-08** missing test for implicit `BinaryFile → Expression` conversion
- **WR-09** `next_dimensions` rebuilds `first_dimensions(meta)` per call

Out of phase: any new feature work, any C API or binding work (deferred to future milestone per ROADMAP.md scope narrowing on 2026-05-06), reductions / comparison ops / `where()` (v2).

</domain>

<decisions>
## Implementation Decisions

### CR-01 + WR-01 (bundled)
- **D-15 — Mirror-case model:** when two operands share a dim by name but disagree on time-ness (one tags it as time, the other does not), throw symmetrically. Same-name dims must agree: both time or both non-time. Mirrors the existing lhs-time / rhs-non-time rejection (`nodes.cpp:140-143`) and aligns with D-04's "TimeProperties must match exactly" philosophy.
- **D-16 — Compatibility loop restructure:** rewrite the time-dim compatibility loop in `BinaryOpNode` constructor to iterate the **union of same-name dims** across both operands (closing the mirror gap). For each same-name pair, validate both-time-or-both-non-time, and when both are time, validate `frequency`, `initial_value`, and **parent dimension by NAME** (not by raw operand index — closes WR-01).
- **D-17 — WR-01 relaxation rule:** resolve each operand's `parent_dimension_index` to the parent dim's NAME on its own side, then compare names across operands. Two same-name time dims are compatible iff their parent-dim names match (or both have `parent_dimension_index == -1`).

### CR-03 (and CR-01 reject wording)
- **D-18 — BinaryOpNode error verb:** use `"Cannot apply: {reason}"` for all throws in `BinaryOpNode` constructor. Replaces the off-pattern `"Cannot apply binary operation: ..."`. Single short verb consistent with `"Cannot save:"` (D-11) and `"Cannot read:"` (existing). Operator-agnostic so the same wording works across `+ - * /`. The new CR-01 mirror-reject message uses this verb too.

### CR-02
- **D-19 — Write-registry leak fix shape:** **late insert**. Move `write_registry.insert(canonical)` AND `binary_file.impl_->registered_path = canonical` to the bottom of the `case 'w':` branch in `BinaryFile::open_file` (`binary_file.cpp:81-100`), AFTER `fill_file_with_nulls()` succeeds. The function-prologue precondition check (`if (write_registry.count(canonical))` at line 59) stays in place — it still rejects concurrent writers; only the population is delayed. Piggybacks on existing `Impl::~Impl()` cleanup (`registered_path`-driven erase). No new types, no new control flow, no try/catch. Aligns with CLAUDE.md "Clean code over defensive code."

### Cleanup aggression (WR-02..05, WR-09)
- **D-20 — Dead-code verification before removal:** for WR-02, WR-04, and any code WR-09's restructure leaves stranded, **grep across `src/`, `tests/`, `include/`, `bindings/` before deletion**. If grep shows zero callers, delete. If callers exist (e.g., test-only friend access to a protected member), surface to the executor and the user before removing. Trust the audit, but verify per item — catches anything the audit missed.
- **D-21 — WR-05 dedup direction:** `validate_dimension_values(unordered_map<string, int64_t> dims)` builds a `vector<int64_t>` in dimension declaration order from the map, then calls `validate_dimension_values_indexed(vec)`. Single source of truth (the indexed version). The map version remains the public slow path; the conversion overhead is irrelevant on the slow path and removes ~50 lines of duplication.
- **D-22 — WR-09 fix shape:** `next_dimensions` adds a `bool incremented` flag inside the existing increment loop, set to `true` only on the `break` path. Replace the post-loop `if (next == first_dimensions(meta))` comparison with `if (!incremented) return std::nullopt;`. No interface change, zero new allocations, no caller updates. The time-dim adjustment loop and the rest of the function stay.

### Tests
- **D-23 — CR-01 + WR-01 test coverage:** add four new GoogleTest cases in `tests/test_expression.cpp`:
  1. **Mirror A:** `lhs` non-time + `rhs` time on same dim name → throws.
  2. **Mirror B:** `lhs` time + `rhs` non-time on same dim name → throws (regression backstop for current implicit coverage).
  3. **WR-01 positive:** both sides time, same dim name, parent-dim has the same NAME on both sides at different operand indices → accepts.
  4. **WR-01 negative:** both sides time, same dim name, parent-dim has different NAMES on each side → throws.
- **D-24 — CR-02 regression test:** add a GoogleTest case that points the metadata-write path at a non-existent parent directory (or read-only dir), expects `BinaryFile::open_file('w', ...)` to throw, then calls `BinaryFile::open_file('w', ...)` on the same canonical path again — expects success (i.e., the registry was NOT leaked). Drop into `tests/test_binary_file.cpp` or a new dedicated case.
- **D-25 — WR-08 implicit-conversion test:** add a GoogleTest case demonstrating `Expression e = somefile + otherfile;` where `somefile`/`otherfile` are `BinaryFile` instances and the implicit conversion fires inside the operator argument (not on a stand-alone `Expression e = somefile;` line). Closes the WR-08 coverage gap.

### Plan structure
- **D-26 — Single plan, audit-order commits:** one `02-01-PLAN.md` covering all 12 audit items. ~11 atomic commits in audit doc order: `CR-01` (bundles WR-01) → `CR-02` → `CR-03` → `WR-02` → `WR-03` → `WR-04` → `WR-05` → `WR-06` → `WR-07` → `WR-08` → `WR-09`. CR-01 + WR-01 land as a single commit per D-16. Reviewer can cross-reference each commit message to its audit ID directly. Matches Phase 1's per-decision atomic-commit cadence.

### Claude's Discretion
- **WR-03 dedup mechanics.** WR-04's deletion of `BinaryFile::dimension_sizes_at_values` (if grep confirms zero callers) automatically resolves WR-03 — only the `iteration.cpp` free function remains. If WR-04 surfaces a caller, decide between keeping the member as a thin delegate to the free function (matches D-12 pattern) vs. extracting both into a shared private helper.
- **WR-06 reverse translation table shape.** The forward translation tables `lhs_dim_translate_` / `rhs_dim_translate_` already exist (output→operand index). Pre-compute the inverse (operand→output index) at `BinaryOpNode` construction; call them `lhs_to_out_` / `rhs_to_out_` (or similar). Replace the inner-loop O(n²) search at `nodes.cpp:283-310` with O(1) lookup. Mechanical extension of D-05.
- **WR-07 `apply_op` exhaustive-switch fix.** Replace `return 0.0;` with a throw that reads as a runtime canary if a new `BinaryOp` variant is added without updating `apply_op` — prefer the project's three-pattern error format. Exact wording is Claude's call (e.g., `"Cannot apply: unhandled BinaryOp variant"` keeps consistency with D-18).
- **Order of work within the audit-order plan.** D-26 fixes the commit sequence, but task subdivision within each item (e.g., grep-then-delete pairs in WR-02/04/09) is the planner's call.
- **Whether to perform any incidental cleanup adjacent to listed items.** Default: stay in scope. If a clearly trivial dead-code item is found while editing one of the 12, surface it as a deferred idea, do not silently expand scope.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Audit and prior phase context
- `.planning/v1.0-MILESTONE-AUDIT.md` — the 12 audit items (CR-01..03 + WR-01..09) with severity, file:line, impact, and disposition. **Source of truth for this phase's scope.**
- `.planning/v1.0-MILESTONE-INTEGRATION.md` — Phase 1 cross-plan wiring map; shows the call graph that constrains how aggressively dead code can be removed.
- `.planning/phases/01-c-core/01-CONTEXT.md` — Phase 1 decisions D-01..D-14 (broadcast model, error wording, BinaryFile extensions). D-04 / D-05 / D-12 / D-13 are particularly load-bearing here.
- `.planning/phases/01-c-core/01-VERIFICATION.md` — verifies which Phase 1 truths and tests must remain green (full `quiver_tests` suite is 781/781).
- `.planning/phases/01-c-core/01-REVIEW.md` — original reviewer write-ups for CR-01..03 and WR-01..09 (richer context than the audit summary).

### Project planning
- `.planning/PROJECT.md` — locked decisions for the lazy-expressions subsystem; ABI / pimpl / no-new-deps constraints still apply.
- `.planning/REQUIREMENTS.md` — v1 requirements (CORE-01..06 + TEST-01); Phase 2 adds NO new REQ entries.
- `.planning/ROADMAP.md` §"Phase 2: Phase 1 Tech Debt" — phase goal, success criteria, 12 audit items mapped to closure conditions.

### Project conventions
- `CLAUDE.md` — three error-message patterns (`Cannot {operation}: {reason}` / `{Entity} not found: {identifier}` / `Failed to {operation}: {reason}`); "Clean code over defensive code"; "Delete unused code, do not deprecate"; ABI / pimpl rules.
- `CLAUDE.md` §"Binary Performance Bottlenecks" — concrete numbers motivating WR-09 (`next_dimensions`) and the broader fast-path posture.

### Touched code
- `src/expr/nodes.cpp` — CR-01, CR-03, WR-01, WR-06, WR-07. Compatibility loop at lines 132-151; `apply_op` at 82-94; `compute_row` reverse-search at 283-310.
- `include/quiver/expr/expression.h` — WR-08 (implicit-conversion test target at line 23).
- `src/binary/binary_file.cpp` — CR-02, WR-02, WR-04, WR-05. `open_file('w', ...)` at 81-100; write registry at line 19; `validate_dimension_values` family at 269-372.
- `include/quiver/binary/binary_file.h` — WR-02 (`validate_file_is_open()` at line 67); WR-04 (`dimension_sizes_at_values` + protected `next_dimensions` at 76-77).
- `src/binary/iteration.cpp` — WR-03, WR-09. Free-function iterator helpers; `next_dimensions` at 104-140.
- `tests/test_expression.cpp` — D-23, D-25 test additions.
- `tests/test_binary_file.cpp` — D-24 CR-02 regression test target.

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- **Forward translation tables** (`lhs_dim_translate_`, `rhs_dim_translate_` in `BinaryOpNode`, established by D-05) — WR-06's reverse table is structurally identical (operand→output instead of output→operand). Pre-computed once at construction, used in `compute_row`'s inner loop.
- **`Impl::~Impl()` registry-erase mechanism** (`binary_file.cpp:37-40`) — already handles `write_registry.erase(registered_path)` when `registered_path` is non-empty. CR-02 late-insert fix piggybacks on this; no new cleanup logic needed.
- **D-12 free-function iterators** (`quiver::binary::first_dimensions`, `next_dimensions`) — WR-09 fix preserves the same signature; only the internal end-of-iteration check changes.
- **D-13 / D-14 fast read/write overloads** — already in place from Phase 1; the cleanup phase doesn't add any new overloads, only fixes existing implementations.

### Established Patterns
- **Three-pattern error messages** (CLAUDE.md) — CR-03 + the new D-18 verb apply this pattern across all `BinaryOpNode` throws. Existing Phase 1 messages already follow it (D-06 / D-07 / D-09 / D-11); CR-03 brings the legacy `"Cannot apply binary operation: ..."` strings into line.
- **"BinaryFile member as thin delegate to free function"** (D-12) — if WR-04's grep surfaces callers, the dedup direction defaults to making the BinaryFile member a thin delegate to the free function. Matches the established pattern.
- **GoogleTest fixtures with programmatic `.qvr` setup** — Phase 1 convention; CR-01 / CR-02 / WR-08 new tests follow the same fixture style as existing `ExpressionFixture` and `BinaryTempFileFixture` cases.
- **Atomic per-decision commits** — Phase 1 cadence; D-26 keeps it for Phase 2 (one commit per audit item, ~11 commits total).

### Integration Points
- **No new public-header surface.** All edits stay inside existing files. WR-04 may shrink the `BinaryFile` public/protected surface (deletions); the audit-driven removals are the only header changes.
- **No CMake changes.** No new sources, no new dependencies, no new test executables.
- **`csv_converter.cpp:85, 130`** — calls `BinaryFile::next_dimensions` (delegated to free function per D-12). WR-09 fix must preserve the existing 2-arg interface to keep csv_converter compiling without edits.
- **`Expression::save` row loop** (`expression.cpp:77-86`) — calls `quiver::binary::first_dimensions(meta)` once for init and `next_dimensions(meta, dims)` per row. WR-09 fix is internal to `next_dimensions` and does not change this call shape.

</code_context>

<specifics>
## Specific Ideas

The user's choices in discussion captured verbatim:

> **CR-01 model:** "Reject symmetrically" — same-name dims must agree on time-ness.

> **CR-01 / WR-01 bundling:** "Bundle into one fix" — single coherent change to the `BinaryOpNode` compatibility loop.

> **CR-03 verb:** "apply" — `"Cannot apply: {reason}"` everywhere in `BinaryOpNode`.

> **CR-02 fix:** "Late insert" — move `write_registry.insert` to after `fill_file_with_nulls()` succeeds; piggyback on existing `Impl::~Impl()` cleanup.

> **CR-02 test:** "Force toml write failure" — non-existent parent directory or read-only dir.

> **Cleanup mode:** "Verify-then-delete each" — grep before removal.

> **WR-05 dedup direction:** "Map version converts to indexed, then calls indexed."

> **WR-09 fix:** "Track 'did we break' flag" — `bool incremented` inside the increment loop.

> **Plan structure:** "Single plan covering all 12."

> **Commit cadence:** "Audit order, 1 commit per item."

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope. Adjacent dead-code or duplication items found during execution should be surfaced as deferred ideas (per Claude's-Discretion note above), not silently folded into Phase 2.

</deferred>

---

*Phase: 2-Tech Debt*
*Context gathered: 2026-05-06*
