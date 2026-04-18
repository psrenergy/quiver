---
phase: 10-native-library-build-bundle
plan: 01
subsystem: .github/workflows, bindings/js
tags: [ci, native-libs, jsr, ffi]
dependency_graph:
  requires: [jsr-publish-pipeline]
  provides: [native-lib-bundling]
  affects: [.github/workflows/publish.yml, bindings/js/deno.json]
tech_stack:
  added: [patchelf, cmake-release-build]
  patterns: [ci-artifact-staging, native-binary-bundling]
key_files:
  modified:
    - .github/workflows/publish.yml
    - bindings/js/deno.json
decisions:
  - Platform keys use Deno.build constants (linux-x86_64, windows-x86_64) not npm convention (linux-x64, win32-x64)
  - build-native runs in parallel with Python wheel builds (no needs dependency)
  - RPATH patching for Linux so libquiver_c.so finds libquiver.so in same directory
metrics:
  completed: 2026-04-18
  tasks_completed: 2
  tasks_total: 2
  files_created: 0
  files_modified: 2
---

# Phase 10 Plan 1: Native Library Build & Bundle Summary

Add build-native CI job and wire publish-jsr to bundle pre-built native libraries in the JSR package.

## Completed Tasks

| Task | Name | Commit | Key Files |
|------|------|--------|-----------|
| 1 | Add build-native matrix job to publish.yml | 63cf211 | .github/workflows/publish.yml |
| 2 | Wire publish-jsr to download artifacts + update deno.json | 63cf211 | .github/workflows/publish.yml, bindings/js/deno.json |

## Changes Made

### Task 1: build-native job

Added matrix job building C++ in Release mode:
- **ubuntu-latest**: builds libquiver.so + libquiver_c.so, patches RPATH with patchelf, uploads as `native-linux-x86_64`
- **windows-latest**: builds libquiver.dll + libquiver_c.dll, uploads as `native-windows-x86_64`
- CMake flags: `-DCMAKE_BUILD_TYPE=Release -DQUIVER_BUILD_C_API=ON -DQUIVER_BUILD_TESTS=OFF`

### Task 2: publish-jsr wiring + deno.json

- `publish-jsr` now `needs: [validate, build-native]`
- Downloads `native-linux-x86_64` into `bindings/js/libs/linux-x86_64/`
- Downloads `native-windows-x86_64` into `bindings/js/libs/windows-x86_64/`
- Verification step lists downloaded files before publishing
- `deno.json` `publish.include` now contains `libs/**/*`

### Critical Discovery: Platform Key Naming

`Deno.build.arch` returns `"x86_64"` (not `"x64"`). The loader's `getBundledLibDir()` produces `linux-x86_64` and `windows-x86_64`. CI artifact directory names match these exactly. No loader changes needed.

## Deviations from Plan

None.

## Self-Check: PASSED

- .github/workflows/publish.yml contains build-native job: YES
- publish-jsr needs includes build-native: YES
- Artifact names match Deno platform keys: YES
- deno.json publish.include contains libs/**/*: YES
- loader.ts unchanged (Tier 1 already works): YES
