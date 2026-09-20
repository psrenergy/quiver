---
phase: 1
slug: sidecar-reader-and-attribute-meaning
# status lifecycle: draft (seeded by plan-phase) → validated (set by validate-phase §6)
# audit-milestone §5.5 distinguishes NOT-VALIDATED (draft) from PARTIAL (validated + nyquist_compliant: false) (#2117)
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-09-20
---

# Phase 1 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | GoogleTest v1.17.0 (already a build dependency — `cmake/Dependencies.cmake`) |
| **Config file** | `tests/CMakeLists.txt` (target `quiver_tests`) |
| **Quick run command** | `build/bin/quiver_tests.exe --gtest_filter=*Ui*:*DatabaseUiMetadata*` |
| **Full suite command** | `build/bin/quiver_tests.exe` |
| **Estimated runtime** | ~20 seconds (quick subset: ~2 seconds) |

---

## Sampling Rate

- **After every task commit:** Run `build/bin/quiver_tests.exe --gtest_filter=*Ui*:*DatabaseUiMetadata*`
- **After every plan wave:** Run `build/bin/quiver_tests.exe` **and** `build/bin/quiver_c_tests.exe`
- **Before `/gsd-verify-work`:** Full `quiver_tests` suite must be green
- **Max feedback latency:** 30 seconds

No binding suite is required: all four binding `describe` suites assert only "returns a string",
and this phase makes zero binding code changes (REQUIREMENTS.md "Out of Scope").

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 1-00-01 | 00 | 0 | (infra) | — | N/A | scaffold | `cmake --build build --config Debug` | ❌ W0 | ⬜ pending |
| 1-01-01 | 01 | 1 | READ-01 | — | Sibling `ui/` resolved via `weakly_canonical(...).parent_path()`; never `<migrations>/ui`, never CWD | unit | `build/bin/quiver_tests.exe --gtest_filter=*UiConfig*PathResolution*` | ❌ W0 | ⬜ pending |
| 1-01-02 | 01 | 1 | READ-02 | — | `main.toml` / `enum.toml` / `themes/*.toml` never selected as a collection file | unit | `build/bin/quiver_tests.exe --gtest_filter=*UiConfig*ShapeSelection*` | ❌ W0 | ⬜ pending |
| 1-01-03 | 01 | 1 | READ-03 | — | Label/tooltip keyed by file `id` + attribute `id` | unit | `build/bin/quiver_tests.exe --gtest_filter=*UiConfig*LabelTooltip*` | ❌ W0 | ⬜ pending |
| 1-01-04 | 01 | 1 | READ-04 | — | String vs `table.en`; `\n`/`\r` collapse to one space; UTF-8 bytes untranscoded | unit | `build/bin/quiver_tests.exe --gtest_filter=*UiConfig*Localized*` | ❌ W0 | ⬜ pending |
| 1-01-05 | 01 | 1 | READ-05 | — | Gapped `[0,2]` and 1-based `[1..7]` vocabularies render real codes; joined by `enum` value, code from entry `id` | unit | `build/bin/quiver_tests.exe --gtest_filter=*UiConfig*Enum*` | ❌ W0 | ⬜ pending |
| 1-01-06 | 01 | 1 | SAFE-02 | — | Missing / empty / zero-byte / unparseable `ui/` → warn + empty map, never throw | unit | `build/bin/quiver_tests.exe --gtest_filter=*UiConfig*Malformed*` | ❌ W0 | ⬜ pending |
| 1-02-01 | 02 | 2 | RENDER-01 | — | `describe` / `describe_collection` append label, tooltip, enum codes after name/type/flags | integration | `build/bin/quiver_tests.exe --gtest_filter=*DatabaseUiMetadata*Render*` | ❌ W0 | ⬜ pending |
| 1-02-02 | 02 | 2 | RENDER-03 | — | 3 undescribed cases (no ui file / no entry / dangling column) render byte-unchanged | integration | `build/bin/quiver_tests.exe --gtest_filter=*DatabaseUiMetadata*Undescribed*` | ❌ W0 | ⬜ pending |
| 1-02-03 | 02 | 2 | SAFE-01 | — | No `ui/` → all three reports byte-identical to a sidecar-deleted run | regression | `build/bin/quiver_tests.exe --gtest_filter=*DatabaseUiMetadata*NoUiDir*` | ❌ W0 | ⬜ pending |
| 1-02-04 | 02 | 2 | SAFE-02 | — | Malformed sidecar → `from_migrations` still opens; reports identical to no-sidecar | integration | `build/bin/quiver_tests.exe --gtest_filter=*DatabaseUiMetadata*Malformed*` | ❌ W0 | ⬜ pending |
| 1-02-05 | 02 | 2 | RENDER-01 (anti-drift) | — | `describe()` scalar line is a strict prefix of `describe_collection()`'s, for every scalar | integration | `build/bin/quiver_tests.exe --gtest_filter=*DatabaseUiMetadata*PrefixInvariant*` | ❌ W0 | ⬜ pending |
| 1-02-06 | 02 | 2 | Success criterion 6 | — | Existing describe assertions pass **unmodified** | regression | `build/bin/quiver_tests.exe --gtest_filter=*DescribeVectorsHeaderPrintedOnce*:*DescribeSetsHeaderPrintedOnce*:*DescribeTimeSeriesWithDimensionColumn*:*DescribeNoCategoryHeaderWhenEmpty*` | ✅ exists | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/test_database_ui_metadata.cpp` — new file, registered in `tests/CMakeLists.txt`,
      covering every row in the map above
- [ ] A temp-dir fixture builder extending the `tests/test_migrations.cpp:12-32` / `:194-199`
      idiom to also write a sibling `ui/` directory from caller-supplied `.toml` contents
- [ ] Framework install: **none** — GoogleTest is already a build dependency

Nothing is committed under `tests/schemas/ui/`; all fixtures are per-test temp dirs.

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Rendering against a real PSR study corpus (`HydroThermalDispatch`, `GNoMo`, `Foresight`, `CarbSteeler`) | RENDER-01, READ-05 | The corpora live outside this repo and cannot be committed as fixtures | Open a real study with `from_migrations`, call `describe_collection`, confirm the enum clause matches the model's own declarations (noting the recorded `HasCommitment` inversion) |

All other phase behaviors have automated verification.

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 30s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
