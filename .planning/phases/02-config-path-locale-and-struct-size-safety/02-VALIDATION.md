---
phase: 2
slug: config-path-locale-and-struct-size-safety
# status lifecycle: draft (seeded by plan-phase) → validated (set by validate-phase §6)
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-09-19
---

# Phase 2 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.
> All nine requirements start at **zero** test coverage — confirmed by reading every relevant
> source file. No existing test anywhere would catch an FFI layout mismatch.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | GoogleTest 1.17.0 (C++ / C API); Test.jl, Dart `test` 1.31.2+, pytest 8.4.1+, `bun:test` per binding |
| **Config file** | None dedicated — each suite's `test.bat` is the config |
| **Quick run command** | `./build/bin/quiver_tests.exe --gtest_filter='DatabaseUi*'` |
| **Full suite command** | `scripts/test-all.bat` |
| **Estimated runtime** | quick ~5 s; full suite ~3–5 min, **plus a one-time Dart native rebuild** once D-15 lands |

**D-15 note:** `bindings/dart/test/test.bat` gains cache clearing this phase. Every Dart run then
pays a native rebuild. That is the accepted cost of not silently testing the old struct layout.

---

## Sampling Rate

- **After every task commit:** the single relevant suite's quick filter
- **After every plan wave:** the full suite for every layer touched in that wave
- **Before `/gsd-verify-work`:** `scripts/test-all.bat` green, **plus** a manual `quiver_cli.exe`
  probe using the new `--ui-config-dir` / `--ui-locale` flags against the explicit-directory
  fixture — the CLI is the Lua host and has no other automated gate
- **Max feedback latency:** 10 s (quick), ~300 s (full suite, + Dart rebuild)

---

## Per-Task Verification Map

Task IDs are assigned by the planner. Every row is ❌ W0 — none of this surface exists.

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| TBD | TBD | 1 | SAFE-01 | T-02-01 | Native `sizeof` is the single source of truth | unit (C API) | `quiver_c_tests.exe --gtest_filter='*Sizeof*'` | ❌ W0 | ⬜ pending |
| TBD | TBD | 1 | OPT-01 | — | Explicit dir overrides convention, incl. `:memory:` (D-01) | unit (C++) | `quiver_tests.exe --gtest_filter='DatabaseUiDescribe.ExplicitConfigDir*'` | ❌ W0 | ⬜ pending |
| TBD | TBD | 1 | OPT-01 | T-02-02 | A missing **explicit** dir warns (not debug) and never throws (D-05) | unit (C++) | same filter | ❌ W0 | ⬜ pending |
| TBD | TBD | 1 | OPT-02 | — | A non-`en` locale changes a rendered label vs its `en` resolution | unit (C++) | `quiver_tests.exe --gtest_filter='*LocaleAffects*'` | ❌ W0 | ⬜ pending |
| TBD | TBD | 1 | OPT-04 | — | `has_ui_config()` round-trips through the C API | unit (C API) | `quiver_c_tests.exe --gtest_filter='*HasUiConfig*'` | ❌ W0 | ⬜ pending |
| TBD | TBD | 2 | OPT-05, SAFE-02 | **T-02-03** | JS: 24-byte buffer from named offsets; **string keepalive survives the call**; `allocPtrOut`/`allocUint64Out` pinned unchanged | unit (JS) | `bun test test/ffi-helpers.test.ts` | ❌ W0 — file absent | ⬜ pending |
| TBD | TBD | 2 | OPT-03, OPT-06, SAFE-02 | — | Julia: regenerated `c_api.jl`, options kwargs, load assertion | unit (Julia) | `bindings/julia/test/test.bat` | ❌ W0 | ⬜ pending |
| TBD | TBD | 2 | OPT-03, OPT-06, SAFE-02 | — | Dart: hand-edited `bindings.dart`, **assertion actively calls** the accessor (symbols resolve lazily) | unit (Dart) | `bindings/dart/test/test.bat` | ❌ W0 | ⬜ pending |
| TBD | TBD | 2 | OPT-03, OPT-06, SAFE-02 | — | Python: cdef matches native; `ffi.sizeof(...)` assertion | unit (Python) | `bindings/python/tests/test.bat` | ❌ W0 | ⬜ pending |
| TBD | TBD | 2 | SAFE-02, SAFE-03 | T-02-01 | **The throw path itself fires** — an injected wrong value must fail, per binding, for all three structs | unit ×4 bindings | per-binding | ❌ W0 | ⬜ pending |
| TBD | TBD | 3 | OPT-03, OPT-04 | — | Lua host: `--ui-config-dir` / `--ui-locale` CLI flags | integration | `quiver_cli.exe` probe + `LuaRunnerDescribe` case | ❌ W0 | ⬜ pending |
| TBD | TBD | 3 | D-15 | — | Dart `test.bat` clears both caches | behavioral | inspect `test.bat`; confirm a rebuild occurs | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/schemas/ui_explicit/` — a `ui/` tree deliberately **not** beside its database. Must not
      disturb `DatabaseUiCorpus`'s directory walk or `FixtureLiteralsArePinned` (both Phase 1).
- [ ] `tests/test_database_ui_describe.cpp` — new cases `ExplicitConfigDirOverridesConvention`,
      `ExplicitConfigDirLoadsEvenForMemoryDatabase`, `ExplicitConfigDirMissingLogsWarnNotDebug`,
      `LocaleAffectsRenderedLabel`.
- [ ] C API test coverage for the three `*_sizeof()` accessors and the `has_ui_config` symbol.
- [ ] `bindings/js/test/ffi-helpers.test.ts` — does not exist. Needed to pin `allocPtrOut` /
      `allocUint64Out` as unchanged (D-11) **and** to cover the keepalive hazard.
- [ ] One failure-path test per binding for the load-time size assertion (criterion 5). Asserting
      the happy path proves nothing — the test must inject a wrong expected value and see it throw.
- [ ] Any new asserted literal must be added to `tests/schemas/ui/README.md`'s authoritative table
      (L1…L17 today) or `FixtureLiteralsArePinned` will not cover it.

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| CLI flags reach the config | OPT-01/02 via Lua host | The CLI is the Lua host; no suite drives it | `quiver_cli.exe --schema … --ui-config-dir <dir> --ui-locale es <db> <script.lua>`; a label must render in Spanish |
| Dart cache clearing actually forces a rebuild | D-15 | Observable only as build behaviour | Run `test.bat` twice; confirm the native rebuild occurs and is not skipped |
| **BREAKING** CHANGELOG entry says what a caller must do | D-18 | Editorial | Read the entry; it must name the ABI break and the action required |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] The size-assertion **failure** path is tested in all four bindings, not just the happy path
- [ ] No watch-mode flags
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
