---
phase: 03-array-decoding-domain-helpers
fixed_at: 2026-04-17T23:45:24Z
review_path: .planning/phases/03-array-decoding-domain-helpers/03-REVIEW.md
iteration: 1
findings_in_scope: 3
fixed: 3
skipped: 0
status: all_fixed
---

# Phase 03: Code Review Fix Report

**Fixed at:** 2026-04-17T23:45:24Z
**Source review:** .planning/phases/03-array-decoding-domain-helpers/03-REVIEW.md
**Iteration:** 1

**Summary:**
- Findings in scope: 3
- Fixed: 3
- Skipped: 0

## Fixed Issues

### WR-01: Inline toCString() temporaries not anchored in csv.ts

**Files modified:** `bindings/js/src/csv.ts`
**Commit:** d91838f
**Applied fix:** Stored `toCString()` results in named local variables (`collBuf`, `grpBuf`, `pathBuf`) in both `exportCsv` and `importCsv` methods, matching the pattern used consistently in all other binding files (`query.ts`, `time-series.ts`, `read.ts`). This prevents GC of the backing `Uint8Array` buffers and future-proofs against async FFI migration.

### WR-02: marshalParams silently ignores unexpected parameter types

**Files modified:** `bindings/js/src/query.ts`
**Commit:** a2402ad
**Applied fix:** Added a defensive `else` branch in the `marshalParams` for-loop that throws `QuiverError` with a descriptive message including the index and type of the unexpected parameter. Also added `QuiverError` to the existing import from `./errors.js`. This prevents silent null-pointer dereference in the C API when untyped JS callers pass unexpected value types.

### WR-03: Import extension inconsistency (.ts vs .js) in migrated files

**Files modified:** `bindings/js/src/ffi-helpers.ts`, `bindings/js/src/errors.ts`, `bindings/js/src/loader.ts`, `bindings/js/src/csv.ts`, `bindings/js/src/query.ts`, `bindings/js/src/time-series.ts`
**Commit:** 56a126f
**Applied fix:** Changed all `.ts` import extensions to `.js` across 6 files to match the established project convention and the `tsconfig.json` NodeNext module resolution setting. Updated both `import` statements and `declare module` specifiers. Verified with grep that no `.ts` import references remain in the `src/` directory.

---

_Fixed: 2026-04-17T23:45:24Z_
_Fixer: Claude (gsd-code-fixer)_
_Iteration: 1_
