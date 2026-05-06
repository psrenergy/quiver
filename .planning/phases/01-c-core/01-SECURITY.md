---
phase: 01
slug: c-core
status: verified
threats_open: 0
asvs_level: 1
created: 2026-05-06
---

# Phase 01 — Security

> Per-phase security contract for the `quiver::expr` C++ core: threat register, accepted risks, and audit trail.

Surface: C++ embedded library; local file I/O via existing `BinaryFile`; no network, no auth, no PII.

---

## Trust Boundaries

| Boundary | Description | Data Crossing |
|----------|-------------|---------------|
| User-supplied operand metadata → `BinaryOpNode` ctor | Validated eagerly via D-01..D-09; every check throws `Cannot apply binary operation: …` on rejection. | Dimension lists, labels, time properties, units. |
| User-supplied path → `Expression::save` | Validated via D-11 self-save check, then delegated to `BinaryFile::open_file('w', …)` (existing canonicalization + write registry). | Filesystem paths only; no content sensitivity. |
| Engine read-side → `BinaryFile::read_into` | NEW trusted-caller fast path; engine guarantees valid dims via `quiver::binary::next_dimensions`. External callers must use the validating `read(vector<int64_t>)` overload. | Dimension index vectors; `vector<double>` row buffers. |
| User-supplied path → `BinaryFile::open_file` | Pre-existing trust boundary; unchanged in Phase 01. | Filesystem paths. |
| Test inputs → `ExpressionFixture` | All inputs are programmatically generated within tests; no external input. | N/A — test-only. |

---

## Threat Register

| Threat ID | Category | Component | Disposition | Mitigation | Status |
|-----------|----------|-----------|-------------|------------|--------|
| T-1-101 | T (Tampering) | `BinaryFile::read_into` | mitigate | Trusted-caller fast-path docstring at `include/quiver/binary/binary_file.h:44-47`. Engine (Plan 02) is the only intended caller; verified in `01-VERIFICATION.md` Required Artifacts. External code paths use the validating `read(vector<int64_t>, bool)` overload. | closed |
| T-1-102 | T | `BinaryFile::next_dimensions` delegate | mitigate | Delegate at `src/binary/binary_file.cpp:381-392` translates the free function's `nullopt` to `first_dimensions(meta)`, preserving the existing `csv_converter` loop semantics. Regression: 37 `CSVConverterFixture` tests included in the 781/0 full-suite pass. | closed |
| T-1-103 | I (Information disclosure) | `read_into` null-cell error message | accept | Error format `"Cannot read: data at {row=N, col=M} contains null values"` mirrors the existing `read(unordered_map…)` error format. No new disclosure surface. See Accepted Risks Log RISK-01. | closed |
| T-1-201 | T | `Expression::save` self-save | mitigate | D-11 eager pre-check at `src/expr/expression.cpp:63-70`: `collect_file_paths` walks the AST, canonicalizes input + output paths via `std::filesystem::weakly_canonical`, throws `"Cannot save: output path collides with input file '<path>'"` BEFORE `BinaryFile::open_file('w', path)` at line 74. Verified by `ExpressionFixture.SelfSaveCollisionThrows` (input file size + first 64 bytes unchanged after the throw). | closed |
| T-1-202 | T | D-11 path canonicalization | mitigate | `std::filesystem::weakly_canonical` applied to BOTH the output path and every collected input path; same canonicalization the `BinaryFile` write registry uses. Verified by `ExpressionFixture.SelfSaveCollisionThrowsWithCanonicalizedPath` (non-canonical path normalizes to the same canonical form and triggers the throw). | closed |
| T-1-203 | I | `BinaryOpNode::compute_row` divide-by-zero | accept | Division by zero produces IEEE 754 NaN/inf written to output. Project policy: clean code over defensive — downstream consumers handle NaN. Documented in `src/expr/nodes.cpp` `apply_op`. See Accepted Risks Log RISK-02. | closed |
| T-1-204 | T | `mutable` buffer members on `BinaryOpNode` + `FileNode` | accept | Project policy is single-threaded use (CONCERNS.md). Comment in `include/quiver/expr/node.h` documents the policy. No concurrency tests required. See Accepted Risks Log RISK-03. | closed |
| T-1-301 | T | `LargeGridCompletes` timing assertion | accept-with-mitigation | 5-second wall-clock budget is intentionally generous; load-bearing CORE-06 verification is the grep audit on production source (`expression.cpp:78`, `node.h:123-126`). Test currently completes in 244 ms on dev hardware. If the timing assertion ever flakes on CI, drop the wall-clock check; the test still verifies value correctness independently. See Accepted Risks Log RISK-04. | closed |

*Status: open · closed*
*Disposition: mitigate (implementation required) · accept (documented risk) · transfer (third-party)*

---

## Accepted Risks Log

| Risk ID | Threat Ref | Rationale | Accepted By | Date |
|---------|------------|-----------|-------------|------|
| RISK-01 | T-1-103 | `read_into` null-cell error format mirrors the pre-existing `read(unordered_map<string,int64_t>, bool)` error format. No new information leak; bindings are deferred so no new external surface. | Phase 01 owner | 2026-05-06 |
| RISK-02 | T-1-203 | Division by zero produces IEEE 754 NaN/inf rather than throwing. Aligns with the project's "clean code over defensive code" philosophy (CLAUDE.md); downstream consumers handle NaN explicitly. | Phase 01 owner | 2026-05-06 |
| RISK-03 | T-1-204 | `mutable` row-reuse buffers on `BinaryOpNode` and `FileNode` are intentional for the CORE-06 single-buffer design. Library is documented single-threaded (`.planning/research/CONCERNS.md`); concurrent `compute_row` calls are out of scope. | Phase 01 owner | 2026-05-06 |
| RISK-04 | T-1-301 | `LargeGridCompletes` 5-second wall-clock budget is a smoke backstop, not the load-bearing verification. CORE-06 is verified primarily by the grep audit on production source. Timing flakes can be removed without weakening the contract. | Phase 01 owner | 2026-05-06 |

---

## Security Audit Trail

| Audit Date | Threats Total | Closed | Open | Run By |
|------------|---------------|--------|------|--------|
| 2026-05-06 | 8 | 8 | 0 | Claude (gsd-secure-phase, State B) |

**Audit notes:** All four `mitigate`-disposition threats (T-1-101, T-1-102, T-1-201, T-1-202) have implementations confirmed by `01-VERIFICATION.md` (Required Artifacts table + Behavioral Spot-Checks: 781/0 tests across 34 suites including the 4 SelfSaveCollisionThrows / TimePropertiesMismatchThrows / FastReadOverload / FastWriteOverload contracts). The four `accept` / `accept-with-mitigation` threats (T-1-103, T-1-203, T-1-204, T-1-301) are recorded in the Accepted Risks Log above. No CR-* finding from `01-REVIEW.md` introduces a new threat — CR-01/CR-02/CR-03 are correctness/quality issues, not new attack surfaces.

---

## Sign-Off

- [x] All threats have a disposition (mitigate / accept / transfer)
- [x] Accepted risks documented in Accepted Risks Log
- [x] `threats_open: 0` confirmed
- [x] `status: verified` set in frontmatter

**Approval:** verified 2026-05-06
