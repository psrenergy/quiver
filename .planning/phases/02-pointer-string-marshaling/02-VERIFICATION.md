---
phase: 02-pointer-string-marshaling
verified: 2026-04-17T22:30:00Z
status: passed
score: 4/4 must-haves verified
overrides_applied: 0
deferred:
  - truth: "5 array decoder functions (decodeInt64Array, decodeFloat64Array, decodeUint64Array, decodePtrArray, decodeStringArray) implemented with real UnsafePointerView logic"
    addressed_in: "Phase 3"
    evidence: "Phase 3 success criteria 1: 'decodeInt64Array, decodeFloat64Array, decodeUint64Array, decodeStringArray, and decodePtrArray read native memory using Deno.UnsafePointerView instead of koffi.decode'"
---

# Phase 2: Pointer & String Marshaling Verification Report

**Phase Goal:** All pointer out-parameter, string marshaling, and native allocation helpers work using Deno FFI primitives
**Verified:** 2026-04-17T22:30:00Z
**Status:** PASSED
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | allocPtrOut/readPtrOut work using BigInt (Deno.UnsafePointer) instead of koffi Buffer/decode | VERIFIED | allocPtrOut returns 8-byte Uint8Array + UnsafePointer.of(). readPtrOut uses DataView.getBigUint64(0,true) + UnsafePointer.create(). Spot-check: readPtrOut on zero buffer returns null. |
| 2 | String encoding uses TextEncoder to produce null-terminated C strings and TextDecoder to read them back | VERIFIED | toCString: encoder.encode(str + "\0"). allocNativeString: same pattern. decodeStringFromBuf: UnsafePointerView.getCString(). errors.ts: getCString() for quiver_get_last_error(). Spot-check: toCString("hello").buf last byte === 0. |
| 3 | allocNativeInt64, allocNativeFloat64, allocNativeString, allocNativeStringArray, and allocNativePtrTable produce valid native memory using Deno FFI primitives (no koffi.alloc/encode) | VERIFIED | All 5 functions use Uint8Array + DataView with little-endian writes. Spot-checks: Int64 values round-trip via DataView; float64 1.5/2.5 round-trip; allocNativeStringArray produces 24-byte table with 3 null-terminated keepalives. |
| 4 | No koffi import remains in ffi-helpers.ts | VERIFIED | grep for koffi/Buffer./node: across all three phase files returns zero matches. |

**Score:** 4/4 truths verified

### Deferred Items

Items not yet met but explicitly addressed in later milestone phases.

| # | Item | Addressed In | Evidence |
|---|------|-------------|----------|
| 1 | 5 array decoder functions implemented with real UnsafePointerView logic | Phase 3 | Phase 3 success criteria 1: "decodeInt64Array, decodeFloat64Array, decodeUint64Array, decodeStringArray, and decodePtrArray read native memory using Deno.UnsafePointerView instead of koffi.decode" |

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `bindings/js/src/types.ts` | Allocation type definition and existing types preserved | VERIFIED | Contains `export type Allocation = { ptr: Deno.PointerValue; buf: Uint8Array; }`. All 5 pre-existing types (ScalarValue, ArrayValue, Value, ElementData, QueryParam) still present. 12 lines. |
| `bindings/js/src/errors.ts` | Error checking with pointer-to-string decoding via getCString | VERIFIED | check() uses `const ptr = lib.quiver_get_last_error()` then `new Deno.UnsafePointerView(ptr).getCString()`. Old broken `const detail = lib.quiver_get_last_error()` pattern removed. |
| `bindings/js/src/ffi-helpers.ts` | All 13 marshaling functions rewritten for Deno FFI; 5 stubs; 18 total exports | VERIFIED | 134 lines. 18 exported functions confirmed. Zero koffi imports. Zero Buffer references. Zero node: imports. All 8 alloc functions return { ptr, buf } (Allocation). |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `bindings/js/src/ffi-helpers.ts` | `bindings/js/src/types.ts` | `import type { Allocation }` | WIRED | Line 1: `import type { Allocation } from "./types.ts"` — Allocation type used as return type for all 8 allocation functions |
| `bindings/js/src/errors.ts` | `Deno.UnsafePointerView` | `getCString()` to decode quiver_get_last_error() pointer return | WIRED | errors.ts line 15: `new Deno.UnsafePointerView(ptr).getCString()` — decodes the const char* return value |
| `bindings/js/src/errors.ts` | `bindings/js/src/loader.ts` | `import { getSymbols }` | WIRED | errors.ts line 1: `import { getSymbols } from "./loader.ts"` — unchanged from Phase 1 (regression check passed) |

Note on plan key link 1 (ffi-helpers -> loader.ts): The plan describes this link as indirect via NativePointer type re-export. In practice, ffi-helpers.ts does not import from loader.ts — it uses `Deno.PointerValue` directly, which is semantically identical to the `NativePointer = Deno.PointerValue` alias in loader.ts. This is a valid deviation: the plan said "not used directly." Goal is fully met.

### Data-Flow Trace (Level 4)

ffi-helpers.ts is a marshaling utility module (no data rendering). Data flow verified through behavioral spot-checks:

