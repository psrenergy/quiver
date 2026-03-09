---
phase: 4
slug: query-and-transaction-control
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-09
---

# Phase 4 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | bun:test (built-in, Bun 1.3.10) |
| **Config file** | none |
| **Quick run command** | `bun test bindings/js/test/` |
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
| 04-01-01 | 01 | 1 | QUERY-01 | integration | `bun test bindings/js/test/query.test.ts -t "queryString"` | ❌ W0 | ⬜ pending |
| 04-01-02 | 01 | 1 | QUERY-01 | integration | `bun test bindings/js/test/query.test.ts -t "queryInteger"` | ❌ W0 | ⬜ pending |
| 04-01-03 | 01 | 1 | QUERY-01 | integration | `bun test bindings/js/test/query.test.ts -t "queryFloat"` | ❌ W0 | ⬜ pending |
| 04-01-04 | 01 | 1 | QUERY-01 | integration | `bun test bindings/js/test/query.test.ts -t "no rows"` | ❌ W0 | ⬜ pending |
| 04-01-05 | 01 | 1 | QUERY-02 | integration | `bun test bindings/js/test/query.test.ts -t "params"` | ❌ W0 | ⬜ pending |
| 04-01-06 | 01 | 1 | QUERY-02 | integration | `bun test bindings/js/test/query.test.ts -t "integer param"` | ❌ W0 | ⬜ pending |
| 04-01-07 | 01 | 1 | QUERY-02 | integration | `bun test bindings/js/test/query.test.ts -t "float param"` | ❌ W0 | ⬜ pending |
| 04-01-08 | 01 | 1 | QUERY-02 | integration | `bun test bindings/js/test/query.test.ts -t "null param"` | ❌ W0 | ⬜ pending |
| 04-01-09 | 01 | 1 | QUERY-02 | integration | `bun test bindings/js/test/query.test.ts -t "multiple"` | ❌ W0 | ⬜ pending |
| 04-02-01 | 02 | 1 | TXN-01 | integration | `bun test bindings/js/test/transaction.test.ts -t "commit"` | ❌ W0 | ⬜ pending |
| 04-02-02 | 02 | 1 | TXN-01 | integration | `bun test bindings/js/test/transaction.test.ts -t "rollback"` | ❌ W0 | ⬜ pending |
| 04-02-03 | 02 | 1 | TXN-01 | integration | `bun test bindings/js/test/transaction.test.ts -t "without begin"` | ❌ W0 | ⬜ pending |
| 04-02-04 | 02 | 1 | TXN-02 | integration | `bun test bindings/js/test/transaction.test.ts -t "inTransaction"` | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `bindings/js/test/query.test.ts` — stubs for QUERY-01, QUERY-02
- [ ] `bindings/js/test/transaction.test.ts` — stubs for TXN-01, TXN-02

*Existing infrastructure covers framework and fixtures.*

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
