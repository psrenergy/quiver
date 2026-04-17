# Phase 2: Pointer & String Marshaling - Context

**Gathered:** 2026-04-17
**Status:** Ready for planning

<domain>
## Phase Boundary

Rewrite all pointer out-parameter, string marshaling, and native memory allocation helpers in `bindings/js/src/ffi-helpers.ts` from koffi to Deno FFI primitives. Functions in scope: `makeDefaultOptions`, `allocPtrOut`, `readPtrOut`, `allocUint64Out`, `readUint64Out`, `decodeStringFromBuf`, `toCString`, `allocNativeInt64`, `allocNativeFloat64`, `allocNativePtrTable`, `allocNativeString`, `allocNativeStringArray`, `nativeAddress`. Array decoders (`decodeInt64Array`, `decodeFloat64Array`, `decodeUint64Array`, `decodePtrArray`, `decodeStringArray`) are Phase 3 scope. No domain module changes (Phase 3-4).

</domain>

<decisions>
## Implementation Decisions

### Native Memory Allocation
- **D-01:** All native memory allocation uses `Uint8Array` + `Deno.UnsafePointer.of(buffer)` instead of `koffi.alloc`/`koffi.encode`. No C-level malloc/free symbols needed.
- **D-02:** Values are written into Uint8Array backing buffers via `DataView` (setBigInt64, setFloat64, setInt32, etc.) with little-endian byte order.

### Return Types & Keepalive
- **D-03:** All alloc functions return `{ ptr: Deno.PointerValue; buf: Uint8Array }` so callers can hold the buffer reference to prevent GC. This extends the existing `{ table, keepalive }` pattern from `allocNativeStringArray` to all allocation functions.
- **D-04:** `toCString` follows the same pattern — returns `{ ptr, buf }` instead of a bare Buffer.

### Pointer Type
- **D-05:** `NativePointer` becomes a type alias for `Deno.PointerValue` (which is `Deno.PointerObject | null`). Simple alias, no branded wrapper. Defined in `types.ts` or `ffi-helpers.ts`.

### GC & Lifetime Safety
- **D-06:** Scope-based keepalive — callers hold buffer references in local variables during C API calls. No centralized allocation tracker. When scope exits, buffers become GC-eligible. This matches how domain modules already use these functions (allocate, call C, return).

### Struct Building
- **D-07:** `makeDefaultOptions` uses `Uint8Array` + `DataView` for the 8-byte options struct, consistent with all other allocation functions. Returns `{ ptr, buf }`. No dedicated struct-builder utility (Phase 3's `CSVOptions` can reuse the same inline pattern).

### String Encoding
- **D-08:** String encoding uses `TextEncoder` to produce UTF-8 bytes, manually appending a null terminator byte. String decoding from C pointers uses `Deno.UnsafePointerView.getCString()`.

### Claude's Discretion
- Exact function signature details for `allocPtrOut`/`readPtrOut` (whether they use Uint8Array + DataView or BigUint64Array)
- Whether `nativeAddress` is still needed or can be replaced by direct pointer operations
- Internal naming of the `Allocation` return type (if a named type is warranted)
- Whether to keep `allocUint64Out`/`readUint64Out` as separate functions or fold into a generic pattern

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Primary Target
- `bindings/js/src/ffi-helpers.ts` -- The file being rewritten. All 13 in-scope functions live here.

### Phase 1 Output (dependency)
- `bindings/js/src/loader.ts` -- Deno.dlopen symbol definitions. Phase 2 functions use the symbols object.
- `.planning/phases/01-ffi-foundation-library-loading/01-CONTEXT.md` -- Phase 1 decisions D-06 (type constants) and D-07 (string-as-pointer mapping)

### Type Definitions
- `bindings/js/src/types.ts` -- Pure type definitions. NativePointer alias target location.

### C API Headers (struct layouts, function signatures)
- `include/quiver/c/options.h` -- quiver_database_options_t struct layout (read_only: int, console_level: int)
- `include/quiver/c/database.h` -- C API function signatures that consume pointers/strings

### Domain Modules (consumers of ffi-helpers)
- `bindings/js/src/database.ts` -- Uses makeDefaultOptions, allocPtrOut, readPtrOut
- `bindings/js/src/create.ts` -- Uses toCString, allocNativeString, allocNativeStringArray
- `bindings/js/src/read.ts` -- Uses decodeStringFromBuf, decodeInt64Array, etc.
- `bindings/js/src/query.ts` -- Uses allocNativeInt64, allocNativeFloat64, allocNativePtrTable

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `bindings/js/src/types.ts` -- Pure TypeScript types, no koffi dependency. NativePointer alias can be updated here.
- `bindings/js/src/errors.ts` -- QuiverError class, no koffi dependency.
- `allocNativeStringArray` already returns `{ table, keepalive }` -- existing precedent for composite return pattern (D-03 extends this).

### Established Patterns
- **Type shorthand:** Phase 1 defined named constants (P, I32, I64, etc.) in loader.ts for Deno FFI types
- **Singleton caching:** `_symbols` caches loaded library -- ffi-helpers imports symbols from loader
- **Little-endian:** All Buffer writes in current code use LE byte order (matches x86/ARM targets)

### Integration Points
- `loadLibrary()` and `getSymbols()` from loader.ts -- ffi-helpers imports the symbols object
- All domain modules import from ffi-helpers -- function signatures must remain stable (names preserved, return types change from Buffer/NativePointer to { ptr, buf }/Deno.PointerValue)
- Phase 3 will rewrite the 5 array decoder functions left as stubs or unchanged in this phase

</code_context>

<specifics>
## Specific Ideas

No specific requirements -- user confirmed the recommended allocation approach (Uint8Array + UnsafePointer.of) and deferred remaining decisions to Claude's judgment.

</specifics>

<deferred>
## Deferred Ideas

None -- discussion stayed within phase scope.

</deferred>

---

*Phase: 02-pointer-string-marshaling*
*Context gathered: 2026-04-17*
