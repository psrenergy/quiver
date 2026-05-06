# Phase 2: Tech Debt - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-05-06
**Phase:** 02-tech-debt
**Areas discussed:** CR-01 mirror-case behavior, CR-02 registry leak fix shape, Cleanup aggression (WR-02..05, WR-09), Plan structure for the 12 items

---

## CR-01 mirror-case behavior

### Q1 — When two operands share a dim by name but disagree on time-ness, what should happen?

| Option | Description | Selected |
|--------|-------------|----------|
| Reject symmetrically | Same-name dims must agree: both time or both non-time, otherwise throw. Simplest model, mirrors current lhs-time/rhs-non-time rejection. Aligns with D-04's "TimeProperties must match exactly" philosophy. | ✓ |
| Accept if parent-name aligns | Allow time/non-time mix only when the time side's parent-dimension (resolved by name, not raw index) aligns with the structure on the other side. Also closes WR-01 (over-restrictive parent_dimension_index check). | |
| Promote to time (time wins) | Accept and let the time-tagged operand's TimeProperties propagate to the output. Looser; requires propagating TimeProperties through the dim-union pass. May surprise users. | |

**User's choice:** Reject symmetrically
**Notes:** Aligns with D-04's "TimeProperties must match exactly" philosophy. Simplest model, no new propagation logic.

### Q2 — WR-01 lives in the same compatibility loop as CR-01. Bundle the fixes?

| Option | Description | Selected |
|--------|-------------|----------|
| Bundle into one fix | Restructure the compatibility loop once: iterate the union of same-name dims (closes mirror gap), and replace raw-index parent comparison with name-based comparison (fixes WR-01). One coherent change to the same code region. | ✓ |
| Separate commits, same plan | Two atomic commits in the same plan: first close the mirror gap (CR-01), second relax the parent comparison (WR-01). Easier to bisect if a regression appears. | |
| Different plans | CR-01 in one plan, WR-01 in another. More overhead, harder to keep coherent since both touch the same loop. | |

**User's choice:** Bundle into one fix
**Notes:** Both touch the same loop in `BinaryOpNode` constructor. One commit, one diff to review.

### Q3 — What verb should replace 'apply binary operation' in BinaryOpNode error messages?

| Option | Description | Selected |
|--------|-------------|----------|
| apply | 'Cannot apply: dimension X has incompatible sizes 3 vs 5 ...'. Single short verb, consistent with 'Cannot save:' (D-11) and 'Cannot read:'. Operator-agnostic. | ✓ |
| compose | 'Cannot compose: dimension X has incompatible sizes ...'. Reads as 'compose an expression', matches the AST-construction conceptual model. | |
| build_expression | 'Cannot build_expression: dimension X has incompatible sizes ...'. Most explicit method-name-like form, but a bit verbose at every throw site. | |

**User's choice:** apply
**Notes:** Affects all CR-03 rewrites and the new CR-01 reject message.

### Q4 — Which test cases must the CR-01 + WR-01 fix introduce?

| Option | Description | Selected |
|--------|-------------|----------|
| Mirror A: lhs non-time, rhs time → throws | The exact configuration the audit calls out. Asymmetric to existing 'lhs time, rhs non-time' rejection. Required. | ✓ |
| Mirror B: lhs time, rhs non-time → throws | Audit hints this is already implicitly covered; making it explicit as a regression backstop is cheap. | ✓ |
| WR-01 positive: same parent name, different operand index → accepts | Verifies WR-01 relaxation. Same parent name 'scenario', different raw index. After fix: accepts; before: throws. | ✓ |
| WR-01 negative: different parent names → throws | Verifies WR-01 still catches genuine mismatches. lhs time parent='scenario', rhs time parent='stage' — still incompatible. | ✓ |

**User's choice:** All four
**Notes:** Multi-select; full coverage of the mirror-gap + relaxation rule.

---

## CR-02 registry leak fix shape

### Q1 — How should the write_registry insert be made exception-safe?

