---
phase: 09-ci-pipeline
verified: 2026-04-18T17:45:00Z
status: human_needed
score: 3/4 must-haves verified
overrides_applied: 0
human_verification:
  - test: "Trigger a push or PR to master and confirm the deno-test job appears and passes in the GitHub Actions run"
    expected: "deno-test job executes all steps (checkout, setup-deno, cmake build, deno task test) and all green"
    why_human: "Cannot invoke GitHub Actions runner locally; requires an actual push/PR to the remote repo"
---

# Phase 9: CI Pipeline Verification Report

**Phase Goal:** CI tests and publishes the Deno binding automatically
**Verified:** 2026-04-18T17:45:00Z
**Status:** human_needed
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths

| #  | Truth                                                                                                          | Status      | Evidence                                                                                                      |
|----|----------------------------------------------------------------------------------------------------------------|-------------|---------------------------------------------------------------------------------------------------------------|
| 1  | Push/PR to master triggers a deno-test job that runs JS binding tests on Ubuntu                                | VERIFIED    | ci.yml triggers on `push: branches: [master]` and `pull_request: branches: [master]`; `deno-test:` job at line 261 on ubuntu-latest; runs `deno task test` with LD_LIBRARY_PATH                        |
| 2  | publish.yml JSR publish job uses `npx jsr publish` with OIDC authentication (`id-token: write` permission)    | VERIFIED    | `publish-jsr:` job at line 104; `id-token: write` at line 111; `npx jsr publish --allow-slow-types` at line 133; no manual token secret referenced                                                     |
| 3  | No package.json, npm install, npm build, or npm publish references exist in any .github/workflows/*.yml file  | VERIFIED    | grep counts for `package.json`, `npm install`, `npm run build`, `npm publish`, `setup-node`, `build-native-npm`, `publish-npm` all returned 0 across ci.yml and publish.yml                            |
| 4  | The ci.yml Deno test job passes in GitHub Actions                                                              | NEEDS HUMAN | Cannot verify without triggering an actual Actions run; code structure is correct but runtime pass requires human                                                                                        |

**Score:** 3/4 truths verified (SC4 deferred to human verification)

### Required Artifacts

| Artifact                          | Expected                                          | Status     | Details                                                                              |
|-----------------------------------|---------------------------------------------------|------------|--------------------------------------------------------------------------------------|
| `.github/workflows/ci.yml`        | deno-test job appended to existing CI jobs        | VERIFIED   | `deno-test:` job present at line 261; follows same structure as julia-coverage, dart-coverage, python-coverage jobs |
| `.github/workflows/publish.yml`   | JSR publish job replacing npm publish jobs        | VERIFIED   | `publish-jsr:` job present at lines 104-133; `build-native-npm` and `publish-npm` jobs completely absent |

### Key Link Verification

| From                                           | To                              | Via                     | Status   | Details                                                                                              |
|------------------------------------------------|---------------------------------|-------------------------|----------|------------------------------------------------------------------------------------------------------|
| `.github/workflows/ci.yml deno-test job`       | `bindings/js/deno.json tasks.test` | `deno task test`       | WIRED    | Job uses `working-directory: bindings/js` and runs `deno task test`; `deno.json` defines `"test": "deno test --allow-ffi --allow-read --allow-write --allow-env test/"` |
| `.github/workflows/publish.yml publish-jsr job` | JSR registry                   | `npx jsr publish` with OIDC | WIRED | `id-token: write` permission set; `npx jsr publish --allow-slow-types` is the publish command; OIDC token exchanged at runtime with JSR |

### Data-Flow Trace (Level 4)

Not applicable — both artifacts are CI workflow YAML files, not components that render dynamic data. No data-flow trace required.

### Behavioral Spot-Checks

| Behavior                                | Command                                                                 | Result                    | Status |
|-----------------------------------------|-------------------------------------------------------------------------|---------------------------|--------|
| `deno-test` job name exists in ci.yml   | `grep "deno-test:" .github/workflows/ci.yml`                           | Line 261 matched          | PASS   |
| `denoland/setup-deno@v2` action present | `grep "denoland/setup-deno" .github/workflows/ci.yml`                  | Line 268 matched          | PASS   |
| `deno task test` invocation present     | `grep "deno task test" .github/workflows/ci.yml`                       | Line 284 matched          | PASS   |
| `LD_LIBRARY_PATH` set for FFI           | `grep "LD_LIBRARY_PATH.*build/lib" .github/workflows/ci.yml`           | Line 283 matched          | PASS   |
| `publish-jsr` job exists in publish.yml | `grep "publish-jsr:" .github/workflows/publish.yml`                    | Line 104 matched          | PASS   |
| OIDC permission present                 | `grep "id-token: write" .github/workflows/publish.yml`                 | Lines 95 and 111 matched  | PASS   |
| `npx jsr publish` command present       | `grep "jsr publish" .github/workflows/publish.yml`                     | Line 133 matched          | PASS   |
| `deno.json` version validation present  | `grep "deno.json" .github/workflows/publish.yml`                       | Lines 122, 125, 127 matched | PASS |
| Zero npm remnants (all workflow files)  | `grep -r "npm install\|npm publish\|package\.json" .github/workflows/` | No matches                | PASS   |
| `deno.json tasks.test` is fully defined | Checked `bindings/js/deno.json`                                         | `"test": "deno test --allow-ffi --allow-read --allow-write --allow-env test/"` | PASS |
| Test directory exists                   | `ls bindings/js/test/`                                                  | 9 `.test.ts` files present | PASS  |
| Summary commits exist in git history    | `git show --stat 709a775` / `git show --stat e3a19e8`                  | Both commits verified     | PASS   |

### Requirements Coverage

| Requirement | Source Plan | Description                                                   | Status      | Evidence                                                                                         |
|-------------|-------------|---------------------------------------------------------------|-------------|--------------------------------------------------------------------------------------------------|
| CI-01       | 09-01-PLAN  | Deno test job runs JS binding tests in ci.yml on push/PR      | SATISFIED   | `deno-test:` job present; trigger on push/PR to master; builds C++ and runs `deno task test`    |
| CI-02       | 09-01-PLAN  | publish.yml replaced with JSR publish workflow (OIDC auth)   | SATISFIED   | `publish-jsr:` job with `id-token: write` and `npx jsr publish --allow-slow-types`              |
| CI-03       | 09-01-PLAN  | Old npm publishing logic removed (no package.json/npm refs)  | SATISFIED   | All npm-related patterns (`npm install`, `npm publish`, `npm run build`, `package.json`, `setup-node`, `build-native-npm`, `publish-npm`) return 0 matches across all workflow files |

All three requirements declared in the PLAN frontmatter are accounted for and satisfied. No orphaned requirements found — REQUIREMENTS.md maps CI-01, CI-02, CI-03 to Phase 9 and no additional IDs are mapped to this phase.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| — | — | No TODO/FIXME/placeholder/empty implementation patterns found | — | — |

No anti-patterns detected in either workflow file.

### Human Verification Required

#### 1. Deno Test Job Passes in GitHub Actions

**Test:** Push a commit (or open a PR) to the master branch and observe the GitHub Actions CI run.
**Expected:** The `deno-test` job completes all four steps — checkout, `denoland/setup-deno@v2`, C++ CMake build, and `deno task test` — with a green status. All 9 test files in `bindings/js/test/` must pass.
**Why human:** Actual job execution requires the GitHub Actions runner environment on Ubuntu. The code structure is fully correct and all wiring is verified, but the runtime behavior (CMake build on ubuntu-latest, shared library resolution via LD_LIBRARY_PATH, deno FFI test execution) cannot be confirmed without a real CI run.

### Gaps Summary

No blocking gaps found. All three observable truths that can be verified programmatically pass. The sole outstanding item is SC4 from the ROADMAP: "The ci.yml Deno test job passes in GitHub Actions" — this requires a live CI run to confirm. The code structure, wiring, and all static checks are sound.

---

_Verified: 2026-04-18T17:45:00Z_
_Verifier: Claude (gsd-verifier)_
