---
phase: 2
slug: update-collection-reads
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-09
---

# Phase 2 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | bun:test (built-in) |
| **Config file** | none — bun:test auto-discovers `*.test.ts` |
| **Quick run command** | `cd bindings/js && bun test test/update.test.ts test/read.test.ts` |
| **Full suite command** | `bindings/js/test/test.bat` |
| **Estimated runtime** | ~5 seconds |

---

## Sampling Rate

- **After every task commit:** Run `cd bindings/js && bun test test/update.test.ts test/read.test.ts`
- **After every plan wave:** Run `bindings/js/test/test.bat`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 5 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 02-01-01 | 01 | 1 | JSUP-01 | unit | `cd bindings/js && bun test test/update.test.ts` | No — W0 | ⬜ pending |
| 02-01-02 | 01 | 1 | JSRD-01 | unit | `cd bindings/js && bun test test/read.test.ts` | Partial | ⬜ pending |
| 02-01-03 | 01 | 1 | JSRD-02 | unit | `cd bindings/js && bun test test/read.test.ts` | Partial | ⬜ pending |
| 02-01-04 | 01 | 1 | JSRD-03 | unit | `cd bindings/js && bun test test/read.test.ts` | Partial | ⬜ pending |
| 02-01-05 | 01 | 1 | JSRD-04 | unit | `cd bindings/js && bun test test/read.test.ts` | Partial | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `bindings/js/test/update.test.ts` — stubs for JSUP-01
- [ ] Add vector/set `describe` blocks to `bindings/js/test/read.test.ts` — covers JSRD-01 through JSRD-04
- [ ] May need float vector table added to `tests/schemas/valid/all_types.sql`

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
