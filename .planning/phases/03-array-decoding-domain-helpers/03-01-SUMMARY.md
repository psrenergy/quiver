---
phase: 03-array-decoding-domain-helpers
plan: 01
subsystem: js-bindings
tags: [deno-ffi, array-decoding, koffi-removal, marshaling]
dependency_graph:
  requires: [02-01]
  provides: [array-decoders, query-deno-ffi, csv-deno-ffi, time-series-deno-ffi]
  affects: [bindings/js/src/ffi-helpers.ts, bindings/js/src/query.ts, bindings/js/src/csv.ts, bindings/js/src/time-series.ts, bindings/js/src/types.ts]
tech_stack:
  added: []
  patterns: [UnsafePointerView-array-decoding, DataView-struct-building, Allocation-keepalive]
key_files:
  created: []
  modified:
    - bindings/js/src/ffi-helpers.ts
    - bindings/js/src/query.ts
    - bindings/js/src/csv.ts
    - bindings/js/src/time-series.ts
    - bindings/js/src/types.ts
decisions:
  - "Narrowed Allocation.buf type from Uint8Array to Uint8Array<ArrayBuffer> for Deno FFI buffer parameter compatibility"
  - "Converted typed array out-params (Int32Array, BigInt64Array, Float64Array) to Uint8Array+DataView pattern for Deno FFI pointer parameter compatibility"
  - "Converted string FFI arguments to toCString().buf for Deno FFI buffer parameters"
  - "Added BigInt() wrapping for i64 and usize FFI parameters (id, count arguments)"
metrics:
  duration: 638s
  completed: "2026-04-17T23:27:14Z"
  tasks_completed: 3
  tasks_total: 3
  files_modified: 5
---

# Phase 3 Plan 01: Array Decoding & Domain Helpers Summary

Implement 5 array decoder functions using Deno UnsafePointerView and remove all koffi usage from query.ts, csv.ts, and time-series.ts

## One-liner

Five UnsafePointerView array decoders (int64/float64/uint64/ptr/string) plus full koffi removal from query, csv, and time-series modules using Uint8Array+DataView marshaling

## Tasks Completed

| Task | Name | Commit | Key Changes |
|------|------|--------|-------------|
| 1 | Implement 5 array decoder functions in ffi-helpers.ts | 7778f91 | Replace Phase 2 stubs with UnsafePointerView implementations |
| 2 | Remove koffi from query.ts and csv.ts | faec3ae | Allocation types, Uint8Array+DataView struct building, toCString encoding |
| 3 | Remove koffi from time-series.ts | 41905e7 | UnsafePointerView column type/path decoding, Allocation keepalive |

## Implementation Details

### Array Decoders (ffi-helpers.ts)

- **decodeInt64Array**: `UnsafePointerView.getArrayBuffer(count * 8)` + `BigInt64Array` + `Array.from(i64, v => Number(v))`
- **decodeFloat64Array**: `UnsafePointerView.getArrayBuffer(count * 8)` + `Float64Array` + `Array.from(f64)`
- **decodeUint64Array**: `UnsafePointerView.getArrayBuffer(count * 8)` + `BigUint64Array` + `Array.from(u64, v => Number(v))`
- **decodePtrArray**: `UnsafePointerView.getPointer(i * 8)` loop returning `Deno.PointerValue[]`
- **decodeStringArray**: `getPointer(i * 8)` + `getCString()` with null -> `""` fallback
- All decoders guard with `if (!ptr || count === 0) return []`
- Data copied via `Array.from()` before native memory is freed

### query.ts Migration

- Removed koffi import (was unused)
- `marshalParams` returns `Allocation` objects for types/values buffers (Uint8Array + DataView)
- Keepalive array changed from `NativePointer[]` to `Allocation[]`
- `nativeAddress(native)` changed to `nativeAddress(native.ptr)` (3 occurrences)
- SQL strings encoded via `toCString(sql).buf` for Deno FFI buffer params
- Out-params converted to `Uint8Array` + `DataView` for pointer parameter compatibility
- `params.length` wrapped in `BigInt()` for usize FFI params

### csv.ts Migration

- Removed koffi import
- `Buffer.alloc(56)` replaced with `new Uint8Array(56)` + `new DataView(buf.buffer)`
- `buf.writeBigUInt64LE()` replaced with `dv.setBigUint64(offset, value, true)` (8 occurrences)
- `koffi.alloc`/`koffi.encode` for entry counts replaced with `Uint8Array` + `DataView` loop
- All struct fields and entry counts wrapped in `Allocation` objects with `.ptr`
- `buildCsvOptionsBuffer` returns `[Allocation, Allocation[]]` instead of `[Buffer, NativePointer[]]`
- Export/import calls pass `optsBuf.ptr` and encode strings via `toCString()`

### time-series.ts Migration