| Option | Description | Selected |
|--------|-------------|----------|
| Late insert + early registered_path | Move write_registry.insert(canonical) and binary_file.impl_->registered_path = canonical to AFTER fill_file_with_nulls() succeeds. ~5-line diff. Registry is only populated for fully-constructed writers; destructor cleans up naturally. Simplest fix. | ✓ |
| RAII guard with commit() | Add a stack-local WriteRegistryGuard that registers on construct, erases on destruct unless commit() is called. Idiomatic C++, reusable if more 'register then maybe-commit' sites appear. Slightly more code but encapsulates the pattern. | |
| try/catch with cleanup | Wrap the post-insert construction in try/catch; on any throw, write_registry.erase(canonical) and rethrow. Most explicit but most boilerplate. Reads as defensive code. | |

**User's choice:** Late insert (after explicit clarification — Claude recommended this option with reasoning: piggybacks on existing `Impl::~Impl()` cleanup, smallest diff, aligns with CLAUDE.md "Clean code over defensive code").
**Notes:** User initially asked for clarification. Claude explained the three options and recommended late-insert based on (a) reuse of existing destructor cleanup, (b) lowest-risk diff, (c) project philosophy alignment.

### Q2 — Should the CR-02 fix come with a regression test?

| Option | Description | Selected |
|--------|-------------|----------|
| Yes — force toml write failure | Make the .toml output path point to a read-only directory (or a non-existent parent), call open_file('w', ...) expecting a throw, then call open_file('w', ...) on the same canonical path again — it should NOT report 'already open for writing'. Locks down the fix with a real test. | ✓ |
| Yes — expose registry inspector | Add a test-only static helper (e.g., quiver::binary::test::is_registered(path)) and assert the registry is empty after the throw. More invasive (test hook in production header) but doesn't require filesystem trickery. | |
| No — follow audit's advisory disposition | Skip the test as the audit recommended. Code review of the diff is sufficient. Saves test infrastructure cost. | |

**User's choice:** Yes — force toml write failure
**Notes:** Real regression test, no production-header test hook needed.

---

## Cleanup aggression (WR-02..05, WR-09)

### Q1 — How should the dead-code / duplication items (WR-02, WR-04, WR-09) be approached?

| Option | Description | Selected |
|--------|-------------|----------|
| Verify-then-delete each | Before each removal, grep across src/, tests/, include/, bindings/ for callers. If grep is empty, delete; if not, surface to user. Slower but catches anything the audit missed (e.g., test-only friend access to protected next_dimensions). | ✓ |
| Trust the audit, delete directly | Audit was done in good faith with full code access; just delete. Compiler will catch any missed reference. Fastest, leans into 'Clean code over defensive code'. | |
| Trust audit, but expand scope | Delete the 5 audit-listed items, AND while in those files, look for adjacent dead/duplicate code (other unused private helpers, similar copy-paste blocks). Risk: scope creep on a bounded cleanup phase. | |

**User's choice:** Verify-then-delete each
**Notes:** Catches any test-only friend access or binding-internal use the audit missed. Surfaces issues to user before deletion.

### Q2 — How should validate_dimension_values and _indexed be deduplicated?

| Option | Description | Selected |
|--------|-------------|----------|
| Map version converts to indexed, then calls indexed | validate_dimension_values(map) builds a vector<int64_t> in dimension declaration order from the map, then calls validate_dimension_values_indexed(vec). Single source of truth (the indexed version). Map version pays a small conversion cost — already on the slow path so cost is irrelevant. | ✓ |
| Extract a private static helper | Add a static helper inside binary_file.cpp's anonymous namespace that holds the calendar-aware logic. Both wrappers do their param-specific bookkeeping then call it. Most explicit about the shared core. | |
| Keep separate, pull only the calendar logic into a helper | Leave the param-specific bounds-check loops in each function but factor only the calendar/time-dim consistency block into a shared helper. Smallest behavioral diff, but ~half the duplication remains. | |

**User's choice:** Map version converts to indexed, then calls indexed
**Notes:** Single source of truth, no new helper functions.

### Q3 — How should next_dimensions detect end-of-iteration without rebuilding first_dimensions?

