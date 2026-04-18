---
phase: 04-struct-marshaling-indirect-modules
reviewed: 2026-04-17T00:00:00Z
depth: standard
files_reviewed: 15
files_reviewed_list:
  - bindings/js/src/metadata.ts
  - bindings/js/src/introspection.ts
  - bindings/js/src/create.ts
  - bindings/js/src/read.ts
  - bindings/js/src/database.ts
  - bindings/js/src/transaction.ts
  - bindings/js/src/lua-runner.ts
  - bindings/js/src/composites.ts
  - bindings/js/src/errors.ts
  - bindings/js/src/index.ts
  - bindings/js/src/loader.ts
  - bindings/js/src/ffi-helpers.ts
  - bindings/js/src/csv.ts
  - bindings/js/src/query.ts
  - bindings/js/src/time-series.ts
findings:
  critical: 1
  warning: 4
  info: 2
  total: 7
status: issues_found
---

# Phase 04: Code Review Report

**Reviewed:** 2026-04-17T00:00:00Z
**Depth:** standard
**Files Reviewed:** 15
**Status:** issues_found

## Summary

Reviewed the Deno/TypeScript FFI binding layer for the Quiver library. The codebase is well-structured, using module augmentation to split the Database class across domain-specific files. Struct marshaling offsets for `quiver_scalar_metadata_t` (56 bytes), `quiver_group_metadata_t` (32 bytes), `quiver_database_options_t` (8 bytes), and `quiver_csv_options_t` (56 bytes) all match the x64 C struct layouts correctly.

The main concern is a systematic missing error-check pattern in six helper functions within `read.ts`, where FFI call return values are silently discarded instead of being passed through `check()`. This means C API errors from bulk vector/set reads would go undetected, potentially leading to reads from uninitialized out-parameter buffers.

## Critical Issues

### CR-01: Missing error checks on FFI calls in bulk read and by-ID helper functions

**File:** `bindings/js/src/read.ts:155`
**Issue:** Six helper functions (`readBulkIntegers`, `readBulkFloats`, `readBulkStrings`, `readByIdIntegers`, `readByIdFloats`, `readByIdStrings`) call C API functions through dynamic dispatch `(lib as Record<string, Function>)[fn](...)` but discard the return value. Every other FFI call in the binding layer wraps the result with `check()` to detect errors. Without `check()`, if the C function returns `QUIVER_ERROR`, the code proceeds to read from uninitialized out-parameter buffers (the `Uint8Array` allocations are zero-initialized, so `count` would read as 0 and the function would return `[]` silently -- masking the actual error). This affects all vector/set bulk reads and all vector/set by-ID reads (12 public methods total).

**Fix:**
```typescript
// read.ts line 155 -- readBulkIntegers (and same pattern at lines 176, 197, 238, 252, 266)
check((lib as Record<string, Function>)[fn](handle, collBuf.buf, attrBuf.buf, outVectors.ptr, outSizes.ptr, outCount.ptr));
```

Apply the same `check(...)` wrapper at:
- Line 155 (`readBulkIntegers`)
- Line 176 (`readBulkFloats`)
- Line 197 (`readBulkStrings`)
- Line 238 (`readByIdIntegers`)
- Line 252 (`readByIdFloats`)
- Line 266 (`readByIdStrings`)

## Warnings

### WR-01: Lossy bigint-to-number conversion for integer arrays in createElement

**File:** `bindings/js/src/create.ts:29`
**Issue:** When `setElementArray` receives a `bigint[]` array, it converts each element via `(values as bigint[]).map(Number)` before passing to `allocNativeInt64`. The `Number()` conversion from `bigint` loses precision for values outside the safe integer range (`Number.MAX_SAFE_INTEGER = 2^53 - 1`). Since the C API accepts `int64_t` (up to 2^63 - 1), values in the range (2^53, 2^63) will be silently truncated. The scalar path at line 69 correctly uses `BigInt(value)` for the opposite direction, but the array path discards the bigint precision.

**Fix:**
```typescript
if (typeof first === "bigint") {
  // Pass bigint values directly -- allocNativeInt64 should accept bigint[]
  // Or convert carefully:
  const arr = allocNativeInt64((values as bigint[]).map(v => {
    if (v > BigInt(Number.MAX_SAFE_INTEGER) || v < BigInt(-Number.MAX_SAFE_INTEGER)) {
      throw new QuiverError(`Integer value ${v} for '${name}' exceeds safe integer range`);
    }
    return Number(v);
  }));
  check(lib.quiver_element_set_array_integer(elemPtr, nameBuf.buf, arr.ptr, values.length));
  return;
}
```

