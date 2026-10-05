---
milestone: lua-2
name: Abstract Expressions and Quiver File Layout
audited: 2026-10-04T23:30:00-03:00
head: f5b3dc3
status: tech_debt
scores:
  requirements: 25/25
  phases: 4/4
  integration: 5/5
  flows: 4/4
gaps:
  requirements: []
  integration: []
  flows: []
tech_debt:
  - phase: 06-quiver-file-layout
    items:
      - "Review WR-01: sol2's automatic __eq/__lt/__le on BinaryFile/Expression always return true (pre-existing; deferred decision EQ-01, listed in Out of Scope)"
      - "Review IN-01: the documented binder call order wording vs core-file order (info)"
  - phase: 07-abstractexpression-in-c
    items:
      - "Review WR-01: AbstractExpression::get_metadata()'s base body is valid only for a subclass whose node() returns a node it keeps alive; documented as a contract comment in abstract_expression.h, not enforced"
      - "include/quiver/expression/expression_node.h still exposes the internal ExpressionNode::metadata() (installed header); outside EXPR-05's scope (Expression's accessor), noted by the integration check"
  - phase: 08-typed-expression-parameters-in-lua
    items:
      - "Review WR-01: f == g / e == e2 / f < g evaluate to true in Lua (same EQ-01 decision)"
      - "Review WR-02: f:save on a file open for writing reports 'Cannot open_file: ...' (the core's text, kept on purpose and pinned by a test; Julia now pins the same text)"
      - "Review IN-01/IN-02: directly called __unm/__bnot report 'got nil'; the traits-include invariant is enforced only by a gitignored harness (info)"
  - phase: 09-julia-abstractexpression-and-docs
    items:
      - "src/AGENTS.md:1030 and :1040 still say Expression::save (now AbstractExpression::save); pre-existing lines not in any review scope"
      - "src/c/AGENTS.md describes the quiver_expression_from_file bridge but never names AbstractExpression (DOC-01 passed verification; strict reading flagged by the integration check)"
  - phase: milestone-wide
    items:
      - "No plan SUMMARY.md in phases 6-9 carries requirements-completed frontmatter; the 3-source cross-check fell back to VERIFICATION.md evidence + PLAN requirements + REQUIREMENTS.md checkboxes"
security:
  status: closed
  note: "/gsd-secure-phase run for phases 6-9 after the first audit pass: 39 threats, 39 closed (33 mitigated, 6 accepted), threats_open 0 in every phase (06/07/08/09-SECURITY.md)"
nyquist:
  skipped: "validate-phase capability inactive (workflow.nyquist_validation: false)"
---

# Milestone lua-2 Audit: Abstract Expressions and Quiver File Layout

**Verdict: tech_debt.** All 25 requirements are satisfied, all four phases passed verification, and the
integration check found no cross-phase gap or broken flow. What remains is a short list of advisory
review items and two workflow gaps; none blocks completing the milestone.

## Phases

| Phase | Verification | Score | Review (C/W/I) | Fixes |
|-------|--------------|-------|----------------|-------|
| 06 Quiver file layout | passed | 5/5 roadmap criteria + plan must-haves | 0/1/1 | — |
| 07 AbstractExpression in C++ | passed | 5/5 roadmap, 33/34 must-haves (1 backstop abstained); 4 human items resolved | 0/1/5 | — |
| 08 Typed expression parameters in Lua | passed | 5/5 roadmap, 39/39 truths, 8/8 requirements | 0/2/3 | — |
| 09 Julia AbstractExpression and docs | passed | 5/5 roadmap, 26/26 must-haves | 0/1/2 | 3/3 fixed (`1402f01`, `3b09ea2`, `3bc1bb5`) |

## Requirements (3-source cross-reference)

| Requirement | Phase | VERIFICATION.md | PLAN `requirements:` | SUMMARY `requirements-completed` | REQUIREMENTS.md | Final |
|-------------|-------|-----------------|----------------------|----------------------------------|-----------------|-------|
| LAYOUT-01..04 | 6 | SATISFIED | 06-01, 06-02, 06-03 | absent | [x] | satisfied |
| LAYOUT-05 | 6 | SATISFIED | 06-03 | absent | [x] | satisfied |
| EXPR-01..05 | 7 | SATISFIED | 07-01 | absent | [x] | satisfied |
| EXPR-06 | 7 | SATISFIED | 07-01, 07-02 | absent | [x] | satisfied |
| EXPR-07 | 7 | SATISFIED | 07-02 | absent | [x] | satisfied |
| LUA-01, 03, 04, 05, 08 | 8 | SATISFIED | 08-01 | absent | [x] | satisfied |
| LUA-02, 07 | 8 | SATISFIED | 08-01, 08-02 | absent | [x] | satisfied |
| LUA-06 | 8 | SATISFIED | 08-02 | absent | [x] | satisfied |
| JUL-01..03 | 9 | SATISFIED | 09-01 | absent | [x] | satisfied |
| DOC-01, DOC-02 | 9 | SATISFIED | 09-01, 09-02 | absent | [x] | satisfied |

