---
phase: 03-array-decoding-domain-helpers
reviewed: 2026-04-17T00:00:00Z
depth: standard
files_reviewed: 5
files_reviewed_list:
  - bindings/js/src/ffi-helpers.ts
  - bindings/js/src/query.ts
  - bindings/js/src/csv.ts
  - bindings/js/src/time-series.ts
  - bindings/js/src/types.ts
findings:
  critical: 0
  warning: 3
  info: 2
  total: 5
status: issues_found
---

# Phase 03: Code Review Report

**Reviewed:** 2026-04-17
**Depth:** standard
**Files Reviewed:** 5
**Status:** issues_found

## Summary

Reviewed the Deno FFI binding layer for Quiver after Phase 03 migration from koffi to native Deno UnsafePointerView. The migration is well-executed overall: array decoders in `ffi-helpers.ts` correctly use `getArrayBuffer` for zero-copy typed array access, the `Allocation` type pattern properly prevents GC of backing buffers, and the time-series columnar marshaling faithfully mirrors the C API's parallel-array contract.

Three warnings were found: one GC-safety concern in `csv.ts` where `toCString()` temporaries are not anchored to local variables, one unhandled type branch in `marshalParams` that silently drops unexpected parameter types, and one import extension inconsistency across the migrated files. Two informational items note precision loss in int64 decoding and a redundant buffer copy in `toCString`.

## Warnings

### WR-01: Inline toCString() temporaries not anchored in csv.ts

**File:** `bindings/js/src/csv.ts:95`
**Issue:** The `exportCsv` and `importCsv` methods call `toCString(collection).buf`, `toCString(group).buf`, and `toCString(filePath).buf` as inline expressions in the FFI call arguments. The returned `Allocation` objects (which own the `Uint8Array` backing buffers) are not stored in local variables. While Deno's synchronous FFI prevents V8 GC during the call (so the buffers survive in practice), this pattern is fragile and inconsistent with every other file in the binding layer (`query.ts`, `time-series.ts`, `read.ts`) where `toCString` results are always stored in named locals. A future refactor to async FFI (`nonblocking: true`) would turn this into a real use-after-free bug.
**Fix:**
```typescript
Database.prototype.exportCsv = function (
  this: Database,
  collection: string,
  group: string,
  filePath: string,
  options?: CsvOptions,
): void {
  const lib = getSymbols();
  const collBuf = toCString(collection);
  const grpBuf = toCString(group);
  const pathBuf = toCString(filePath);
  const [optsBuf, _keepalive] = buildCsvOptionsBuffer(options);
  check(lib.quiver_database_export_csv(this._handle, collBuf.buf, grpBuf.buf, pathBuf.buf, optsBuf.ptr));
};
```
Apply the same pattern to `importCsv` on line 107.

### WR-02: marshalParams silently ignores unexpected parameter types

**File:** `bindings/js/src/query.ts:28-51`
**Issue:** The `marshalParams` function handles `null`, `number`, and `string` parameter types, but the `for` loop has no `else` branch. If a caller passes a value of an unexpected type (e.g., `boolean`, `bigint`, or an object), the types and values arrays at that index remain zero-filled from the `Uint8Array` constructor. The zero in the types buffer corresponds to `DATA_TYPE_INTEGER` (0), and the zero in the values buffer is a null pointer -- causing the C API to dereference a null pointer to read an integer value, which is undefined behavior and will likely crash. The `QueryParam` type union (`number | string | null`) guards against this at compile time, but at runtime (e.g., from untyped JS callers or `any` casts) this is reachable.
**Fix:** Add a defensive else branch that throws:
```typescript
} else {
  throw new QuiverError(`Unsupported query parameter type at index ${i}: ${typeof p}`);
}
```

### WR-03: Import extension inconsistency (.ts vs .js) in migrated files

**File:** `bindings/js/src/csv.ts:1-5`, `bindings/js/src/query.ts:1-5`, `bindings/js/src/time-series.ts:1-21`, `bindings/js/src/ffi-helpers.ts:1`
**Issue:** The four migrated files use `.ts` extensions in their imports (`from "./database.ts"`, `from "./errors.ts"`, etc.), while all other source files in the binding layer (`database.ts`, `create.ts`, `read.ts`, `metadata.ts`, `composites.ts`, `transaction.ts`, `introspection.ts`, `lua-runner.ts`, `index.ts`) use `.js` extensions. The `tsconfig.json` sets `"module": "NodeNext"` and `"moduleResolution": "NodeNext"`, which requires `.js` extensions in imports for correct compiled output. The `.ts` extensions work in a pure-Deno runtime but will break the `tsc` build and the published npm package (`"main": "dist/index.js"`). Additionally, `errors.ts` and `loader.ts` (dependencies of the migrated files) also use `.ts` extensions in a circular import chain, creating a split where approximately half the codebase uses one convention and half uses the other.
**Fix:** Standardize all import extensions to `.js` to match the established project convention and tsconfig module resolution:
```typescript
// csv.ts, query.ts, time-series.ts, ffi-helpers.ts, errors.ts, loader.ts
import { Database } from "./database.js";
import { check } from "./errors.js";
import { ... } from "./ffi-helpers.js";
import { getSymbols } from "./loader.js";
import type { Allocation } from "./types.js";
```

## Info

### IN-01: int64-to-Number precision loss for values beyond Number.MAX_SAFE_INTEGER

**File:** `bindings/js/src/ffi-helpers.ts:58`
**Issue:** `decodeInt64Array` converts `BigInt64Array` elements to `Number` via `Number(v)`. JavaScript `Number` (IEEE 754 double) loses precision for integer values beyond +/-2^53. The same applies to `decodeUint64Array` (line 76) and `readUint64Out` (line 39). This is a standard trade-off that matches how other Quiver bindings handle integer values, and the database schemas likely do not store values of this magnitude. Noting for completeness; no action needed unless the API contract changes.
**Fix:** If needed in the future, provide parallel `BigInt`-returning variants (e.g., `decodeInt64ArrayBigInt`) for callers that need full 64-bit precision.

### IN-02: Redundant buffer copy in toCString

**File:** `bindings/js/src/ffi-helpers.ts:103-108`
**Issue:** `toCString` allocates `encoded` via `encoder.encode()`, then creates a second `Uint8Array` of the same length and copies into it with `buf.set(encoded)`. The `encoder.encode()` already returns a fresh `Uint8Array` that could be used directly. Compare with `allocNativeString` (line 145-148) which correctly uses the encoder result without copying.
**Fix:**
```typescript
export function toCString(str: string): Allocation {
  const buf = encoder.encode(str + "\0");
  return { ptr: Deno.UnsafePointer.of(buf)!, buf };
}
```
This eliminates one allocation and one copy per string conversion. This is the same implementation as `allocNativeString`, so consider whether both functions are needed or if one can be removed.

---

_Reviewed: 2026-04-17_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
