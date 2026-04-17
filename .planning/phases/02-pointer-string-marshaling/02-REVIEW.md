---
phase: 02-pointer-string-marshaling
reviewed: 2026-04-17T19:30:00Z
depth: standard
files_reviewed: 3
files_reviewed_list:
  - bindings/js/src/types.ts
  - bindings/js/src/errors.ts
  - bindings/js/src/ffi-helpers.ts
findings:
  critical: 0
  warning: 2
  info: 2
  total: 4
status: issues_found
---

# Phase 02: Code Review Report

**Reviewed:** 2026-04-17T19:30:00Z
**Depth:** standard
**Files Reviewed:** 3
**Status:** issues_found

## Summary

Reviewed the Deno/JS FFI marshaling layer: type definitions (`types.ts`), error handling (`errors.ts`), and pointer/string helper functions (`ffi-helpers.ts`). The code is generally well-structured with clear ownership semantics via the `Allocation` type. Two warnings were found: a logic gap in error message retrieval that can produce empty error messages, and a type-safety issue in `allocNativeInt64` that will throw at runtime for non-integer inputs. Two info-level items were also found: an unused function and a precision note.

## Warnings

### WR-01: check() produces empty error message instead of "Unknown error"

**File:** `bindings/js/src/errors.ts:14-18`
**Issue:** `quiver_get_last_error()` returns `g_last_error.c_str()` in the C layer, which is always a non-null pointer -- even when the error string is empty (e.g., after `quiver_clear_last_error()` or before any error occurs). The Deno FFI returns this as a non-null `Deno.PointerValue`, so the `if (ptr)` guard on line 14 is always true. This means the "Unknown error" fallback on line 18 is unreachable dead code. More importantly, if `check()` is ever called when the last-error string is empty, it will throw `QuiverError("")` (an error with an empty message) instead of the intended `QuiverError("Unknown error")`.

In the current code path this is unlikely because the C API always calls `quiver_set_last_error(message)` before returning `QUIVER_ERROR`, but the guard's intent is to handle exactly the edge case where it does not.
**Fix:** Check the decoded string content instead of the pointer:
```typescript
export function check(err: number): void {
  if (err !== 0) {
    const lib = getSymbols();
    const ptr = lib.quiver_get_last_error();
    const detail = ptr ? new Deno.UnsafePointerView(ptr).getCString() : "";
    throw new QuiverError(detail || "Unknown error");
  }
}
```

### WR-02: allocNativeInt64 throws TypeError at runtime for non-integer number values

**File:** `bindings/js/src/ffi-helpers.ts:84-91`
**Issue:** The function signature accepts `number[]`, but `BigInt(value)` throws `TypeError` if `value` is not an integer (e.g., `3.14`, `NaN`, `Infinity`). Since the C API expects `int64_t` values, callers must pass integers, but nothing in the type system or runtime validates this before the `BigInt()` conversion on line 88 produces an unhandled `TypeError` with a confusing message like `"Cannot convert a non-integer to a BigInt"`.
**Fix:** Either narrow the input type to `bigint[]` (pushing conversion responsibility to callers), or truncate explicitly:
```typescript
export function allocNativeInt64(values: number[]): Allocation {
  const buf = new Uint8Array(values.length * 8);
  const dv = new DataView(buf.buffer);
  for (let i = 0; i < values.length; i++) {
    dv.setBigInt64(i * 8, BigInt(Math.trunc(values[i])), true);
  }
  return { ptr: Deno.UnsafePointer.of(buf)!, buf };
}
```
Or validate and throw a clear `QuiverError` if a non-integer is encountered.

## Info

### IN-01: toCString is unused dead code duplicating allocNativeString

**File:** `bindings/js/src/ffi-helpers.ts:78-81`
**Issue:** `toCString` and `allocNativeString` (lines 118-121) are functionally identical -- both encode `str + "\0"` via `TextEncoder` and return an `Allocation`. `toCString` is exported but never imported or called anywhere in the codebase. All callers use `allocNativeString` instead.
**Fix:** Delete the `toCString` function (lines 78-81).

### IN-02: readUint64Out silently truncates values beyond Number.MAX_SAFE_INTEGER

**File:** `bindings/js/src/ffi-helpers.ts:37-40`
**Issue:** The function converts a `BigUint64` to `number` via `Number()`, which silently loses precision for values exceeding 2^53 - 1. In practice this is used for `size_t` array counts which will be small, so this is not a runtime risk. However, the function name and return type (`number`) do not communicate the precision boundary.
**Fix:** No code change needed given current usage. If this function is ever used for non-count values, consider returning `bigint` or adding a range check.

---

_Reviewed: 2026-04-17T19:30:00Z_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
