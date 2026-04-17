---
phase: 01-ffi-foundation-library-loading
plan: 01
subsystem: bindings/js
tags: [ffi, deno, dlopen, loader, migration]
dependency_graph:
  requires: []
  provides: [deno-ffi-loader, native-pointer-type, symbol-definitions]
  affects: [errors.ts, ffi-helpers.ts, database.ts, all-js-binding-modules]
tech_stack:
  added: [Deno.dlopen, jsr:@std/path, Deno.statSync, import.meta.dirname]
  patterns: [domain-grouped-symbol-definitions, struct-return-via-uint8array, singleton-library-cache]
key_files:
  created: [bindings/js/deno.lock]
  modified: [bindings/js/src/loader.ts, bindings/js/src/errors.ts]
decisions:
  - Used "buffer" for string inputs and "pointer" for opaque handles and string returns
  - Grouped 85 symbols into 11 domain objects matching C API source file organization
  - Kept singleton _lib/_coreLib caching pattern from original koffi loader
  - Fixed errors.ts import path as Rule 3 deviation to unblock smoke test
metrics:
  duration: 6 minutes
  completed: 2026-04-17
  tasks_completed: 2
  tasks_total: 2
  files_modified: 3
---

# Phase 01 Plan 01: FFI Foundation & Library Loading Summary

Deno.dlopen-based loader with 85 domain-grouped symbol definitions replacing koffi FFI, verified with struct-return and pointer-return smoke tests.

## Changes Made

### Task 1: Rewrite loader.ts with Deno.dlopen and all 85 symbol definitions

Complete rewrite of `bindings/js/src/loader.ts` (229 lines koffi-based -> 202 lines Deno-native).

**Removed:**
- `import koffi from "koffi"` and all `koffi.load()`, `lib.func()`, `koffi.struct()` calls
- `import { existsSync } from "node:fs"`
- `import { dirname, join } from "node:path"`
- `import { fileURLToPath } from "node:url"`
- `DatabaseOptionsType` koffi struct definition
- `bindSymbols()` function
- `FFIFunction` type export
- Old `Symbols` type (Record-based)

**Added:**
- `import { join, dirname } from "jsr:@std/path"` for path operations
- 8 type shorthand constants (P, BUF, I32, I64, USIZE, F64, V) for readable symbol definitions
- Platform constants using `Deno.build.os` / `Deno.build.arch`
- `export type NativePointer = Deno.PointerValue` replacing `type NativePointer = unknown`
- 11 domain-grouped symbol definition objects (`lifecycleSymbols`, `elementSymbols`, `crudSymbols`, `readSymbols`, `querySymbols`, `transactionSymbols`, `metadataSymbols`, `timeSeriesSymbols`, `csvSymbols`, `freeSymbols`, `luaSymbols`) with `as const`
- Combined `allSymbols` spread object for `Deno.dlopen`
- `fileExists()` using `Deno.statSync` replacing `existsSync`
- `__dirname` via `import.meta.dirname!` replacing `fileURLToPath(import.meta.url)`
- `openLibrary()` helper with Windows DLL pre-loading via `Deno.dlopen(corePath, {})`
- Properly typed `DenoLib` = `Deno.DynamicLibrary<typeof allSymbols>`
- `export type Symbols = ReturnType<typeof getSymbols>` for downstream type inference

**Preserved:**
- 3-tier library search (bundled libs/, dev walk-up 5 levels, system PATH)
- Windows DLL pre-loading pattern (load libquiver.dll before libquiver_c.dll)
- Singleton caching via `_lib` / `_coreLib`
- Error message format using `QuiverError`
- Public API: `loadLibrary()`, `getSymbols()`, `NativePointer` exports

### Task 2: Verify library loads, symbol count matches, and struct return works

Ran smoke test verifying:
- Library loads via `Deno.dlopen` with all 85 symbols accessible
- `quiver_version()` returns valid pointer, read as "0.6.0" via `Deno.UnsafePointerView.getCString()`
- `quiver_database_options_default()` returns 8-byte `Uint8Array` with correct struct layout (read_only=0, console_level=1)
- Struct-return code path (RESEARCH.md Pitfall 6) works correctly with `{ struct: [I32, I32] }` descriptor

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Fixed errors.ts import path for Deno module resolution**
- **Found during:** Task 2 (smoke test would not run)
- **Issue:** `errors.ts` imported from `"./loader.js"` which does not resolve in Deno (requires `.ts` extension)
- **Fix:** Changed import to `"./loader.ts"` in `errors.ts`
- **Files modified:** `bindings/js/src/errors.ts`
- **Commit:** 88360b6

**2. [Rule 3 - Blocking] Added deno.lock for jsr:@std/path dependency**
- **Found during:** Task 2 (deno check generated lock file)
- **Issue:** `deno check` created `bindings/js/deno.lock` for the new `jsr:@std/path` import
- **Fix:** Committed the lock file as an intentional artifact
- **Files modified:** `bindings/js/deno.lock` (new file)
- **Commit:** 88360b6

## Verification Results

| Check | Result |
|-------|--------|
| Zero koffi imports in loader.ts | PASS |
| Zero node: imports in loader.ts | PASS |
| 85 symbols defined | PASS |
| Deno.dlopen loads native library | PASS |
| quiver_version() returns valid string | PASS ("0.6.0") |
| quiver_database_options_default() returns 8-byte struct | PASS (read_only=0, console_level=1) |
| NativePointer exported as Deno.PointerValue | PASS |
| deno check on loader.ts itself | PASS (no errors in loader.ts) |
| Downstream errors.ts type error | Expected (Phase 2 scope -- quiver_get_last_error returns pointer, not string) |

## Known Type Errors in Downstream Files

The following type error exists in `errors.ts` and will be resolved in Phase 2:
- `errors.ts:15` -- `lib.quiver_get_last_error()` now returns `Deno.PointerValue` instead of `string`. The `check()` function passes this pointer directly to `new QuiverError(detail)`. Fix: use `Deno.UnsafePointerView.getCString()` to decode the pointer (Phase 2: ffi-helpers.ts rewrite).

## Self-Check: PASSED

- [x] bindings/js/src/loader.ts exists
- [x] bindings/js/src/errors.ts exists
- [x] bindings/js/deno.lock exists
- [x] .planning/phases/01-ffi-foundation-library-loading/01-01-SUMMARY.md exists
- [x] Commit c52d623 found (Task 1: loader rewrite)
- [x] Commit 88360b6 found (Task 2: errors.ts fix + verification)