### WR-02: Uninitialized array elements when valueColumnCount > 0 but valueColumnsPtr is null

**File:** `bindings/js/src/metadata.ts:52-58`
**Issue:** `readGroupMetadataAt` allocates `new Array(valueColumnCount)` at line 52 but only populates it inside the `if (valueColumnsPtr && valueColumnCount > 0)` guard at line 53. If the C API returns `valueColumnCount > 0` with a null `valueColumnsPtr` (an unlikely but possible malformed response), the returned `valueColumns` array would contain `undefined` elements instead of `ScalarMetadata` objects, violating the declared `ScalarMetadata[]` type.

**Fix:**
```typescript
const valueColumns: ScalarMetadata[] = [];
if (valueColumnsPtr && valueColumnCount > 0) {
  const colView = new Deno.UnsafePointerView(valueColumnsPtr);
  for (let i = 0; i < valueColumnCount; i++) {
    valueColumns.push(readScalarMetadataAt(colView, i * SCALAR_METADATA_SIZE));
  }
}
```

### WR-03: LuaRunner leaks native error string on run failure

**File:** `bindings/js/src/lua-runner.ts:27-33`
**Issue:** When `quiver_lua_runner_run` fails, the code calls `quiver_lua_runner_get_error` and reads the error string via `decodeStringFromBuf`. The `readPtrOut(outError)` returns a pointer to a C-allocated string, but that string is never freed. Unlike `quiver_get_last_error` (which returns thread-local storage that does not need freeing), `quiver_lua_runner_get_error` likely allocates a new string. If the Lua runner follows the same pattern as other C API functions that return strings via out-parameters, this is a memory leak on every Lua script error.

**Fix:** Verify the C API contract for `quiver_lua_runner_get_error`. If it allocates a new string, free it after reading:
```typescript
if (getErr === 0 && readPtrOut(outError)) {
  const msg = decodeStringFromBuf(outError);
  const errorPtr = readPtrOut(outError);
  if (errorPtr) lib.quiver_database_free_string(errorPtr);
  if (msg) throw new QuiverError(msg);
}
```

### WR-04: composites.ts throws plain Error instead of QuiverError

**File:** `bindings/js/src/composites.ts:32`
**Issue:** The `readScalarsById`, `readVectorsById`, and `readSetsById` functions throw `new Error(...)` on unknown data types (lines 32, 60, 90) instead of `new QuiverError(...)`. Every other error path in the binding layer uses `QuiverError`. This inconsistency means callers catching `QuiverError` would miss these errors.

**Fix:**
```typescript
// composites.ts -- import QuiverError at top:
import { QuiverError } from "./errors.ts";

// Then at lines 32, 60, 90:
throw new QuiverError(`Unknown data type ${attr.dataType} for attribute '${attr.name}'`);
```

## Info

### IN-01: Unused import of QuiverError in create.ts

**File:** `bindings/js/src/create.ts:2`
**Issue:** `QuiverError` is imported from `./errors.ts` at line 2, which is correct since it is used at lines 51 and 87. However, it is worth noting that `check` is also imported from `./errors.ts` on the same line. Both are actually used, so this is informational only -- the imports are correct.

_Retracted: on re-inspection both imports are used. No issue._

### IN-02: Magic numbers for data types across multiple files

**File:** `bindings/js/src/composites.ts:21`
**Issue:** Data type constants (0 = INTEGER, 1 = FLOAT, 2 = STRING, 3 = DATE_TIME, 4 = NULL) appear as magic numbers in `composites.ts` (lines 21, 23, 28-29, 49, 51, 56-57, 79, 81, 86-87), `time-series.ts` (lines 36-39), and `query.ts` (lines 17-18). The `time-series.ts` and `query.ts` files define local constants (`DATA_TYPE_INTEGER`, etc.) but `composites.ts` uses raw numeric literals with comments.

**Fix:** Extract the data type constants into a shared location (e.g., `types.ts` or a new `constants.ts`) and import them in all three files:
```typescript
// In types.ts or constants.ts:
export const DATA_TYPE_INTEGER = 0;
export const DATA_TYPE_FLOAT = 1;
export const DATA_TYPE_STRING = 2;
export const DATA_TYPE_DATE_TIME = 3;
export const DATA_TYPE_NULL = 4;
```

---

_Reviewed: 2026-04-17T00:00:00Z_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
