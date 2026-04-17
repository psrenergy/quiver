---
phase: 01-ffi-foundation-library-loading
reviewed: 2026-04-17T00:00:00Z
depth: standard
files_reviewed: 2
files_reviewed_list:
  - bindings/js/src/loader.ts
  - bindings/js/src/errors.ts
findings:
  critical: 1
  warning: 1
  info: 1
  total: 3
status: issues_found
---

# Phase 1: Code Review Report

**Reviewed:** 2026-04-17
**Depth:** standard
**Files Reviewed:** 2
**Status:** issues_found

## Summary

Reviewed the Deno FFI foundation layer: `loader.ts` (library discovery, symbol definitions, and native library loading) and `errors.ts` (error class and error-checking helper). The symbol declarations are comprehensive and well-organized by C API domain. Library search follows a sensible three-tier strategy (bundled, dev-mode walk, system PATH). One critical bug exists in the error handling path where the C string pointer returned by `quiver_get_last_error` is used without decoding, producing garbage error messages instead of the actual C API error text. One warning for a crash-prone non-null assertion on `import.meta.dirname`.

## Critical Issues

### CR-01: quiver_get_last_error pointer not decoded to string

**File:** `bindings/js/src/errors.ts:13-15`
**Issue:** `lib.quiver_get_last_error()` returns a `Deno.PointerValue` (typed as `bigint | null` in Deno FFI) because the symbol is declared with `result: "pointer"`. The `check` function passes this raw pointer value directly to the `QuiverError` constructor, which expects a `string`. JavaScript will coerce the bigint to a string like `"140234567890"` -- the memory address, not the error message. Every error thrown through `check()` will contain an opaque memory address instead of the actual error text from the C API (e.g., `"Cannot create_element: ..."`). This defeats the project principle that bindings surface error messages from the C/C++ layer.
**Fix:**
```typescript
export function check(err: number): void {
  if (err !== 0) {
    const lib = getSymbols();
    const ptr = lib.quiver_get_last_error();
    if (ptr) {
      const detail = new Deno.UnsafePointerView(ptr).getCString();
      throw new QuiverError(detail);
    }
    throw new QuiverError("Unknown error");
  }
}
```

## Warnings

### WR-01: Non-null assertion on import.meta.dirname crashes on remote imports

**File:** `bindings/js/src/loader.ts:172`
**Issue:** `import.meta.dirname` is `string | undefined` in Deno. It is `undefined` when the module is loaded via a remote URL (e.g., `https://deno.land/x/...`). The non-null assertion `!` causes a silent `undefined` to flow through to `join()` calls in `getBundledLibDir()` and `getSearchPaths()`, producing malformed paths and confusing "cannot load library" errors rather than a clear diagnostic. Since this is a module-level constant, it evaluates on first import -- there is no opportunity for the caller to catch or recover.
**Fix:**
```typescript
const __dirname = import.meta.dirname;
if (!__dirname) {
  throw new QuiverError(
    "Cannot determine module directory. The quiver JS binding must be loaded from a local file path, not a remote URL."
  );
}
```

## Info

### IN-01: Circular import between errors.ts and loader.ts

**File:** `bindings/js/src/errors.ts:1` and `bindings/js/src/loader.ts:2`
**Issue:** `errors.ts` imports `getSymbols` from `loader.ts`, and `loader.ts` imports `QuiverError` from `errors.ts`. This circular dependency is safe at runtime because both accesses are deferred inside function bodies (not evaluated at module initialization), but it is a design smell. If either module later adds a top-level usage of the imported symbol, the circular import will break with `undefined` at module evaluation time.
**Fix:** Consider inlining the `QuiverError` class directly in `loader.ts` (it is 5 lines), or extracting `QuiverError` to a standalone `quiver-error.ts` module with no imports, breaking the cycle.

---

_Reviewed: 2026-04-17_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