- Removed koffi import
- `koffi.decode(typesPtr, koffi.array("int", colCount))` replaced with `UnsafePointerView.getArrayBuffer(colCount * 4)` + `Int32Array`
- `koffi.decode(pathsPtr, koffi.array("str", count))` replaced with `getPointer(i * 8)` + `getCString()` with `null` preservation
- Keepalive arrays changed from `NativePointer[]` to `Allocation[]` (2 occurrences)
- `dataPtrs` changed from `NativePointer[]` to `Deno.PointerValue[]`, push `.ptr` for Allocation objects
- `namesTable.ptr`, `dataTable.ptr`, `colsTable.ptr`, `pathsTable.ptr` used in FFI calls
- `typesArr` (Int32Array) replaced with `Uint8Array` + `DataView` + `Allocation` wrapping
- `id` arguments wrapped in `BigInt()` for i64 FFI params
- String arguments encoded via `toCString()` for buffer params

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Allocation.buf type too loose for Deno FFI buffer params**
- **Found during:** Task 2
- **Issue:** `Allocation.buf: Uint8Array` resolved to `Uint8Array<ArrayBufferLike>` which is not assignable to Deno FFI's `BufferSource` (expects `Uint8Array<ArrayBuffer>`)
- **Fix:** Narrowed type in types.ts to `buf: Uint8Array<ArrayBuffer>`
- **Files modified:** bindings/js/src/types.ts
- **Commit:** faec3ae

**2. [Rule 1 - Bug] toCString produces mistyped Uint8Array from TextEncoder**
- **Found during:** Task 2
- **Issue:** `TextEncoder.encode()` returns `Uint8Array` with `ArrayBufferLike` generic, not compatible with narrowed `Allocation.buf` type
- **Fix:** Copy encoded bytes into `new Uint8Array(encoded.length)` which creates `Uint8Array<ArrayBuffer>`
- **Files modified:** bindings/js/src/ffi-helpers.ts
- **Commit:** faec3ae

**3. [Rule 3 - Blocking] FFI calls pass typed arrays to pointer parameters**
- **Found during:** Task 2, Task 3
- **Issue:** Deno FFI `"pointer"` params expect `Deno.PointerValue`, not typed arrays. Out-params (`Int32Array`, `BigInt64Array`, `Float64Array`) and input arrays must be converted
- **Fix:** Replaced typed array out-params with `Uint8Array` + `DataView` + `Deno.UnsafePointer.of()` pattern; marshal input arrays into `Allocation` objects
- **Files modified:** bindings/js/src/query.ts, bindings/js/src/time-series.ts
- **Commits:** faec3ae, 41905e7

**4. [Rule 3 - Blocking] String arguments incompatible with Deno FFI buffer params**
- **Found during:** Task 2, Task 3
- **Issue:** Deno FFI `"buffer"` params expect `Uint8Array`, not `string`. Plain string arguments to FFI calls fail type check
- **Fix:** Encode all string FFI arguments via `toCString(str).buf`
- **Files modified:** bindings/js/src/query.ts, bindings/js/src/csv.ts, bindings/js/src/time-series.ts
- **Commits:** faec3ae, 41905e7

**5. [Rule 3 - Blocking] Numeric arguments need BigInt for i64/usize FFI params**
- **Found during:** Task 2, Task 3
- **Issue:** Deno FFI `"i64"` and `"usize"` params expect `bigint`, not `number`
- **Fix:** Wrap `id`, `params.length`, `count` in `BigInt()` at FFI call sites
- **Files modified:** bindings/js/src/query.ts, bindings/js/src/time-series.ts
- **Commits:** faec3ae, 41905e7

## Known Stubs

None. All Phase 2 stubs in ffi-helpers.ts have been replaced with real implementations.

## Pre-existing Issues (Not Fixed)

database.ts still uses `.js` import extensions (not `.ts`), causing `TS2307` errors when type-checking files that import from database.ts. This is a pre-existing issue from Phase 1/2 migration and is out of scope for this plan. All 3 files modified by this plan (query.ts, csv.ts, time-series.ts) have zero errors of their own; the 3 errors reported by `deno check` are from the transitive database.ts dependency.

## Verification Results

| Check | Result |
|-------|--------|
| `deno check ffi-helpers.ts` | PASS (0 errors) |
| `deno check query.ts` | 3 upstream errors only (database.ts .js imports) |
| `deno check csv.ts` | 3 upstream errors only (database.ts .js imports) |
| `deno check time-series.ts` | 3 upstream errors only (database.ts .js imports) |
| Zero koffi in 4 files | PASS |
| Zero Buffer in csv.ts | PASS |
| Zero NativePointer in query/csv/time-series | PASS |
| Zero "Not implemented" stubs | PASS |

## Self-Check: PASSED

All 5 modified files exist on disk. All 3 task commits verified in git log.