No requirement is orphaned. By the matrix, "passed + SUMMARY field missing" reads *partial, verify
manually*. The manual check was done: each VERIFICATION.md has a per-requirement table with evidence,
every PLAN lists the IDs it covers, and REQUIREMENTS.md marks all 25 complete. The missing field is a
SUMMARY-template gap across the milestone, not a coverage gap, so all 25 are counted satisfied.

## Integration

| Seam | Requirements | Result |
|------|--------------|--------|
| Phase 6 layout ↔ Phase 8 typed parameters: traits in `src/lua_runner/internal.h:38-40`, included by every TU that binds `BinaryFile`/`Expression` (no ODR risk); binder order `lua_runner.cpp:113-126`; no stale `db_*`/`bind_core` names | LAYOUT-01..05, LUA-01, LUA-06 | wired |
| C++ `AbstractExpression` → Lua sol2 base → C API `quiver_expression_from_file` (`src/c/expression/expression.cpp:99-112`, explicit conversion through `BinaryFile::node()`, handle not retained) → Julia `_expression` | EXPR-01..06, LUA-04, JUL-02 | wired |
| Cross-binding parity: a raw file takes the same operation set in Lua and Julia; `get_metadata` everywhere | LUA-04, LUA-05, JUL-03 | wired |
| Docs ↔ code: CHANGELOG `[0.13.0]` BREAKING lines (63-74) + Julia entry (88), version 0.13.0, `LUA_DB_API_REFERENCE` + sync test | DOC-01, DOC-02, LUA-08 | wired (DOC-01 warning below) |
| Build: `src/CMakeLists.txt` source lists match `src/lua_runner/` and `src/expression/` on disk exactly | LAYOUT-04 | wired |

EXPR-07 (Linux GCC/Clang) and LUA-07 (no Release warnings) are toolchain properties, so the static
integration check doesn't cover them; phases 7 and 8 verified them by running the toolchains.

## End-to-end flows

| Flow | Result |
|------|--------|
| Lua: a raw file → operators → `aggregate` → `save` / `get_metadata` | complete |
| C API: `from_file` → close the file → `save` the expression | complete |
| Julia: raw `Binary.File` → operators → `Quiver.save` / `Quiver.get_metadata` | complete |
| Docs → `LUA_DB_API_REFERENCE` → sync test | complete |

All six suites pass on HEAD (C++ 1476, C API 543, Julia 1675 (expression 290 after the review fixes),
Dart 444, JS 241, Python 350), plus the lua-api sync test.

## Tech debt

**Phase 6:** the review's WR-01 (`==`/`<`/`<=` on files/expressions always true) is the deferred
EQ-01 decision, and IN-01 is binder-order wording.

**Phase 7:** WR-01 says `AbstractExpression::get_metadata()`'s base body is safe only for subclasses
that keep their node alive. That is documented as a contract comment and not enforced. The installed
header `expression_node.h` still has the internal `ExpressionNode::metadata()`, which is outside EXPR-05.

**Phase 8:** WR-01 is the same EQ-01 issue. WR-02 is that `f:save` on a writer reports `Cannot open_file`,
which was kept on purpose and is now pinned in Lua and Julia. IN-01/IN-02 are minor.

**Phase 9:** `src/AGENTS.md:1030,1040` still say `Expression::save`. `src/c/AGENTS.md` describes the
bridge but doesn't name `AbstractExpression`.

**Milestone-wide:** none of the summaries has `requirements-completed` frontmatter. (The security
gap from the first pass is closed: `/gsd-secure-phase` ran for phases 6-9, 39 threats, 0 open.)

Total: 9 items across 4 phases plus 1 milestone-wide. Two are open decisions (EQ-01, which appears in
both phases 6 and 8, and WR-02); the rest are doc wording, info notes and workflow gaps.