| Behavior | Data Path | Status |
|----------|-----------|--------|
| allocPtrOut -> readPtrOut round-trip | Uint8Array(8) -> DataView.getBigUint64 -> UnsafePointer.create | VERIFIED (spot-check) |
| toCString null-termination | encoder.encode(str + "\0") -> buf last byte === 0 | VERIFIED (spot-check) |
| makeDefaultOptions struct layout | DataView.setInt32(0,0,true) + setInt32(4,1,true) -> 8-byte struct | VERIFIED (spot-check: getInt32(0,true)===0, getInt32(4,true)===1) |
| allocNativeStringArray composition | allocNativeString* -> allocNativePtrTable -> { table, keepalive } | VERIFIED (spot-check: 24-byte table, 3 keepalives all null-terminated) |

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| All 18 exports are functions | deno eval import check | "All 18 exports are functions" | PASS |
| allocPtrOut returns { ptr, buf } with 8-byte Uint8Array | deno eval | "allocPtrOut: OK" | PASS |
| readPtrOut on zero buffer returns null | deno eval | "readPtrOut(zero): null OK" | PASS |
| readUint64Out on zero buffer returns 0 | deno eval | "readUint64Out(zero): 0 OK" | PASS |
| toCString appends null terminator | deno eval | "toCString: OK, null-terminated" | PASS |
| makeDefaultOptions: read_only=0, console_level=1 | deno eval | "makeDefaultOptions: OK, read_only=0 console_level=1" | PASS |
| allocNativeInt64 encodes values correctly (little-endian) | deno eval | "allocNativeInt64: OK" | PASS |
| allocNativeFloat64 encodes values correctly (little-endian) | deno eval | "allocNativeFloat64: OK" | PASS |
| allocNativeString appends null terminator | deno eval | "allocNativeString: OK, null-terminated" | PASS |
| allocNativePtrTable empty input | deno eval | "allocNativePtrTable([]): OK" | PASS |
| nativeAddress(null) returns 0n | deno eval | "nativeAddress(null): 0n OK" | PASS |
| 5 stubs throw "Phase 3 scope" error | deno eval | "5 stubs throw Phase 3 error: OK" | PASS |
| allocNativeStringArray composition (3 strings) | deno eval | "table=24 bytes, 3 keepalives, all null-terminated" | PASS |
| decodeStringFromBuf(null ptr) returns "" | deno eval | "empty string OK" | PASS |
| deno check types.ts | deno check | Exit 0, no errors | PASS |
| deno check errors.ts | deno check | Exit 0, no errors | PASS |
| deno check ffi-helpers.ts | deno check | Exit 0, no errors | PASS |

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|-------------|-------------|--------|----------|
| MARSH-01 | 02-01-PLAN.md | Pointer out-parameters work using BigInt (Deno.UnsafePointer) instead of koffi Buffer | SATISFIED | allocPtrOut(Uint8Array(8) + UnsafePointer.of), readPtrOut(DataView.getBigUint64 + UnsafePointer.create), 0n null-check present. Spot-check: round-trip confirmed. |
| MARSH-03 | 02-01-PLAN.md | String marshaling uses TextEncoder/TextDecoder for C string encoding/decoding | SATISFIED | toCString + allocNativeString use encoder.encode(str+"\0"). decodeStringFromBuf + errors.ts use UnsafePointerView.getCString(). Spot-checks: null-termination verified. |
| MARSH-04 | 02-01-PLAN.md | Native memory allocation (int64, float64, string, pointer tables) works using Deno FFI primitives | SATISFIED | allocNativeInt64 (DataView.setBigInt64, LE), allocNativeFloat64 (DataView.setFloat64, LE), allocNativeString (TextEncoder), allocNativePtrTable (DataView.setBigUint64, LE), allocNativeStringArray (composes both). All return Allocation { ptr, buf }. Spot-checks passed. |

**Orphaned requirements check:** REQUIREMENTS.md maps only MARSH-01, MARSH-03, MARSH-04 to Phase 2. No orphaned requirements. MARSH-02 is correctly assigned to Phase 3.

### Anti-Patterns Found

No anti-patterns found in the three phase-modified files.

Scan results:
- TODO/FIXME/placeholder comments: None
- Empty implementations (return null/[]/{}): None
- Hardcoded empty returns in data paths: None
- koffi imports: None
- Buffer references: None
- node: protocol imports: None

Known planned stubs (NOT anti-patterns — intentional Phase 3 scope per plan):

| File | Lines | Stub | Reason |
|------|-------|------|--------|
| bindings/js/src/ffi-helpers.ts | 54 | decodeInt64Array throws "Not implemented: Phase 3 scope" | Phase 3 scope per plan (MARSH-02) |
| bindings/js/src/ffi-helpers.ts | 59 | decodeFloat64Array throws "Not implemented: Phase 3 scope" | Phase 3 scope per plan (MARSH-02) |
| bindings/js/src/ffi-helpers.ts | 64 | decodeUint64Array throws "Not implemented: Phase 3 scope" | Phase 3 scope per plan (MARSH-02) |
| bindings/js/src/ffi-helpers.ts | 69 | decodePtrArray throws "Not implemented: Phase 3 scope" | Phase 3 scope per plan (MARSH-02) |
| bindings/js/src/ffi-helpers.ts | 74 | decodeStringArray throws "Not implemented: Phase 3 scope" | Phase 3 scope per plan (MARSH-02) |

These 5 stubs are exported by design to prevent import errors in domain modules (database.ts, create.ts, read.ts, query.ts, time-series.ts, csv.ts) until Phase 3 implements the real logic.

### Phase 1 Regression Check

| Artifact | Check | Status |
|----------|-------|--------|
| `bindings/js/src/loader.ts` | deno check | PASS — no regressions |
| `bindings/js/src/loader.ts` | koffi/node: imports | CLEAN — none found |

### Human Verification Required

None. All success criteria were verified programmatically via behavioral spot-checks and `deno check`.

### Gaps Summary

No gaps. All 4 roadmap success criteria verified, all 3 requirements satisfied, all key links wired, behavioral spot-checks passed on 14 distinct behaviors. The 5 array decoder stubs are intentional Phase 3 scope and are confirmed as deferred by Phase 3's first success criterion.

---

_Verified: 2026-04-17T22:30:00Z_
_Verifier: Claude (gsd-verifier)_
