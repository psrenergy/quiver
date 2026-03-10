---
phase: 6
slug: introspection-lua
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-10
---

# Phase 6 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | bun:test (built-in) |
| **Config file** | none — bun:test uses convention |
| **Quick run command** | `cd bindings/js && bun test test/introspection.test.ts test/lua-runner.test.ts` |
| **Full suite command** | `cd bindings/js && bun test` |
| **Estimated runtime** | ~5 seconds |

---

## Sampling Rate

- **After every task commit:** Run `cd bindings/js && bun test test/introspection.test.ts test/lua-runner.test.ts`
- **After every plan wave:** Run `cd bindings/js && bun test`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 5 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 06-01-01 | 01 | 1 | JSDB-01 | unit | `cd bindings/js && bun test test/introspection.test.ts` | ❌ W0 | ⬜ pending |
| 06-01-02 | 01 | 1 | JSLUA-01 | integration | `cd bindings/js && bun test test/lua-runner.test.ts` | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `bindings/js/test/introspection.test.ts` — stubs for JSDB-01 (isHealthy, currentVersion, path, describe)
- [ ] `bindings/js/test/lua-runner.test.ts` — stubs for JSLUA-01 (create from Lua, error handling, lifecycle)

*Existing infrastructure covers framework — bun:test already in use.*

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
