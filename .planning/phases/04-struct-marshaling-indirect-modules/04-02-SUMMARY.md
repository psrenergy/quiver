---
phase: 04-struct-marshaling-indirect-modules
plan: 02
status: complete
started: 2026-04-17T22:00:00Z
completed: 2026-04-17T22:45:00Z
---

## Summary

Fixed Deno FFI compatibility across all 15 remaining source files in `bindings/js/src/`. Converted typed array out-parameters (`Int32Array`, `BigInt64Array`, `Float64Array`) to the `Uint8Array+DataView+UnsafePointer.of()` pattern. Encoded all raw string FFI arguments via `toCString().buf`. Wrapped all `i64`/`usize` FFI arguments in `BigInt()`. Fixed all `Allocation` objects to pass `.ptr` for pointer params and `.buf` for buffer params. Replaced all `.js` import extensions with `.ts` across all 16 source files.

## Key Changes

### Task 1: FFI Compatibility in 6 Indirect Modules
- **introspection.ts**: Converted `Int32Array(1)` and `BigInt64Array(1)` out-params to `Uint8Array+DataView`; fixed `outPath.ptr`
- **create.ts**: Added `toCString` for all name/collection strings; `BigInt(id)` for update/delete; `Uint8Array(8)+DataView` for outId; `allocNativeInt64`/`allocNativeFloat64`/`allocNativeStringArray` for array data; fixed `outElem.ptr`
- **read.ts**: Added `toCString` for all collection/attribute strings; `BigInt(id)` for by-ID methods; `Uint8Array+DataView` for scalar by-ID out-params; `.ptr` for all Allocation args; `BigInt(count)` for USIZE free params
- **database.ts**: Added `toCString` for dbPath/schemaPath/migrationsPath; `.buf` for options struct; `.ptr` for outDb
- **transaction.ts**: Converted `Int32Array(1)` out-param in `inTransaction` to `Uint8Array(4)+DataView`
- **lua-runner.ts**: Added `toCString` for script arg; fixed `.ptr` for outRunner and outError

### Task 2: Import Extensions
- All 15 remaining files updated from `.js` to `.ts` import/export/declare-module extensions
- Affected: composites, errors, index, loader, ffi-helpers, csv, query, time-series, plus the 6 from Task 1

### Task 3: Verification
- `deno check bindings/js/src/index.ts` passes with zero errors (transitively checks all 16 files)
- Zero `koffi` references remain
- Zero `.js"` import extensions remain
- Zero `Buffer.` references remain
- Zero `new Int32Array(1)` or `new BigInt64Array(1)` out-param patterns remain

## Self-Check: PASSED

All acceptance criteria met. Full `deno check` clean.

## Key Files

### Created
(none)

### Modified
- `bindings/js/src/introspection.ts`
- `bindings/js/src/create.ts`
- `bindings/js/src/read.ts`
- `bindings/js/src/database.ts`
- `bindings/js/src/transaction.ts`
- `bindings/js/src/lua-runner.ts`
- `bindings/js/src/composites.ts`
- `bindings/js/src/errors.ts`
- `bindings/js/src/index.ts`
- `bindings/js/src/loader.ts`
- `bindings/js/src/ffi-helpers.ts`
- `bindings/js/src/csv.ts`
- `bindings/js/src/query.ts`
- `bindings/js/src/time-series.ts`
