---
phase: 02-pointer-string-marshaling
plan: 01
subsystem: ffi
tags: [deno, ffi, unsafe-pointer, dataview, text-encoder, marshaling, uint8array]

# Dependency graph
requires:
  - phase: 01-ffi-foundation-library-loading
    provides: "Deno.dlopen loader with NativePointer type and symbol definitions"
provides:
  - "Allocation type for GC-safe native memory references"
  - "13 rewritten marshaling functions using Deno FFI primitives"
  - "Pointer decoding fix in errors.ts using UnsafePointerView.getCString()"
  - "5 array decoder stubs exported for Phase 3 compatibility"
affects: [03-domain-module-migration, 04-domain-module-migration, 06-testing]

# Tech tracking
tech-stack:
  added: []
  patterns: ["Allocation { ptr, buf } return type for all native memory allocations", "DataView with little-endian for all typed value writes", "TextEncoder for null-terminated C string encoding", "UnsafePointerView.getCString() for C string decoding"]

key-files:
  created: []
  modified: ["bindings/js/src/types.ts", "bindings/js/src/errors.ts", "bindings/js/src/ffi-helpers.ts"]

key-decisions:
  - "All alloc functions return { ptr, buf } Allocation for GC safety (D-03)"
  - "Array decoder functions kept as throwing stubs to maintain export compatibility for Phase 3"
  - "nativeAddress kept as thin wrapper around Deno.UnsafePointer.value() for API uniformity"

patterns-established:
  - "Allocation pattern: every function that allocates native memory returns { ptr: Deno.PointerValue, buf: Uint8Array }"
  - "Little-endian convention: all DataView get/set calls pass true for littleEndian parameter"
  - "Null-terminated strings: encoder.encode(str + '\\0') for all C string encoding"
  - "Null pointer check: readPtrOut checks for 0n before Deno.UnsafePointer.create()"

requirements-completed: [MARSH-01, MARSH-03, MARSH-04]

# Metrics
duration: 2min
completed: 2026-04-17
---

# Phase 2 Plan 1: Pointer & String Marshaling Summary

**Rewrote all 13 ffi-helpers.ts marshaling functions from koffi to Deno FFI primitives (UnsafePointer, DataView, TextEncoder) with Allocation return type for GC safety**

## Performance

- **Duration:** 2 min
- **Started:** 2026-04-17T22:12:33Z
- **Completed:** 2026-04-17T22:14:56Z
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments
- Added Allocation type to types.ts providing GC-safe native memory reference pattern used by all allocation functions
- Fixed errors.ts pointer decoding bug where quiver_get_last_error() pointer was passed directly as string instead of being decoded via UnsafePointerView.getCString()
- Rewrote all 13 in-scope ffi-helpers.ts functions replacing koffi.alloc/encode/decode with Uint8Array, DataView, UnsafePointer, and TextEncoder
- Maintained 5 array decoder function exports as stubs for Phase 3 compatibility (18 total exported functions)
- Zero koffi imports, zero Buffer references, zero node: imports remaining in all three files

## Task Commits

Each task was committed atomically:

1. **Task 1: Add Allocation type to types.ts and fix errors.ts pointer decoding** - `f8d7ffd` (feat)
2. **Task 2: Rewrite all 13 ffi-helpers.ts functions from koffi to Deno FFI** - `b0efee6` (feat)

## Files Created/Modified
- `bindings/js/src/types.ts` - Added Allocation type definition for GC-safe { ptr, buf } native memory references
- `bindings/js/src/errors.ts` - Fixed check() to decode quiver_get_last_error() pointer via UnsafePointerView.getCString()
- `bindings/js/src/ffi-helpers.ts` - Complete rewrite of 13 marshaling functions from koffi to Deno FFI primitives

## Decisions Made
- All allocation functions return `{ ptr, buf }` Allocation type per D-03, ensuring callers hold buffer references to prevent GC from collecting buffers while native code holds pointers
- Array decoder functions (decodeInt64Array, decodeFloat64Array, decodeUint64Array, decodePtrArray, decodeStringArray) kept as throwing stubs rather than removed, maintaining export compatibility so domain modules that import them do not get missing-export errors until Phase 3 rewrites them
- nativeAddress retained as convenience wrapper around Deno.UnsafePointer.value() for API uniformity across domain modules

## Deviations from Plan

None - plan executed exactly as written.

## Known Stubs

| File | Line | Stub | Reason |
|------|------|------|--------|
| bindings/js/src/ffi-helpers.ts | 54 | decodeInt64Array throws "Not implemented: Phase 3 scope" | Phase 3 scope per plan |
| bindings/js/src/ffi-helpers.ts | 59 | decodeFloat64Array throws "Not implemented: Phase 3 scope" | Phase 3 scope per plan |
| bindings/js/src/ffi-helpers.ts | 64 | decodeUint64Array throws "Not implemented: Phase 3 scope" | Phase 3 scope per plan |
| bindings/js/src/ffi-helpers.ts | 69 | decodePtrArray throws "Not implemented: Phase 3 scope" | Phase 3 scope per plan |
| bindings/js/src/ffi-helpers.ts | 74 | decodeStringArray throws "Not implemented: Phase 3 scope" | Phase 3 scope per plan |

These stubs are intentional -- the plan explicitly specifies these 5 array decoder functions as Phase 3 scope. They remain exported to prevent import errors in domain modules. The plan's goal (rewriting the 13 in-scope marshaling functions) is fully achieved.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- All marshaling primitives are ready for domain module migration in Phase 3
- Domain modules (database.ts, query.ts, read.ts, etc.) will have type errors until Phase 3 updates their call sites to use the new Allocation return type
- The 5 array decoder stubs must be implemented in Phase 3 before domain modules can decode native array data

## Self-Check: PASSED

All files exist. All commits verified.

---
*Phase: 02-pointer-string-marshaling*
*Completed: 2026-04-17*
