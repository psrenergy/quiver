---
phase: 3
slug: metadata
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-09
---

# Phase 3 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | bun:test (built-in) |
| **Config file** | none — bun:test auto-discovers *.test.ts |
| **Quick run command** | `cd bindings/js && bun test test/metadata.test.ts` |
| **Full suite command** | `cd bindings/js && bun test` |
| **Estimated runtime** | ~5 seconds |

---

## Sampling Rate

- **After every task commit:** Run `cd bindings/js && bun test test/metadata.test.ts`
- **After every plan wave:** Run `cd bindings/js && bun test`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 5 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 03-01-01 | 01 | 1 | JSMETA-01 | unit | `cd bindings/js && bun test test/metadata.test.ts` | ❌ W0 | ⬜ pending |
| 03-01-02 | 01 | 1 | JSMETA-01 | unit | `cd bindings/js && bun test test/metadata.test.ts` | ❌ W0 | ⬜ pending |
| 03-01-03 | 01 | 1 | JSMETA-01 | unit | `cd bindings/js && bun test test/metadata.test.ts` | ❌ W0 | ⬜ pending |
| 03-01-04 | 01 | 1 | JSMETA-01 | unit | `cd bindings/js && bun test test/metadata.test.ts` | ❌ W0 | ⬜ pending |
| 03-01-05 | 01 | 1 | JSMETA-02 | unit | `cd bindings/js && bun test test/metadata.test.ts` | ❌ W0 | ⬜ pending |
| 03-01-06 | 01 | 1 | JSMETA-02 | unit | `cd bindings/js && bun test test/metadata.test.ts` | ❌ W0 | ⬜ pending |
| 03-01-07 | 01 | 1 | JSMETA-02 | unit | `cd bindings/js && bun test test/metadata.test.ts` | ❌ W0 | ⬜ pending |
| 03-01-08 | 01 | 1 | JSMETA-02 | unit | `cd bindings/js && bun test test/metadata.test.ts` | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `bindings/js/test/metadata.test.ts` — stubs for JSMETA-01, JSMETA-02

*Existing infrastructure covers framework setup.*

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
