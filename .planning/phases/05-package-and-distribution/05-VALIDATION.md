---
phase: 5
slug: package-and-distribution
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-09
---

# Phase 5 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | bun:test (built-in, Bun 1.3.10) |
| **Config file** | bunfig.toml (created in Wave 0) |
| **Quick run command** | `cd bindings/js && bun test` |
| **Full suite command** | `bindings/js/test/test.bat` |
| **Estimated runtime** | ~5 seconds |

---

## Sampling Rate

- **After every task commit:** Run `cd bindings/js && bun test`
- **After every plan wave:** Run `bindings/js/test/test.bat` + `cd bindings/js && bun run typecheck` + `cd bindings/js && bun run lint`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 10 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 05-01-01 | 01 | 1 | PKG-01 | manual | Verify `files` field + `bun pm pack` output | N/A | ⬜ pending |
| 05-01-02 | 01 | 1 | PKG-02 | unit | `cd bindings/js && bun run typecheck` | ✅ tsconfig.json | ⬜ pending |
| 05-01-03 | 01 | 1 | PKG-03 | unit | `cd bindings/js && bun test` | ✅ 5 test files | ⬜ pending |
| 05-01-04 | 01 | 1 | PKG-04 | integration | `bindings/js/test/test.bat` | ✅ test.bat | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `bindings/js/bunfig.toml` — test preload configuration for standalone `bun test`
- [ ] `bindings/js/test/setup.ts` — DLL PATH setup preload script
- [ ] `bindings/js/biome.json` — Biome configuration
- [ ] `@biomejs/biome` dev dependency installation

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Package installable via `bun add` | PKG-01 | Requires external project context | Run `bun pm pack`, verify output includes src/, README.md |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 10s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
