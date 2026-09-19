---
phase: 1
slug: enum-labels-in-describe
# status lifecycle: draft (seeded by plan-phase) → validated (set by validate-phase §6)
# audit-milestone §5.5 distinguishes NOT-VALIDATED (draft) from PARTIAL (validated + nyquist_compliant: false) (#2117)
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-09-19
---

# Phase 1 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | GoogleTest 1.17.0 (C++ core + C API); per-binding native runners (Test.jl, Dart `test`, pytest 8.4.1+, bun:test) |
| **Config file** | `tests/CMakeLists.txt` (C++/C API); each binding's own test runner config |
| **Quick run command** | `./build/bin/quiver_tests.exe --gtest_filter='*Describe*'` |
| **Full suite command** | `scripts/test-all.bat` |
| **Estimated runtime** | quick ~5 s; full suite ~3–5 min (6 suites + CLI smoke) |

---

## Sampling Rate

- **After every task commit:** Run `./build/bin/quiver_tests.exe --gtest_filter='*Describe*'`
- **After every plan wave:** Run `scripts/test-all.bat`
- **Before `/gsd-verify-work`:** Full suite must be green, **plus** two phase-gate checks that a
  green suite does not cover:
  - `git diff --stat` shows no path under `bindings/` (Success Criterion 5)
  - byte-diff of `describe()` / `describe_collection()` / `summarize_collection()` output on a
    sidecar-less database against `master` (Success Criterion 3 demands a diff, not "tests pass")
- **Max feedback latency:** 10 seconds (quick filter), 300 seconds (full suite)

---

## Per-Task Verification Map

> Task IDs are assigned by the planner; this map is keyed by requirement group and is refined
> per-task during execution. Every row below is ❌ W0 — none of this test surface exists today.

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| TBD | TBD | 0 | CORPUS-01..03 | — | N/A (fixture data) | fixture | n/a — data, not test code | ❌ W0 (`tests/schemas/ui/` absent) | ⬜ pending |
| TBD | TBD | 0 | PARSE-01..12 | — | Malformed/absent config never publishes partial state | integration | `./build/bin/quiver_tests.exe --gtest_filter='DatabaseUiDescribe*'` | ❌ W0 (new file + fixture helper) | ⬜ pending |
| TBD | TBD | 1 | DESC-01..06 | — | N/A | integration | `./build/bin/quiver_tests.exe --gtest_filter='DatabaseUiDescribe*'` | ❌ W0 | ⬜ pending |
| TBD | TBD | 1 | DESC-03 (byte-identical no-config) | — | Degraded path is indistinguishable from today | integration + diff | quick filter **and** manual byte-diff vs `master` | ❌ W0 | ⬜ pending |
| TBD | TBD | 2 | DESC-07 (Lua) | — | N/A | integration | `./build/bin/quiver_tests.exe --gtest_filter='*LuaRunnerDescribe*'` | ❌ W0 (exact-string asserts are new) | ⬜ pending |
| TBD | TBD | 2 | DESC-07 (Julia) | — | N/A | integration | `bindings/julia/test/test.bat` | ❌ W0 (suite asserts only "is a String") | ⬜ pending |
| TBD | TBD | 2 | DESC-07 (Dart) | — | N/A | integration | `bindings/dart/test/test.bat` | ❌ W0 (same) | ⬜ pending |
| TBD | TBD | 2 | DESC-07 (Python) | — | N/A | integration | `bindings/python/tests/test.bat` | ❌ W0 (same; do **not** reuse the existing `tmp_path` fixture) | ⬜ pending |
| TBD | TBD | 2 | DESC-07 (JS/Bun) | — | N/A | integration | `bindings/js/test/test.bat` | ❌ W0 (same) | ⬜ pending |
| TBD | TBD | 2 | DESC-07 (C API) | — | N/A | integration | `./build/bin/quiver_c_tests.exe` | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/schemas/ui/` fixture tree — does not exist. One directory per D-27 tolerance, plus the
      three CORPUS-01 named cases. Tolerances and named cases may share a directory where the real
      file already exhibits several at once (the BESSOperation-shaped fixture covers all-bare-string,
      the `degradation` attribute/group namespace collision, and interleaved `[[attribute]]` /
      `[[attribute_group]]` blocks simultaneously).
- [ ] New C++ test fixture helper — builds a **file-backed** database inside
      `tests/schemas/ui/<name>/`. Distinct from `LuaSandboxTest`, which uses a system temp dir that
      D-28 forbids here. Needed because `UIConfigSet` is private (D-18) and `tests/CMakeLists.txt`
      gives `quiver_tests` no include path into `src/` — every PARSE tolerance must be exercised
      **indirectly** through `Database::describe()` / `has_ui_config()`.
- [ ] `tests/test_database_ui_describe.cpp` (matching `tests/CLAUDE.md`'s `test_database_<area>.cpp`
      convention), registered in `tests/CMakeLists.txt`.
- [ ] Per-binding fixture helper for "file-backed db beside a `ui/` fixture directory", discovered
      through each binding's existing `tests_path()` / `schemas_path()`-style helper. None of the
      four weak binding suites does this today.
- [ ] `src/CMakeLists.txt` gains `ui_config.cpp` in the `quiver` target's source list.

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| No file under `bindings/` changed | Success Criterion 5 | A test suite cannot assert on its own diff | `git diff --stat master...HEAD -- bindings/` must be empty |
| `describe*` output byte-identical with no `ui/` present | DESC-03 / Success Criterion 3 | The criterion explicitly demands a diff against `master`, not a green assertion | Capture all three reports on a sidecar-less db at `master` and at HEAD; `diff` must be empty |
| `CHANGELOG.md` heads 0.10.6 (the version `CMakeLists.txt` carries) | Success Criterion 5 | Editorial reconciliation, not testable behavior | Read `CHANGELOG.md` head vs `CMakeLists.txt` `project(... VERSION ...)` |
| Warning is logged (not just "open() succeeded") on a malformed `ui/` | PARSE / DESC-03 | spdlog console output is not captured by the assertions | Run with `console_level` at warn; observe the message names the offending path |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 300s (full suite); < 10s (quick filter)
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