| Option | Description | Selected |
|--------|-------------|----------|
| Track 'did we break' flag | Add bool incremented = false inside the increment loop; set true on the break path. After the loop, if !incremented, return std::nullopt without ever calling first_dimensions(meta). Smallest diff, no allocation, runs the time-dim adjustment loop only when needed. | ✓ |
| Cache first_dimensions per meta | Store first_dimensions in a static thread_local map keyed by &meta. Avoids rebuild within a single iteration sequence. Costs a hash lookup per call and adds lifetime concerns (meta could be a temporary). Heavier. | |
| Pass first_dims as optional 3rd arg | Caller (Expression::save) already calls first_dimensions(meta) once for the loop init; pass it as a const& to next_dimensions. Zero allocation. Breaks the current 2-arg interface — touches Expression::save and any other caller (csv_converter.cpp delegates). | |

**User's choice:** Track 'did we break' flag (after explicit clarification — Claude recommended this option with reasoning: smallest diff, no interface change, the information is already implicit in the loop, other options trade real cost for non-existent problems).
**Notes:** User initially asked for clarification. Claude recommended the flag-based fix based on (a) zero interface impact (csv_converter, Expression::save unchanged), (b) the loop already implicitly knows it didn't break, (c) other options' tradeoffs aren't worth their cost in this codebase.

---

## Plan structure for the 12 items

### Q1 — How should the 12 audit items be packaged into plans?

| Option | Description | Selected |
|--------|-------------|----------|
| Single plan covering all 12 | One PLAN.md, atomic commits per audit item. ~12 commits. Reviewer sees the whole cleanup in one PR. Best for a small-scoped phase like this; matches Phase 1's per-decision atomic-commit cadence. | ✓ |
| Two plans — binary subsystem vs expr subsystem | Plan 02-01: src/binary/* items. Plan 02-02: src/expr/* items. Cleaner ownership boundary; binary plan can complete and ship independently. | |
| Two plans — blockers vs warnings | Plan 02-01: 3 BLOCKERs. Plan 02-02: remaining 8 WARNINGs. Risk-tiered — land critical fixes first. | |
| Three plans — by concern type | Plan 02-01: correctness. Plan 02-02: messages + tests. Plan 02-03: dedup + perf. Finest granularity but most overhead. | |

**User's choice:** Single plan covering all 12
**Notes:** Phase scope is bounded; one plan keeps coherence.

### Q2 — What commit ordering / cadence should the single plan follow?

| Option | Description | Selected |
|--------|-------------|----------|
| Audit order, 1 commit per item | CR-01→CR-02→CR-03→WR-01→...→WR-09. Mirrors the audit doc exactly — reviewer cross-references trivially. CR-01 + WR-01 still bundle but stay in their audit position. ~11 commits. | ✓ |
| Dependency-aware order, 1 commit per item | CR-03 first (establishes 'apply' verb), then CR-01+WR-01 bundle (uses the new verb), then expr/nodes.cpp WRs (06/07/08), then binary_file.cpp items, then iteration.cpp. Groups by file; respects dependencies. ~11 commits. | |
| Coarser — 1 commit per file/concern | ~5 commits: nodes.cpp correctness, nodes.cpp perf, expression.h test, binary_file.cpp, iteration.cpp. Less bisect granularity, fewer review hops. | |

**User's choice:** Audit order, 1 commit per item
**Notes:** CR-01 + WR-01 still land as one commit per Q2 above (in CR-01's audit slot). Reviewer-friendly, easy bisect.

---

## Claude's Discretion

- **WR-03 dedup mechanics** — likely subsumed by WR-04 deletion if grep confirms no callers; otherwise default to D-12's "BinaryFile member as thin delegate to free function" pattern.
- **WR-06 reverse translation table shape** — pre-compute operand→output index lookup at `BinaryOpNode` construction; mirrors D-05's existing forward tables.
- **WR-07 `apply_op` exhaustive-switch fix** — replace `return 0.0` with a throw using the D-18 `"Cannot apply: ..."` verb (e.g., `"Cannot apply: unhandled BinaryOp variant"`).
- **Order of work within audit-order plan** — task subdivision within each commit (e.g., grep-then-delete pairs in WR-02/04) is the planner's call.
- **Incidental-cleanup discipline** — stay in scope; defer adjacent items rather than silently expanding.

## Deferred Ideas

None — discussion stayed within phase scope. Adjacent dead/duplicate code found during execution should be surfaced as deferred ideas, not silently folded.
