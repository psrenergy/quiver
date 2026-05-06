---
phase: 02
slug: tech-debt
status: verified
threats_open: 0
asvs_level: 1
created: 2026-05-06
---

# Phase 02 — Security

> Per-phase security contract: threat register, accepted risks, and audit trail.
>
> Phase 02 is a refactor (closing 12 audit items: CR-01..CR-03 + WR-01..WR-09). No new attack surface. Net security delta is positive across ASVS V5 (Input Validation), V8 (Data Protection), V12 (Files/Resources). Phase 1 baseline threats T-1-101..T-1-301 are unchanged and out of scope for this audit.

---

## Trust Boundaries

| Boundary | Description | Data Crossing |
|----------|-------------|---------------|
| user code → Quiver C++ API | Caller-supplied paths, metadata, and dimension values cross into the library. Existing input-validation perimeter (`BinaryMetadata::validate`, `validate_dimension_values`, file-mode whitelist) is unchanged. | filesystem paths, TOML metadata, `int64_t` dim values |
| In-process `write_registry` | Static `std::unordered_set<std::string>` in `src/binary/binary_file.cpp` tracks canonical paths of files open for writing. Single-process, single-threaded contract per CLAUDE.md "Binary Performance Bottlenecks" / Pitfall 2. | canonical filesystem paths |

---

## Threat Register

| Threat ID | Category | Component | Disposition | Mitigation | Status |
|-----------|----------|-----------|-------------|------------|--------|
| T-2-101 | DoS / Tampering (V12 + V8) | `BinaryFile::open_file('w', ...)` write-registry lifecycle | mitigate (improved) | CR-02 D-19 deferred-insert: `write_registry.insert(canonical)` + `impl_->registered_path = canonical` moved AFTER `fill_file_with_nulls()` succeeds. Throw paths leave registry empty so subsequent legitimate writers succeed. Verified at `src/binary/binary_file.cpp:87-89` (NOTE), `src/binary/binary_file.cpp:104-105` (insert site), `src/binary/binary_file.cpp:37-44` (destructor non-empty guard). Regression test `OpenFileWriteFailureDoesNotLeakRegistry` at `tests/test_binary_file.cpp:473-493`. Commit 6905ed7. | closed |
| T-2-102 | Tampering / Repudiation (V5) | `apply_op` exhaustive switch on `BinaryOp` variant | mitigate (improved) | WR-07: switch fall-through `return 0.0;` replaced with `throw std::runtime_error("Cannot apply: unhandled BinaryOp variant");`. Future variants or static_cast invariant violations now surface as runtime errors instead of silent zero outputs. Verified at `src/expr/nodes.cpp:82-98` (switch covers Add/Subtract/Multiply/Divide; throw at line 97). Commit d63a604. | closed |
| T-2-103 | Tampering (V5) | `BinaryOpNode` time-dim compatibility check | mitigate (improved) | CR-01 + WR-01 (D-15..D-17): symmetric reject for same-name time-vs-non-time mismatch on either side; parent comparison resolved BY NAME on each side's own metadata, then compared across operands. Closes the asymmetry where lhs non-time + rhs time on same dim previously passed validation. Verified at `src/expr/nodes.cpp:136-169` (symmetric check + `parent_name_of` lambda lines 138-140; cross-name compare line 165). Regression tests `MirrorTimeNonTimeMismatchAThrows`, `MirrorTimeNonTimeMismatchBThrows`, `ParentDimMatchByNameAcceptsCrossPosition`, `ParentDimNameMismatchThrows` at `tests/test_expression.cpp:448-558`. Commit 53eed8e. | closed |

*Status: open · closed*
*Disposition: mitigate (implementation required) · accept (documented risk) · transfer (third-party)*

---

## Accepted Risks Log

| Risk ID | Threat Ref | Rationale | Accepted By | Date |
|---------|------------|-----------|-------------|------|

*No accepted risks.*

---

## Security Audit Trail

| Audit Date | Threats Total | Closed | Open | Run By |
|------------|---------------|--------|------|--------|
| 2026-05-06 | 3 | 3 | 0 | gsd-security-auditor |

### Audit 2026-05-06

| Metric | Count |
|--------|-------|
| Threats found | 3 |
| Closed | 3 |
| Open | 0 |

Auditor return: `## SECURED`. Each threat verified by greppable code evidence at cited file:line, not by documentation alone. Phase 1 baseline (T-1-101..T-1-301) unchanged per phase scope; out of scope for this audit.

---

## Sign-Off

- [x] All threats have a disposition (mitigate / accept / transfer)
- [x] Accepted risks documented in Accepted Risks Log (none required)
- [x] `threats_open: 0` confirmed
- [x] `status: verified` set in frontmatter

**Approval:** verified 2026-05-06
