---
phase: 3
slug: read-operations
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-08
---

# Phase 3 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | bun:test (built-in, Bun 1.3.10) |
| **Config file** | none |
| **Quick run command** | `bun test bindings/js/test/read.test.ts` |
| **Full suite command** | `bun test bindings/js/test/` |
| **Estimated runtime** | ~5 seconds |

---

## Sampling Rate

- **After every task commit:** Run `bun test bindings/js/test/`
- **After every plan wave:** Run `bun test bindings/js/test/`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 5 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 03-01-01 | 01 | 1 | READ-01 | integration | `bun test bindings/js/test/read.test.ts -t "readScalarIntegers"` | ❌ W0 | ⬜ pending |
| 03-01-02 | 01 | 1 | READ-01 | integration | `bun test bindings/js/test/read.test.ts -t "readScalarFloats"` | ❌ W0 | ⬜ pending |
| 03-01-03 | 01 | 1 | READ-01 | integration | `bun test bindings/js/test/read.test.ts -t "readScalarStrings"` | ❌ W0 | ⬜ pending |
| 03-01-04 | 01 | 1 | READ-01 | integration | `bun test bindings/js/test/read.test.ts -t "empty"` | ❌ W0 | ⬜ pending |
| 03-01-05 | 01 | 1 | READ-02 | integration | `bun test bindings/js/test/read.test.ts -t "readScalarIntegerById"` | ❌ W0 | ⬜ pending |
| 03-01-06 | 01 | 1 | READ-02 | integration | `bun test bindings/js/test/read.test.ts -t "readScalarFloatById"` | ❌ W0 | ⬜ pending |
| 03-01-07 | 01 | 1 | READ-02 | integration | `bun test bindings/js/test/read.test.ts -t "readScalarStringById"` | ❌ W0 | ⬜ pending |
| 03-01-08 | 01 | 1 | READ-03 | integration | `bun test bindings/js/test/read.test.ts -t "readElementIds"` | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `bindings/js/test/read.test.ts` — test stubs for READ-01, READ-02, READ-03
- [ ] `bindings/js/src/read.ts` — all read operation implementations

*Existing infrastructure covers test framework needs.*

---

## Manual-Only Verifications

*All phase behaviors have automated verification.*

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 5s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
