---
phase: 1
slug: ffi-foundation-and-database-lifecycle
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-08
---

# Phase 1 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | bun:test (built-in, Bun 1.3.10) |
| **Config file** | none — Wave 0 installs |
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
| 01-01-01 | 01 | 0 | FFI-01 | smoke | `bun test bindings/js/test/database.test.ts -t "loads library"` | ❌ W0 | ⬜ pending |
| 01-01-02 | 01 | 0 | FFI-02 | unit | `bun test bindings/js/test/database.test.ts -t "error handling"` | ❌ W0 | ⬜ pending |
| 01-01-03 | 01 | 0 | FFI-03 | integration | `bun test bindings/js/test/database.test.ts -t "fromSchema"` | ❌ W0 | ⬜ pending |
| 01-01-04 | 01 | 0 | FFI-04 | unit | `bun test bindings/js/test/database.test.ts -t "conversions"` | ❌ W0 | ⬜ pending |
| 01-01-05 | 01 | 1 | LIFE-01 | integration | `bun test bindings/js/test/database.test.ts -t "fromSchema"` | ❌ W0 | ⬜ pending |
| 01-01-06 | 01 | 1 | LIFE-02 | integration | `bun test bindings/js/test/database.test.ts -t "fromMigrations"` | ❌ W0 | ⬜ pending |
| 01-01-07 | 01 | 1 | LIFE-03 | unit | `bun test bindings/js/test/database.test.ts -t "close"` | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `bindings/js/test/database.test.ts` — stubs for FFI-01 through LIFE-03
- [ ] `bindings/js/test/test.bat` — test runner with PATH setup for DLL discovery
- [ ] `bindings/js/package.json` — package metadata
- [ ] `bindings/js/tsconfig.json` — TypeScript config

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| DLL auto-detection from package location | FFI-01 | Path resolution depends on install location | Install package, verify import loads DLLs without PATH setup |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 5s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
