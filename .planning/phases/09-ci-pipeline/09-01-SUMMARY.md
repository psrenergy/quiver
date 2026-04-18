---
phase: 09-ci-pipeline
plan: 01
subsystem: .github/workflows
tags: [ci, deno, jsr, github-actions]
dependency_graph:
  requires: [jsr-package-config]
  provides: [deno-ci-test, jsr-publish-pipeline]
  affects: [.github/workflows/ci.yml, .github/workflows/publish.yml]
tech_stack:
  added: [denoland/setup-deno@v2, jsr-oidc]
  removed: [actions/setup-node, npm-publish, native-lib-bundling]
  patterns: [oidc-publish, deno-ffi-test]
key_files:
  modified:
    - .github/workflows/ci.yml
    - .github/workflows/publish.yml
decisions:
  - Deno test job runs on ubuntu-latest only (multi-platform deferred to CI-04)
  - JSR publish uses npx jsr publish (not deno publish) for consistency with Phase 8
  - publish-jsr job needs validate gate (shared with PyPI)
  - No coverage collection for Deno tests (can be added later)
metrics:
  completed: 2026-04-18
  tasks_completed: 2
  tasks_total: 2
  files_created: 0
  files_modified: 2
---

# Phase 9 Plan 1: CI Pipeline Summary

Add Deno test job to ci.yml and replace npm publishing with JSR publishing in publish.yml.

## Completed Tasks

| Task | Name | Commit | Key Files |
|------|------|--------|-----------|
| 1 | Add Deno test job to ci.yml | 709a775 | .github/workflows/ci.yml |
| 2 | Replace npm publish with JSR publish in publish.yml | e3a19e8 | .github/workflows/publish.yml |

## Changes Made

### Task 1: Deno test job in ci.yml

Added `deno-test` job following existing binding test pattern (julia-coverage, dart-coverage, python-coverage):
- Install Deno 2.x via `denoland/setup-deno@v2`
- Build C++ libs (CMake Debug, TESTS=ON, C_API=ON)
- Run `deno task test` from `bindings/js/` with `LD_LIBRARY_PATH` set

### Task 2: JSR publish replacing npm in publish.yml

**Removed:**
- `build-native-npm` job (C++ native lib builds for npm bundling)
- `publish-npm` job (Node.js setup, package.json validation, npm install/build/publish)

**Added:**
- `publish-jsr` job with OIDC auth (`id-token: write`), version validation against `deno.json`, `npx jsr publish --allow-slow-types`

**Verified:** Zero occurrences of package.json, npm install, npm build, npm publish, setup-node remain in any workflow file.

## Deviations from Plan

None.

## Self-Check: PASSED

- .github/workflows/ci.yml contains deno-test job: YES
- .github/workflows/publish.yml contains publish-jsr job: YES
- npm references in workflow files: ZERO
