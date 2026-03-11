---
phase: 8
slug: convenience-composites
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-11
---

# Phase 8 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | bun:test (built-in) |
| **Config file** | none (bun:test uses conventions) |
| **Quick run command** | `cd bindings/js && bun test test/composites.test.ts` |
| **Full suite command** | `bindings/js/test/test.bat` |
| **Estimated runtime** | ~5 seconds |

---

## Sampling Rate

- **After every task commit:** Run `cd bindings/js && bun test test/composites.test.ts`
- **After every plan wave:** Run `bindings/js/test/test.bat`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 5 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 08-01-01 | 01 | 0 | JSCONV-01 | stub | `cd bindings/js && bun test test/composites.test.ts` | ❌ W0 | ⬜ pending |
| 08-01-02 | 01 | 1 | JSCONV-01a | unit | `cd bindings/js && bun test test/composites.test.ts` | ❌ W0 | ⬜ pending |
| 08-01-03 | 01 | 1 | JSCONV-01b | unit | `cd bindings/js && bun test test/composites.test.ts` | ❌ W0 | ⬜ pending |
| 08-01-04 | 01 | 1 | JSCONV-01c | unit | `cd bindings/js && bun test test/composites.test.ts` | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `bindings/js/test/composites.test.ts` — test stubs for JSCONV-01a/b/c
- [ ] `bindings/js/src/composites.ts` — implementation file stub

*Existing infrastructure covers framework — bun:test is built-in. No new test schemas needed — `basic.sql` and `composite_helpers.sql` already exist.*

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
